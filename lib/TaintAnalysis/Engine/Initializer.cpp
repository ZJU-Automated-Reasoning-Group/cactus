#include "Context/Context.h"
#include "PointerAnalysis/Analysis/SemiSparsePointerAnalysis.h"
#include "TaintAnalysis/Engine/TaintGlobalState.h"
#include "TaintAnalysis/Engine/Initializer.h"
#include "TaintAnalysis/Program/DefUseModule.h"
#include "TaintAnalysis/Support/TaintEnv.h"
#include "TaintAnalysis/Support/TaintMemo.h"

#include <llvm/IR/Function.h>

using namespace context;

namespace taint
{

/**
 * Constructor for Initializer class.
 * Sets up references to key components needed for initializing the taint analysis.
 *
 * @param g The global state for taint analysis
 * @param m The taint memo table for recording taint values
 */
Initializer::Initializer(TaintGlobalState& g, TaintMemo& m): duModule(g.getDefUseModule()), env(g.getEnv()), ptrAnalysis(g.getPointerAnalysis()), memo(m)
{
}

/**
 * Initializes taint values for command-line arguments in the main function.
 * Sets up initial taint status for argc, argv, and envp as appropriate:
 * - argc is considered tainted (user-controlled)
 * - argv pointer itself is untainted
 * - argv contents (strings) are tainted
 * - envp pointer is untainted
 * - envp contents are tainted
 *
 * @param store The taint store to update with initial taint values
 */
void Initializer::initializeMainArgs(TaintStore& store)
{
	auto const& entryFunc = duModule.getEntryFunction().getFunction();
	auto globalCtx = Context::getGlobalContext();
	
	if (entryFunc.arg_size() > 0)
	{
		// argc is tainted
		auto argcValue = entryFunc.arg_begin();
		env.strongUpdate(TaintValue(globalCtx, argcValue), TaintLattice::Tainted);

		if (entryFunc.arg_size() > 1)
		{
			// argv is not tainted
			auto argvValue = (++entryFunc.arg_begin());
			env.strongUpdate(TaintValue(globalCtx, argvValue), TaintLattice::Untainted);

			// *argv and **argv are tainted
			auto argvObj = ptrAnalysis.getMemoryManager().getArgvObject();
			store.strongUpdate(argvObj, TaintLattice::Tainted);

			if (entryFunc.arg_size() > 2)
			{
				auto envpValue = (++argvValue);
				env.strongUpdate(TaintValue(globalCtx, envpValue), TaintLattice::Untainted);

				auto envpObj = ptrAnalysis.getMemoryManager().getEnvpObject();
				store.strongUpdate(envpObj, TaintLattice::Tainted);
			}
		}
	}
}

/**
 * Initializes taint values for global variables in the program.
 * By default, all global variables are considered untainted, except for
 * the universal object which is marked as "Either" (may be tainted or untainted).
 *
 * @param store The taint store to update with initial taint values
 */
void Initializer::initializeGlobalVariables(TaintStore& store)
{
	auto globalCtx = Context::getGlobalContext();
	for (auto const& global: duModule.getModule().globals())
	{
		// We don't need to worry about global variables in env because all of them are constants and are already handled properly

		auto pSet = ptrAnalysis.getPtsSet(globalCtx, &global);
		for (auto obj: pSet)
		{
			if (!obj->isSpecialObject())
				store.strongUpdate(obj, TaintLattice::Untainted);
		}
	}
	store.strongUpdate(tpa::MemoryManager::getUniversalObject(), TaintLattice::Either);
}

/**
 * Main entry point for initializing the taint analysis state.
 * Sets up initial taint values for program inputs and global variables,
 * then enqueues the entry instruction for analysis.
 *
 * @param initStore Initial taint store (typically empty)
 * @return WorkList containing the entry point for beginning taint analysis
 */
WorkList Initializer::runOnInitState(TaintStore&& initStore)
{
	WorkList workList;

	initializeMainArgs(initStore);
	initializeGlobalVariables(initStore);

	auto entryCtx = context::Context::getGlobalContext();
	auto entryInst = duModule.getEntryFunction().getEntryInst();
	assert(entryInst != nullptr);
	
	auto pp = ProgramPoint(entryCtx, entryInst);
	memo.update(pp, std::move(initStore));
	workList.enqueue(pp);

	return workList;
}

}