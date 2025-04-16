/**
 * @file PrecisionLossTracker.cpp
 * @brief Implementation of precision loss tracking for taint analysis
 *
 * This file provides functionality to track the sources of imprecision in taint analysis
 * that lead to false positives at taint sinks. The tracker works backward from sink
 * violations to identify program points that contribute to imprecision.
 */
#include "PointerAnalysis/Analysis/SemiSparsePointerAnalysis.h"
#include "TaintAnalysis/Engine/TaintGlobalState.h"
#include "TaintAnalysis/Precision/LocalTracker.h"
#include "TaintAnalysis/Precision/PrecisionLossTracker.h"
#include "TaintAnalysis/Precision/TrackerGlobalState.h"
#include "TaintAnalysis/Precision/TrackerTransferFunction.h"
#include "TaintAnalysis/Precision/TrackerWorkList.h"

#include <llvm/IR/CallSite.h>

#include <unordered_set>

namespace taint
{

/**
 * Initializes the work list with starting points for tracking imprecision.
 * 
 * This function identifies program points with Either (imprecise) taint values
 * at sink violations and adds them to the work list for backward tracking.
 *
 * @param workList The work list to initialize with starting points
 * @param record The record of sink violations to analyze
 */
void PrecisionLossTracker::initializeWorkList(TrackerWorkList& workList, const SinkViolationRecord& record)
{
	auto const& ptrAnalysis = globalState.getPointerAnalysis();
	for (auto const& mapping: record)
	{
		// We only care about imprecision
		auto const& pp = mapping.first;
		llvm::ImmutableCallSite cs(pp.getDefUseInstruction()->getInstruction());
		assert(cs);
		auto ctx = pp.getContext();
		std::unordered_set<const llvm::Value*> trackedValues;
		std::unordered_set<const tpa::MemoryObject*> trackedObjects;
		
		for (auto const& violation: mapping.second)
		{
			// Only track Either values which represent imprecision
			if (violation.actualVal != TaintLattice::Either)
				continue;

			auto argVal = cs.getArgument(violation.argPos);
			if (violation.what == annotation::TClass::ValueOnly)
			{
				trackedValues.insert(argVal);
			}
			else
			{
				auto pSet = ptrAnalysis.getPtsSet(ctx, argVal);
				assert(!pSet.empty());

				trackedObjects.insert(pSet.begin(), pSet.end());
			}
		}

		// Initialize tracking for identified imprecise values and memory objects
		LocalTracker tracker(workList);
		tracker.trackValue(pp, trackedValues);
		tracker.trackMemory(pp, trackedObjects);
	}
}

/**
 * Tracks sources of imprecision through the program from sink violations.
 * 
 * This is the main entry point for precision loss tracking. It performs
 * a backward analysis starting from sink violations to identify program
 * points that contribute to imprecision (taint values of "Either").
 *
 * @param record The record of sink violations to analyze
 * @return A set of program points that contribute to imprecision
 */
ProgramPointSet PrecisionLossTracker::trackImprecision(const SinkViolationRecord& record)
{
	ProgramPointSet ppSet;

	// Prepare the tracker state
	TrackerGlobalState trackerState(globalState.getDefUseModule(), globalState.getPointerAnalysis(), globalState.getExternalTaintTable(), globalState.getEnv(), globalState.getMemo(), globalState.getCallGraph(), ppSet);

	// Prepare the tracker worklist
	TrackerWorkList workList;
	initializeWorkList(workList, record);

	// The main backward analysis
	while (!workList.empty())
	{
		auto pp = workList.dequeue();

		TrackerTransferFunction(trackerState, workList).eval(pp);
	}

	return ppSet;
}

}