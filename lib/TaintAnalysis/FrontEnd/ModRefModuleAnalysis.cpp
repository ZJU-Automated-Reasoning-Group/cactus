/**
 * @file ModRefModuleAnalysis.cpp
 * @brief Implementation of the Mod-Ref analysis for LLVM modules
 *
 * This file provides a module-level analysis that tracks which memory locations
 * and values are modified (mod) or referenced (ref) by each function. The analysis
 * creates summaries for each function and propagates them through the call graph.
 */
#include "Annotation/ModRef/ExternalModRefTable.h"
#include "PointerAnalysis/Analysis/SemiSparsePointerAnalysis.h"
#include "PointerAnalysis/MemoryModel/MemoryObject.h"
#include "TaintAnalysis/FrontEnd/ModRefModuleAnalysis.h"
#include "Util/DataStructure/FIFOWorkList.h"
#include "Util/DataStructure/VectorSet.h"

#include <llvm/IR/InstVisitor.h>
#include <llvm/Support/raw_ostream.h>

using namespace annotation;
using namespace llvm;
using namespace tpa;

namespace taint
{

/**
 * Reverse call map that maps function definitions to their call sites.
 * Used for propagating summaries backwards from callees to callers.
 */
using RevCallMapType = std::unordered_map<const Function*, util::VectorSet<const Instruction*>>;

namespace
{

/**
 * Determines if a memory object is allocated on the local stack of a function.
 *
 * Local stack allocations don't need to be tracked in mod-ref summaries because
 * they're not visible outside the function.
 *
 * @param loc The memory object to check
 * @param f The function context in which the object is being accessed
 * @return true if the location is a local stack allocation or special object
 */
inline bool isLocalStackLocation(const MemoryObject* loc, const Function* f)
{
	auto const& allocSite = loc->getAllocSite();

	switch (allocSite.getAllocType())
	{
		// NullObj and UniversalObj are counted as local stack location because we don't want them to appear in the mod-ref summaries
		case AllocSiteTag::Null:
		case AllocSiteTag::Universal:
			return true;
		case AllocSiteTag::Global:
		case AllocSiteTag::Function:
		case AllocSiteTag::Heap:
			return false;
		case AllocSiteTag::Stack:
		{
			if (auto allocInst = dyn_cast<AllocaInst>(allocSite.getLocalValue()))
				return allocInst->getParent()->getParent() == f;
			else
				return false;
		}
	}
}

/**
 * Updates a caller's mod-ref summary by incorporating effects from a callee's summary.
 *
 * Propagates value reads and memory reads/writes from the callee to the caller,
 * filtering out effects on local stack allocations.
 *
 * @param callerSummary The summary to update
 * @param calleeSummary The summary containing effects to propagate
 * @param caller The caller function
 * @return true if the caller's summary was changed
 */
bool updateSummary(ModRefFunctionSummary& callerSummary, const ModRefFunctionSummary& calleeSummary, const Function* caller)
{
	bool changed = false;

	for (auto value: calleeSummary.value_reads())
		changed |= callerSummary.addValueRead(value);
	for (auto loc: calleeSummary.mem_reads())
	{
		if (!isLocalStackLocation(loc, caller))
			changed |= callerSummary.addMemoryRead(loc);
	}
	for (auto loc: calleeSummary.mem_writes())
	{
		if (!isLocalStackLocation(loc, caller))
			changed |= callerSummary.addMemoryWrite(loc);
	}

	return changed;
}

/**
 * Updates the reverse call graph by identifying all call sites within a function.
 *
 * For each call site, adds an entry mapping the callee to the instruction that calls it.
 *
 * @param revCallGraph The reverse call graph to update
 * @param f The function containing call sites to analyze
 * @param ptrAnalysis The pointer analysis used to resolve indirect calls
 */
void updateRevCallGraph(RevCallMapType& revCallGraph, const Function& f, const SemiSparsePointerAnalysis& ptrAnalysis)
{
	for (auto const& bb: f)
	{
		for (auto const& inst: bb)
		{
			ImmutableCallSite cs(&inst);
			if (!cs)
				continue;

			auto callTgts = ptrAnalysis.getCallees(cs);
			for (auto callee: callTgts)
				revCallGraph[callee].insert(cs.getInstruction());
		}
	}
}

/**
 * InstVisitor that collects mod-ref information from individual instructions.
 *
 * Visits loads, stores, and other instructions to identify values and memory locations
 * that are read or written by a function.
 */
class SummaryInstVisitor: public InstVisitor<SummaryInstVisitor>
{
private:
	const SemiSparsePointerAnalysis& ptrAnalysis;
	ModRefFunctionSummary& summary;
public:
	SummaryInstVisitor(const SemiSparsePointerAnalysis& p, ModRefFunctionSummary& s): ptrAnalysis(p), summary(s) {}

