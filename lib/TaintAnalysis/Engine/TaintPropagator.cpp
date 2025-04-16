#include "TaintAnalysis/Engine/EvalResult.h"
#include "TaintAnalysis/Engine/TaintPropagator.h"
#include "TaintAnalysis/Engine/WorkList.h"
#include "TaintAnalysis/Support/TaintMemo.h"

#include "Util/IO/TaintAnalysis/Printer.h"
#include <llvm/Support/raw_ostream.h>
using namespace util::io;

namespace taint
{

/**
 * Enqueues a program point for further analysis if the taint value for a memory object
 * has changed in the memo table. This helps optimize the analysis by only processing
 * program points with updated taint information.
 *
 * @param pp The program point to potentially enqueue
 * @param obj The memory object to check for taint changes
 * @param store The taint store containing current taint values
 */
void TaintPropagator::enqueueIfMemoChange(const ProgramPoint& pp, const tpa::MemoryObject* obj, const TaintStore& store)
{
	auto objVal = store.lookup(obj);
	if (objVal == TaintLattice::Unknown)
		return;
	if (memo.insert(pp, obj, objVal))
	{
		//llvm::errs() << "\tenqueue mem " << pp << "\n";
		workList.enqueue(pp);
	}
}

/**
 * Propagates taint information to successor program points based on evaluation results.
 * This function is responsible for driving the taint analysis forward by enqueueing
 * relevant program points in the work list.
 *
 * @param evalResult The evaluation result containing successor information and taint store
 */
void TaintPropagator::propagate(const EvalResult& evalResult)
{
	auto const& store = evalResult.getStore();
	for (auto succ: evalResult)
	{
		if (succ.isTopLevel())
			workList.enqueue(succ.getProgramPoint());
		else
			enqueueIfMemoChange(succ.getProgramPoint(), succ.getMemoryObject(), store);
	}
}

}