#include "PointerAnalysis/Analysis/SemiSparsePointerAnalysis.h"
#include "TaintAnalysis/Precision/CallTracker.h"
#include "TaintAnalysis/Precision/LocalTracker.h"
#include "TaintAnalysis/Precision/ReturnTracker.h"
#include "TaintAnalysis/Precision/TrackerGlobalState.h"
#include "TaintAnalysis/Precision/TrackerTransferFunction.h"
#include "TaintAnalysis/Precision/VectorTransform.h"
#include "TaintAnalysis/Support/ProgramPoint.h"
#include "TaintAnalysis/Support/TaintEnv.h"
#include "TaintAnalysis/Support/TaintMemo.h"
#include "Util/IO/TaintAnalysis/Printer.h"

#include <llvm/IR/Instructions.h>
#include <llvm/Support/raw_ostream.h>

using namespace llvm;
using namespace tpa;
using namespace util::io;

namespace taint
{


template <typename ContainerType, typename LookupFunction, typename Action>
static void propagateImprecision(ContainerType& container, LookupFunction&& lookup, Action&& action, size_t sizeHint = 0)
{
	using ElemType = typename ContainerType::value_type;

	bool hasTaint = false, hasUntaint = false;
	std::vector<ElemType> candidates;
	if (sizeHint != 0)
		candidates.reserve(sizeHint);

	for (auto elem: container)
	{
		TaintLattice taintVal = lookup(elem);
		switch (taintVal)
		{
			case TaintLattice::Tainted:
				hasTaint = true;
				break;
			case TaintLattice::Untainted:
				hasUntaint = true;
				break;
			case TaintLattice::Either:
				candidates.push_back(elem);
				break;
			case TaintLattice::Unknown:
				assert(false && "Unknown lattice found here?");
				break;
		}
	}

	// If we have both Taint and Untaint on the rhs, it's not useful to track any other operands even if they are imprecise, since the imprecision comes from path sensitivity
	if (!(hasTaint && hasUntaint))
		std::for_each(candidates.begin(), candidates.end(), std::forward<Action>(action));
}

void TrackerTransferFunction::evalEntryInst(const ProgramPoint& pp)
{
	auto fc = tpa::FunctionContext(pp.getContext(), pp.getDefUseInstruction()->getFunction());
	auto callers = trackerState.getCallGraph().getCallers(fc);

	auto callerVec = vectorTransform(callers, [] (auto const& x) { return x; });
	CallTracker(trackerState, workList).trackCall(pp, callerVec);
}

TrackerTransferFunction::ValueSet TrackerTransferFunction::evalAllOperands(const ProgramPoint& pp)
{
	ValueSet valueSet;

	// Obtain all values
	auto ctx = pp.getContext();
	auto inst = pp.getDefUseInstruction()->getInstruction();
	std::vector<const Value*> values;
	values.reserve(inst->getNumOperands());
	for (auto const& use: inst->operands())
		values.push_back(use.get());

	propagateImprecision(
		values,
		[this, ctx] (const Value* val)
		{
			return trackerState.getEnv().lookup(TaintValue(ctx, val));
		},
		[&valueSet] (const Value* val)
		{
			if (!isa<GlobalValue>(val))
				valueSet.insert(val);
		}
	);

	return valueSet;
}

TrackerTransferFunction::MemorySet TrackerTransferFunction::evalPtsSet(const ProgramPoint& pp, tpa::PtsSet pSet, const TaintStore& store)
{
	MemorySet memSet;

	// Remove uLoc and nLoc
	std::vector<const MemoryObject*> validObjs;
	validObjs.reserve(pSet.size());
	for (auto obj: pSet)
		if (!obj->isSpecialObject())
			validObjs.push_back(obj);

	propagateImprecision(
		validObjs,
		[&store] (const MemoryObject* obj)
		{
			return store.lookup(obj);
		},
		[&memSet] (const MemoryObject* obj)
		{
			memSet.insert(obj);
		}
	);

	return memSet;
}

TrackerTransferFunction::ValueSet TrackerTransferFunction::evalStore(const ProgramPoint& pp)
{
	ValueSet valueSet;

	auto storeInst = cast<StoreInst>(pp.getDefUseInstruction()->getInstruction());
	auto valOp = storeInst->getValueOperand();
	auto storeVal = trackerState.getEnv().lookup(TaintValue(pp.getContext(), valOp));

	switch (storeVal)
	{
		case TaintLattice::Tainted:
		case TaintLattice::Untainted:
			// If the value is precise and this store inst is still enqueued, it means that the imprecision comes from weak update, about which we cannot do anything
			break;
		case TaintLattice::Either:
			valueSet.insert(valOp);
			break;
		case TaintLattice::Unknown:
			assert(false && "TrackingTransferFunction find Unknown operand?");
			break;
	}

	return valueSet;
}

TrackerTransferFunction::MemorySet TrackerTransferFunction::evalLoad(const ProgramPoint& pp, const TaintStore& store)
{
	auto loadInst = cast<LoadInst>(pp.getDefUseInstruction()->getInstruction());

	auto ptrOp = loadInst->getPointerOperand();
	auto ptsSet = trackerState.getPointerAnalysis().getPtsSet(pp.getContext(), ptrOp);
	assert(!ptsSet.empty());

	return evalPtsSet(pp, ptsSet, store);
}

void TrackerTransferFunction::evalExternalCallInst(const ProgramPoint&, const llvm::Function*)
{
	llvm_unreachable("Not implemented yet");
}

void TrackerTransferFunction::evalCallInst(const ProgramPoint& pp)
{
	auto callees = trackerState.getCallGraph().getCallees(pp);

	std::vector<tpa::FunctionContext> nonExtCallees;
	for (auto callTgt: callees)
	{
		auto callee = callTgt.getFunction();
		if (callee->isDeclaration())
			evalExternalCallInst(pp, callee);
		else
			nonExtCallees.push_back(callTgt);
	}

	ReturnTracker(trackerState, workList).trackReturn(pp, nonExtCallees);
}


void TrackerTransferFunction::evalInst(const ProgramPoint& pp, const TaintStore* store)
{
	auto inst = pp.getDefUseInstruction()->getInstruction();
	assert(inst != nullptr);

	switch (inst->getOpcode())
	{
		case Instruction::Alloca:
		case Instruction::Br:
			// These instructions never produce imprecise result
			break;
		case Instruction::Trunc:
		case Instruction::ZExt:
		case Instruction::SExt:
		case Instruction::FPTrunc:
		case Instruction::FPExt:
		case Instruction::FPToUI:
		case Instruction::FPToSI:
		case Instruction::UIToFP:
		case Instruction::SIToFP:
		case Instruction::IntToPtr:
		case Instruction::PtrToInt:
		case Instruction::BitCast:
		case Instruction::AddrSpaceCast:
		case Instruction::ExtractElement:
		case Instruction::ExtractValue:
		// Binary operators
		case Instruction::And:
		case Instruction::Or:
		case Instruction::Xor:
		case Instruction::Shl:
		case Instruction::LShr:
		case Instruction::AShr:
		case Instruction::Add:
		case Instruction::FAdd:
		case Instruction::Sub:
		case Instruction::FSub:
		case Instruction::Mul:
		case Instruction::FMul:
		case Instruction::UDiv:
		case Instruction::SDiv:
		case Instruction::FDiv:
		case Instruction::URem:
		case Instruction::SRem:
		case Instruction::FRem:
		case Instruction::ICmp:
		case Instruction::FCmp:
		case Instruction::ShuffleVector:
		case Instruction::InsertElement:
		case Instruction::InsertValue:
		// Ternary operators
		case Instruction::Select:
		// N-ary operators
		case Instruction::GetElementPtr:
		case Instruction::PHI:
		{
			auto valueSet = evalAllOperands(pp);
			LocalTracker(workList).trackValue(pp, valueSet);
			break;
		}
		case Instruction::Store:
		{
			auto valueSet = evalStore(pp);
			LocalTracker(workList).trackValue(pp, valueSet);
			break;
		}
		case Instruction::Load:
		{
			assert(store != nullptr);
			auto memSet = evalLoad(pp, *store);
			LocalTracker(workList).trackMemory(pp, memSet);
			break;
		}
		case Instruction::Invoke:
		case Instruction::Call:
		{
			evalCallInst(pp);
			break;
		}
		case Instruction::Ret:
		{
			auto retVal = cast<ReturnInst>(inst)->getReturnValue();
			if (retVal != nullptr)
			{
				auto valueSet = evalAllOperands(pp);
				LocalTracker(workList).trackValue(pp, valueSet);
			}
			break;
		}
		case Instruction::Switch:
		case Instruction::IndirectBr:
		case Instruction::AtomicCmpXchg:
		case Instruction::AtomicRMW:
		case Instruction::Fence:
		case Instruction::VAArg:
		case Instruction::LandingPad:
		case Instruction::Resume:
		case Instruction::Unreachable:
		{
			errs() << *inst << "\n";
			llvm_unreachable("instruction not handled");
		}
	}
}

void TrackerTransferFunction::eval(const ProgramPoint& pp)
{
	if (!trackerState.insertVisitedLocation(pp))
		return;

	errs() << "tracking " << pp << "\n";

	auto duInst = pp.getDefUseInstruction();
	if (duInst->isEntryInstruction())
		evalEntryInst(pp);
	else
	{
		//errs() << "tracking " << *duInst->getInstruction() << "\n";
		auto store = trackerState.getMemo().lookup(pp);
		evalInst(pp, store);
	}
}

}