	/**
	 * Processes a load instruction, recording the values and memory locations it reads.
	 *
	 * @param loadInst The load instruction to process
	 */
	void visitLoadInst(LoadInst& loadInst)
	{
		auto ptrOp = loadInst.getPointerOperand();
		if (isa<GlobalValue>(ptrOp))
			summary.addValueRead(ptrOp->stripPointerCasts());
		auto pSet = ptrAnalysis.getPtsSet(ptrOp);
		for (auto obj: pSet)
		{
			if (!isLocalStackLocation(obj, loadInst.getParent()->getParent()))
				summary.addMemoryRead(obj);
		}
	}

	/**
	 * Processes a store instruction, recording the values it reads and memory locations it writes.
	 *
	 * @param storeInst The store instruction to process
	 */
	void visitStoreInst(StoreInst& storeInst)
	{
		auto storeSrc = storeInst.getValueOperand();
		auto storeDst = storeInst.getPointerOperand();

		if (isa<GlobalValue>(storeSrc))
			summary.addValueRead(storeSrc->stripPointerCasts());
		if (isa<GlobalValue>(storeDst))
			summary.addValueRead(storeDst->stripPointerCasts());

		auto pSet = ptrAnalysis.getPtsSet(storeDst);
		for (auto obj: pSet)
		{
			if (!isLocalStackLocation(obj, storeInst.getParent()->getParent()))
				summary.addMemoryWrite(obj);
		}
	}

