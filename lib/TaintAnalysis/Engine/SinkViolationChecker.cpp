#include "Annotation/Taint/ExternalTaintTable.h"
#include "PointerAnalysis/Analysis/SemiSparsePointerAnalysis.h"
#include "TaintAnalysis/Engine/SinkViolationChecker.h"
#include "TaintAnalysis/Program/DefUseInstruction.h"
#include "TaintAnalysis/Support/SinkSignature.h"
#include "TaintAnalysis/Support/TaintEnv.h"
#include "TaintAnalysis/Support/TaintMemo.h"
#include "TaintAnalysis/Support/TaintValue.h"

#include <llvm/IR/CallSite.h>
#include <llvm/Support/raw_ostream.h>

using namespace annotation;
using namespace llvm;

namespace taint
{

/**
 * Looks up the taint value for a given TaintValue based on the specified taint class.
 * Different taint classes (ValueOnly, DirectMemory, ReachableMemory) require different
 * lookup strategies.
 *
 * @param tv The TaintValue to look up
 * @param what The taint class specifying how to interpret the taint value
 * @param store The taint store containing memory taint information (may be null)
 * @return The computed taint lattice value
 */
TaintLattice SinkViolationChecker::lookupTaint(const TaintValue& tv, TClass what, const TaintStore* store)
{
	switch (what)
	{
		case TClass::ValueOnly:
			return env.lookup(tv);
		case TClass::DirectMemory:
		{
			// Return a conservative value if store is null
			if (store == nullptr) {
				errs() << "Warning: Null store in SinkViolationChecker::lookupTaint. Returning Unknown.\n";
				return TaintLattice::Unknown;
			}
			
			auto pSet = ptrAnalysis.getPtsSet(tv.getContext(), tv.getValue());
			assert(!pSet.empty());
			auto res = TaintLattice::Unknown;
			for (auto loc: pSet)
			{
				auto val = store->lookup(loc);
				res = Lattice<TaintLattice>::merge(res, val);
			}
			return res;
		}
		case TClass::ReachableMemory:
		{
			// Instead of crashing, provide a conservative result (treat as Unknown)
			errs() << "Warning: ReachableMemory used in sink entry. This is not fully supported.\n";
			return TaintLattice::Unknown;
		}
	}
	
	// Should never reach here, but just in case
	return TaintLattice::Unknown;
}

/**
 * Checks if a value's taint status complies with the expected taint status for a sink.
 * If a violation is detected, it is added to the violations list.
 *
 * @param tv The taint value to check
 * @param tClass The taint class to use for lookup
 * @param argPos The argument position in the call
 * @param store The taint store for memory lookups
 * @param violations List to which any detected violations will be added
 */
void SinkViolationChecker::checkValueWithTClass(const TaintValue& tv, TClass tClass, uint8_t argPos, const TaintStore* store, SinkViolationList& violations)
{
	auto currVal = lookupTaint(tv, tClass, store);
	auto cmpRes = Lattice<TaintLattice>::compare(TaintLattice::Untainted, currVal);

	if (cmpRes != LatticeCompareResult::Equal && cmpRes != LatticeCompareResult::GreaterThan)
		violations.push_back({ argPos, tClass, TaintLattice::Untainted, currVal });
}

/**
 * Checks a call site against a sink taint entry for taint violations.
 * Examines arguments at positions specified by the sink entry.
 *
 * @param pp The program point (call site) to check
 * @param entry The sink taint entry containing expected taint information
 * @param violations List to which any detected violations will be added
 */
void SinkViolationChecker::checkCallSiteWithEntry(const ProgramPoint& pp, const SinkTaintEntry& entry, SinkViolationList& violations)
{
	auto taintPos = entry.getArgPosition().getAsArgPosition();

	ImmutableCallSite cs(pp.getDefUseInstruction()->getInstruction());
	auto checkArgument = [this, &pp, &entry, &violations, &cs] (size_t idx)
	{
		auto store = memo.lookup(pp);

		auto argVal = TaintValue(pp.getContext(), cs.getArgument(idx));
		checkValueWithTClass(argVal, entry.getTaintClass(), idx, store, violations);
	};
	if (taintPos.isAfterArgPosition())
	{
		for (size_t i = taintPos.getArgIndex(), e = cs.arg_size(); i < e; ++i)
			checkArgument(i);
	}
	else
	{
		checkArgument(taintPos.getArgIndex());
	}

}

/**
 * Checks a call site against a taint summary for sink violations.
 * Processes only the sink entries from the summary.
 *
 * @param pp The program point (call site) to check
 * @param summary The taint summary containing sink entries
 * @return List of detected violations
 */
SinkViolationList SinkViolationChecker::checkCallSiteWithSummary(const ProgramPoint& pp, const TaintSummary& summary)
{
	SinkViolationList violations;
	ImmutableCallSite cs(pp.getDefUseInstruction()->getInstruction());

	for (auto const& entry: summary)
	{
		// We only care about the sink here
		if (entry.getEntryEnd() != TEnd::Sink)
			continue;

		checkCallSiteWithEntry(pp, entry.getAsSinkEntry(), violations);
	}

	return violations;
}

/**
 * Checks a sink signature for taint violations by looking up its associated
 * taint summary and analyzing the call site.
 *
 * @param sig The sink signature to check
 * @param records Map to which violation records will be added
 */
void SinkViolationChecker::checkSinkViolation(const SinkSignature& sig, SinkViolationRecord& records)
{
	if (auto taintSummary = table.lookup(sig.getCallee()->getName()))
	{
		auto callsite = sig.getCallSite();
		auto violations = checkCallSiteWithSummary(sig.getCallSite(), *taintSummary);
		if (!violations.empty())
			records[callsite] = std::move(violations);
	}
	else
	{
		// Instead of crashing, print a warning message
		errs() << "Warning: Unrecognized external function call: " << sig.getCallee()->getName() << "\n";
	}
}

}