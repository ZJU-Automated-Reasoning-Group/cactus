/**
 * @file ReachingDefModuleAnalysis.cpp
 * @brief Implementation of reaching definition analysis for LLVM modules
 *
 * This file provides a reaching definition analysis that tracks which instructions 
 * may have last modified each memory location at each program point.
 * The analysis builds on mod-ref information and propagates definitions through the control flow graph.
 */
#include "Annotation/ModRef/ExternalModRefTable.h"
#include "PointerAnalysis/Analysis/SemiSparsePointerAnalysis.h"
#include "TaintAnalysis/FrontEnd/ModRefModuleSummary.h"
#include "TaintAnalysis/FrontEnd/ReachingDefModuleAnalysis.h"
#include "Util/DataStructure/FIFOWorkList.h"

#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/InstVisitor.h>
#include <llvm/Support/raw_ostream.h>

using namespace annotation;
using namespace llvm;
using namespace tpa;

namespace taint
{

namespace
{

/**
 * Gets the instruction that follows the given instruction in its basic block.
 *
 * @param inst The instruction whose successor to find
 * @return The next instruction in the basic block
 */
const Instruction* getNextInstruction(const Instruction* inst)
{
	BasicBlock::const_iterator itr = inst;
	++itr;
	assert(itr != inst->getParent()->end());
	return itr;
}

/**
 * Visitor class that computes reaching definitions by evaluating instructions.
 *
 * This visitor analyzes each instruction to identify memory locations that it
 * may define (modify), and updates the reaching definition store accordingly.
 */
class EvalVisitor: public InstVisitor<EvalVisitor>
{
private:
	const SemiSparsePointerAnalysis& ptrAnalysis;  ///< Pointer analysis used to resolve memory locations
	const ModRefModuleSummary& summaryMap;         ///< Summary information about memory effects
	const ExternalModRefTable& modRefTable;        ///< Information about external function effects

	using StoreType = ReachingDefStore<Instruction>;
	StoreType store;  ///< Records the reaching definitions

	/**
	 * Handles a store operation by updating the reaching definitions for 
	 * memory locations pointed to by the pointer operand.
	 *
	 * @param inst The store instruction
	 * @param ptr The pointer operand (destination of the store)
	 */
	void evalStore(Instruction& inst, Value* ptr)
	{
		auto pSet = ptrAnalysis.getPtsSet(ptr);
		bool needWeekUpdate = true;
		if (pSet.size() == 1)
		{
			auto obj = *pSet.begin();
			if (!obj->isSummaryObject())
			{
				store.updateBinding(obj, &inst);
				needWeekUpdate = false;
			}
		}
		if (needWeekUpdate)
		{
			for (auto obj: pSet)
				store.insertBinding(obj, &inst);
		}
	}

	/**
	 * Records a modification to memory locations pointed to by a value.
	 * Used for handling effects of function calls and other operations.
	 *
	 * @param v The value (pointer) being modified
	 * @param inst The instruction performing the modification
	 * @param isReachableMemory Whether the effect applies to all reachable memory (true) or just direct memory (false)
	 */
	void modValue(const Value* v, const Instruction* inst, bool isReachableMemory)
	{
		auto pSet = ptrAnalysis.getPtsSet(v);
		for (auto loc: pSet)
		{
			if (isReachableMemory)
			{
				for (auto oLoc: ptrAnalysis.getMemoryManager().getReachableMemoryObjects(loc))
					store.insertBinding(oLoc, inst);
			}
			else
			{
				store.insertBinding(loc, inst);
			}
			
		}
	}