	/**
	 * Processes any other instruction, recording global values it reads.
	 *
	 * @param inst The instruction to process
	 */
	void visitInstruction(Instruction& inst)
	{
		for (auto& use: inst.operands())
		{
			auto value = use.get();
			if (isa<GlobalValue>(value))
				summary.addValueRead(value->stripPointerCasts());
		}
	}
};

/**
 * Adds memory write effects to a function summary based on an external function's modification effects.
 *
 * @param v The value (typically a function argument) through which memory is accessed
 * @param summary The summary to update
 * @param caller The calling function
 * @param ptrAnalysis The pointer analysis used to resolve memory locations
 * @param isReachableMemory Whether the effect applies to all reachable memory (true) or just direct memory (false)
 * @return true if the summary was changed
 */
bool addExternalMemoryWrite(const Value* v, ModRefFunctionSummary& summary, const Function* caller, const SemiSparsePointerAnalysis& ptrAnalysis, bool isReachableMemory)
{
	auto changed = false;
	auto pSet = ptrAnalysis.getPtsSet(v);
	for (auto loc: pSet)
	{
		if (isLocalStackLocation(loc, caller))
			continue;
		if (isReachableMemory)
		{
			for (auto oLoc: ptrAnalysis.getMemoryManager().getReachableMemoryObjects(loc))
				changed |= summary.addMemoryWrite(oLoc);		
		}
		else
		{
			changed |= summary.addMemoryWrite(loc);
		}
	}
	return changed;
}

/**
 * Updates a function summary based on a modification effect from an external function call.
 *
 * @param inst The call instruction
 * @param summary The summary to update
 * @param ptrAnalysis The pointer analysis used to resolve memory locations
 * @param modEffect The modification effect to apply
 * @return true if the summary was changed
 */
bool updateSummaryForModEffect(const Instruction* inst, ModRefFunctionSummary& summary, const SemiSparsePointerAnalysis& ptrAnalysis, const ModRefEffect& modEffect)
{
	assert(modEffect.isModEffect());

	ImmutableCallSite cs(inst);
	assert(cs);
	auto caller = inst->getParent()->getParent();

	auto const& pos = modEffect.getPosition();
	if (pos.isReturnPosition())
		return addExternalMemoryWrite(inst, summary, caller, ptrAnalysis, modEffect.onReachableMemory());
	else
	{
		auto const& argPos = pos.getAsArgPosition();
		unsigned idx = argPos.getArgIndex();

		if (idx >= cs.arg_size()) {
			errs() << "Warning: Argument index " << idx << " out of range (max " << cs.arg_size() 
			       << ") in call to " << cs.getCalledFunction()->getName() << ". Skipping effect.\n";
			return false;
		}

		if (!argPos.isAfterArgPosition())
			return addExternalMemoryWrite(cs.getArgument(idx)->stripPointerCasts(), summary, caller, ptrAnalysis, modEffect.onReachableMemory());
		else
		{
			bool changed = false;
			for (auto i = idx, e = cs.arg_size(); i < e; ++i)
				changed |= addExternalMemoryWrite(cs.getArgument(i)->stripPointerCasts(), summary, caller, ptrAnalysis, modEffect.onReachableMemory());
			return changed;
		}
	}
}

/**
 * Adds memory read effects to a function summary based on an external function's reference effects.
 *
 * @param v The value (typically a function argument) through which memory is accessed
 * @param summary The summary to update
 * @param caller The calling function
 * @param ptrAnalysis The pointer analysis used to resolve memory locations
 * @param isReachableMemory Whether the effect applies to all reachable memory (true) or just direct memory (false)
 * @return true if the summary was changed
 */
bool addExternalMemoryRead(const Value* v, ModRefFunctionSummary& summary, const Function* caller, const SemiSparsePointerAnalysis& ptrAnalysis, bool isReachableMemory)
{
	bool changed = false;
	auto pSet = ptrAnalysis.getPtsSet(v);
	for (auto loc: pSet)
	{
		if (isLocalStackLocation(loc, caller))
			continue;
		if (isReachableMemory)
		{
			for (auto oLoc: ptrAnalysis.getMemoryManager().getReachableMemoryObjects(loc))
				changed |= summary.addMemoryRead(oLoc);		
		}
		else
		{
			changed |= summary.addMemoryRead(loc);
		}
	}
	return changed;
}

/**
 * Updates a function summary based on a reference effect from an external function call.
 *
 * @param inst The call instruction
 * @param summary The summary to update
 * @param ptrAnalysis The pointer analysis used to resolve memory locations
 * @param refEffect The reference effect to apply
 * @return true if the summary was changed
 */
bool updateSummaryForRefEffect(const Instruction* inst, ModRefFunctionSummary& summary, const SemiSparsePointerAnalysis& ptrAnalysis, const ModRefEffect& refEffect)
{
	assert(refEffect.isRefEffect());

	ImmutableCallSite cs(inst);
	assert(cs);
	auto caller = inst->getParent()->getParent();

	auto const& pos = refEffect.getPosition();
	assert(!pos.isReturnPosition() && "It doesn't make any sense to ref a return position!");
	
	auto const& argPos = pos.getAsArgPosition();
	unsigned idx = argPos.getArgIndex();

	// Add a bounds check to avoid "Argument # out of range!" assertion
	if (idx >= cs.arg_size()) {
		errs() << "Warning: Argument index " << idx << " out of range (max " << cs.arg_size() 
		       << ") in call to " << cs.getCalledFunction()->getName() << ". Skipping effect.\n";
		return false;
	}

	if (!argPos.isAfterArgPosition())
		return addExternalMemoryRead(cs.getArgument(idx)->stripPointerCasts(), summary, caller, ptrAnalysis, refEffect.onReachableMemory());
	else
	{
		bool changed = false;
		for (auto i = idx, e = cs.arg_size(); i < e; ++i)
		{
			changed |= addExternalMemoryRead(cs.getArgument(i)->stripPointerCasts(), summary, caller, ptrAnalysis, refEffect.onReachableMemory());
		}
		return changed;
	}
}

/**
 * Updates a function summary based on a call to an external function with mod-ref effects.
 *
 * @param inst The call instruction
 * @param f The external function being called
 * @param summary The summary to update
 * @param ptrAnalysis The pointer analysis used to resolve memory locations
 * @param modRefTable The table of mod-ref effects for external functions
 * @return true if the summary was changed
 */
bool updateSummaryForExternalCall(const Instruction* inst, const Function* f, ModRefFunctionSummary& summary, const SemiSparsePointerAnalysis& ptrAnalysis, const ExternalModRefTable& modRefTable)
{
	auto modRefSummary = modRefTable.lookup(f->getName());
	if (modRefSummary == nullptr)
	{
		errs() << "Warning: Missing entry in ModRefTable: " << f->getName() << "\n";
		errs() << "Treating as no effect. Add annotation to modref config for more precise analysis.\n";
		return false;
	}

	bool changed = false;
	for (auto const& effect: *modRefSummary)
	{
		if (effect.isModEffect())
			changed |= updateSummaryForModEffect(inst, summary, ptrAnalysis, effect);
		else
			changed |= updateSummaryForRefEffect(inst, summary, ptrAnalysis, effect);
	}
	return changed;
}

/**
 * Propagates mod-ref summaries through the call graph until a fixed point is reached.
 *
 * Uses a work list algorithm to propagate effects from callees back to their callers.
 * When a function's summary changes, all of its callers are re-analyzed.
 *
 * @param moduleSummary The module summary to populate
 * @param revCallGraph The reverse call graph mapping functions to their call sites
 * @param ptrAnalysis The pointer analysis used to resolve memory locations
 * @param modRefTable The table of mod-ref effects for external functions
 */
void propagateSummary(ModRefModuleSummary& moduleSummary, const RevCallMapType& revCallGraph, const SemiSparsePointerAnalysis& ptrAnalysis, const ExternalModRefTable& modRefTable)
{
	auto workList = util::FIFOWorkList<const Function*>();

	// Enqueue all callees at the beginning
	for (auto const& mapping: revCallGraph)
		workList.enqueue(mapping.first);

	// Each function "push"es its info back into its callers
	while (!workList.empty())
	{
		auto f = workList.dequeue();

		auto itr = revCallGraph.find(f);
		if (itr == revCallGraph.end())
			continue;
		auto& callerSet = itr->second;

		if (f->isDeclaration())
		{
			for (auto callSite: callerSet)
			{
				auto caller = callSite->getParent()->getParent();
				auto& callerSummary = moduleSummary.getSummary(caller);
				if (updateSummaryForExternalCall(callSite, f, callerSummary, ptrAnalysis, modRefTable))
					workList.enqueue(caller);
			}
		}
		else
		{
			auto& calleeSummary = moduleSummary.getSummary(f);
			for (auto callSite: callerSet)
			{
				auto caller = callSite->getParent()->getParent();
				auto& callerSummary = moduleSummary.getSummary(caller);
				if (updateSummary(callerSummary, calleeSummary, caller))
					workList.enqueue(caller);
			}

		}
	}
}

}	// end of anonymous namespace

/**
 * Collects mod-ref information for a single function by visiting its instructions.
 *
 * @param f The function to analyze
 * @param summary The summary to populate with mod-ref information
 */
void ModRefModuleAnalysis::collectProcedureSummary(const Function& f, ModRefFunctionSummary& summary)
{
	SummaryInstVisitor summaryVisitor(ptrAnalysis, summary);
	summaryVisitor.visit(const_cast<Function&>(f));
}

/**
 * Runs the mod-ref analysis on an entire module.
 *
 * First collects summaries for individual functions, then propagates the effects
 * through the call graph until a fixed point is reached.
 *
 * @param module The LLVM module to analyze
 * @return A module summary containing mod-ref information for all functions
 */
ModRefModuleSummary ModRefModuleAnalysis::runOnModule(const Module& module)
{
	ModRefModuleSummary moduleSummary;
	RevCallMapType revCallGraph;

	for (auto const& f: module)
	{
		if (f.isDeclaration())
			continue;
		collectProcedureSummary(f, moduleSummary.getSummary(&f));
		updateRevCallGraph(revCallGraph, f, ptrAnalysis);
	}

	propagateSummary(moduleSummary, revCallGraph, ptrAnalysis, modRefTable);

	return moduleSummary;
}

}