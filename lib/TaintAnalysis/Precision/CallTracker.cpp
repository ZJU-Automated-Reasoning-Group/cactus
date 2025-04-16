/**
 * @file CallTracker.cpp
 * @brief Implementation of call site precision tracking
 *
 * This file provides functionality to track imprecision through function calls,
 * identifying how taint propagates from callers to callees and which arguments
 * contribute to imprecision at call sites.
 */
#include "TaintAnalysis/Precision/CallTracker.h"
#include "TaintAnalysis/Precision/LocalTracker.h"
#include "TaintAnalysis/Precision/TrackerGlobalState.h"
#include "TaintAnalysis/Support/TaintEnv.h"
#include "TaintAnalysis/Support/TaintMemo.h"
#include "Util/DataStructure/VectorSet.h"
#include "Util/IO/TaintAnalysis/Printer.h"

#include <llvm/IR/CallSite.h>
#include <llvm/IR/Function.h>
#include <llvm/Support/raw_ostream.h>

using namespace llvm;
using namespace util::io;

namespace taint
{

/**
 * Helper function to get an argument value at a specific position from a call instruction.
 *
 * @param inst The call instruction
 * @param idx The argument index
 * @return The value representing the argument at the given position
 */
static const Value* getArgAtPos(const Instruction* inst, size_t idx)
{
	ImmutableCallSite cs(inst);
	assert(cs);

	return cs.getArgument(idx);
}

/**
 * Gets taint values for arguments at a specific position across multiple call sites.
 *
 * This function collects the taint status of the same argument position across
 * different calls to the same function, which is used to identify sources of imprecision.
 *
 * @param callers Vector of call sites to analyze
 * @param idx The argument index to collect taint values for
 * @return Vector of taint values for the specified argument across call sites
 */
TaintVector CallTracker::getArgTaintValues(const CallerVector& callers, size_t idx)
{
	TaintVector ret;
	ret.reserve(callers.size());

	for (auto const& callsite: callers)
	{
		auto arg = getArgAtPos(callsite.getDefUseInstruction()->getInstruction(), idx);
		auto taint = trackerState.getEnv().lookup(TaintValue(callsite.getContext(), arg));
		if (taint == TaintLattice::Unknown)
			errs() << *callsite.getDefUseInstruction()->getInstruction() << "\n";
		assert(taint != TaintLattice::Unknown);
		ret.push_back(taint);
	}

	return ret;
}

/**
 * Tracks value-based imprecision through function calls.
 * 
 * This function identifies which arguments at call sites cause imprecision
 * in the callee, either due to their own imprecision or due to the merging
 * of different precise values from different call sites.
 *
 * @param pp The program point representing the callee's entry
 * @param callers Vector of call sites to analyze
 */
void CallTracker::trackValue(const ProgramPoint& pp, const CallerVector& callers)
{
	auto callee = pp.getDefUseInstruction()->getFunction();
	auto numArgs = callee->arg_size();
	if (numArgs == 0)
		return;

	std::unordered_map<size_t, util::VectorSet<const llvm::Value*>> trackedValues;
	for (auto i = 0u; i < numArgs; ++i)
	{
		auto callTaints = getArgTaintValues(callers, i);

		// Record which callers demand precision (have divergent taint values)
		auto demandingIndices = getDemandingIndices(callTaints);
		for (auto idx: demandingIndices)
			trackerState.addImprecisionSource(callers[idx]);

		// Record which callers have imprecise values that need tracking
		auto impreciseIndices = getImpreciseIndices(callTaints);
		for (auto idx: impreciseIndices)
		{
			auto arg = getArgAtPos(callers[idx].getDefUseInstruction()->getInstruction(), i);
			trackedValues[idx].insert(arg);
		}
	}

	// Start tracking the identified imprecise values
	LocalTracker localTracker(workList);
	for (auto const& mapping: trackedValues)
		localTracker.trackValue(callers[mapping.first], mapping.second);
}

/**
 * Gets taint values for a memory object across multiple call sites.
 *
 * This function collects the taint status of the same memory object across
 * different calls to the same function, used to identify sources of memory-based imprecision.
 *
 * @param callers Vector of call sites to analyze
 * @param obj The memory object to check
 * @return Vector of taint values for the memory object across call sites
 */
TaintVector CallTracker::getMemoryTaintValues(const CallerVector& callers, const tpa::MemoryObject* obj)
{
	auto retVec = std::vector<TaintLattice>();
	retVec.reserve(callers.size());

	for (auto const& callsite: callers)
	{
		auto store = trackerState.getMemo().lookup(callsite);
		assert(store != nullptr);

		auto tVal = store->lookup(obj);
		retVec.push_back(tVal);
	}
	return retVec;
}

/**
 * Tracks memory-based imprecision through function calls.
 * 
 * This function identifies which memory objects at call sites cause imprecision
 * in the callee, either due to their own imprecision or due to the merging
 * of different precise memory states from different call sites.
 *
 * @param pp The program point representing the callee's entry
 * @param callers Vector of call sites to analyze
 */
void CallTracker::trackMemory(const ProgramPoint& pp, const CallerVector& callers)
{
	std::unordered_map<size_t, util::VectorSet<const tpa::MemoryObject*>> trackedObjects;
	
	for (auto const& mapping: pp.getDefUseInstruction()->mem_succs())
	{
		auto obj = mapping.first;
		auto callTaints = getMemoryTaintValues(callers, obj);

		// Record which callers demand precision
		auto demandingIndices = getDemandingIndices(callTaints);
		for (auto idx: demandingIndices)
			trackerState.addImprecisionSource(callers[idx]);

		// Record which callers have imprecise values that need tracking
		auto impreciseIndices = getImpreciseIndices(callTaints);
		for (auto idx: impreciseIndices)
			trackedObjects[idx].insert(obj);
	}

	// Start tracking the identified imprecise memory objects
	LocalTracker localTracker(workList);
	for (auto const& mapping: trackedObjects)
		localTracker.trackMemory(callers[mapping.first], mapping.second);
}

/**
 * Main entry point for tracking imprecision through calls.
 * 
 * This function tracks both value-based and memory-based imprecision
 * from call sites to the callee.
 *
 * @param pp The program point representing the callee's entry
 * @param callers Vector of call sites to analyze
 */
void CallTracker::trackCall(const ProgramPoint& pp, const std::vector<ProgramPoint>& callers)
{
	if (!callers.empty())
	{
		trackValue(pp, callers);
		trackMemory(pp, callers);
	}
}

}