	/**
	 * Evaluates an external function call to determine its memory effects.
	 * Uses the external mod-ref table to identify modified locations.
	 *
	 * @param cs The call site
	 * @param f The called function
	 */
	void evalExternalCall(CallSite cs, const Function* f)
	{
		auto summary = modRefTable.lookup(f->getName());
		if (summary == nullptr)
		{
			errs() << "Warning: Missing entry in ModRefTable: " << f->getName() << "\n";
			errs() << "Treating as no effect. Add annotation to modref config for more precise analysis.\n";
			return;
		}

		for (auto const& effect: *summary)
		{
			if (effect.isModEffect())
			{
				auto const& pos = effect.getPosition();
				if (pos.isReturnPosition())
				{
					modValue(cs.getInstruction()->stripPointerCasts(), cs.getInstruction(), effect.onReachableMemory());
				}
				else
				{
					auto const& argPos = pos.getAsArgPosition();
					unsigned idx = argPos.getArgIndex();

					if (idx >= cs.arg_size()) {
						errs() << "Warning: Argument index " << idx << " out of range (max " << cs.arg_size() 
							<< ") in call to " << f->getName() << ". Skipping effect.\n";
						continue;
					}

					if (!argPos.isAfterArgPosition())
						modValue(cs.getArgument(idx)->stripPointerCasts(), cs.getInstruction(), effect.onReachableMemory());
					else
					{
						for (auto i = idx, e = cs.arg_size(); i < e; ++i)
							modValue(cs.getArgument(i)->stripPointerCasts(), cs.getInstruction(), effect.onReachableMemory());
					}
				}
			}
		}
	}
public:
	/**
	 * Constructor for the EvalVisitor.
	 *
	 * @param p The pointer analysis
	 * @param sm The module summary containing mod-ref information
	 * @param e The external mod-ref table
	 * @param s The initial reaching definition store
	 */
	EvalVisitor(const SemiSparsePointerAnalysis& p, const ModRefModuleSummary& sm, const ExternalModRefTable& e, const StoreType& s): ptrAnalysis(p), summaryMap(sm), modRefTable(e), store(s) {}
	
	/**
	 * Gets the computed reaching definition store.
	 *
	 * @return The store with updated reaching definitions
	 */
	const StoreType& getStore() const { return store; }

	/**
	 * Default handler for instructions with no special processing.
	 *
	 * @param inst The instruction to process
	 */
	void visitInstruction(Instruction&) {}

	/**
	 * Processes an alloca instruction, which defines a new memory location.
	 *
	 * @param allocInst The alloca instruction
	 */
	void visitAllocaInst(AllocaInst& allocInst)
	{
		auto pSet = ptrAnalysis.getPtsSet(&allocInst);
		for (auto obj: pSet)
			store.insertBinding(obj, &allocInst);
	}

	/**
	 * Processes a store instruction, which modifies memory.
	 *
	 * @param storeInst The store instruction
	 */
	void visitStoreInst(StoreInst& storeInst)
	{
		auto ptr = storeInst.getPointerOperand();
		evalStore(storeInst, ptr);
	}

	/**
	 * Processes a call site, which may have various memory effects.
	 *
	 * @param cs The call site to analyze
	 */
	void visitCallSite(CallSite cs)
	{
		auto callTgts = ptrAnalysis.getCallees(cs);
		for (auto callee: callTgts)
		{
			if (callee->isDeclaration())
			{
				evalExternalCall(cs, callee);
			}
			else
			{
				auto& summary = summaryMap.getSummary(callee);
				for (auto loc: summary.mem_writes())
					store.insertBinding(loc, cs.getInstruction());
			}
		}
	}
};

}	// end of anonymous namespace

/**
 * Computes reaching definitions for a function using a fixed-point algorithm.
 *
 * Initializes the reaching definition map for the entry instruction, then
 * propagates definitions forward through the control flow graph until convergence.
 *
 * @param func The function to analyze
 * @return A map containing reaching definitions for each instruction in the function
 */
ReachingDefMap<Instruction> ReachingDefModuleAnalysis::runOnFunction(const Function& func)
{
	ReachingDefMap<Instruction> rdMap;

	// Initialize the reaching definitions at the function entry
	auto entryInst = func.getEntryBlock().begin();
	auto& summary = summaryMap.getSummary(&func);
	auto& initStore = rdMap.getReachingDefStore(entryInst);
	for (auto loc: summary.mem_reads())
		initStore.insertBinding(loc, nullptr);

	// Fixed-point computation using a work list
	util::FIFOWorkList<const Instruction*> workList;
	workList.enqueue(entryInst);

	while (!workList.empty())
	{
		auto inst = workList.dequeue();
		
		// Compute the reaching definitions after executing this instruction
		EvalVisitor evalVisitor(ptrAnalysis, summaryMap, modRefTable, rdMap.getReachingDefStore(inst));
		evalVisitor.visit(const_cast<Instruction&>(*inst));

		// Propagate to successor instructions
		if (auto termInst = dyn_cast<TerminatorInst>(inst))
		{
			for (auto i = 0u, e = termInst->getNumSuccessors(); i != e; ++i)
			{
				auto succInst = termInst->getSuccessor(i)->begin();
				if (rdMap.update(succInst, evalVisitor.getStore()))
					workList.enqueue(succInst);
			}
		}
		else
		{
			auto nextInst = getNextInstruction(inst);
			if (rdMap.update(nextInst, evalVisitor.getStore()))
				workList.enqueue(nextInst);
		}
	}

	return rdMap;
}

}