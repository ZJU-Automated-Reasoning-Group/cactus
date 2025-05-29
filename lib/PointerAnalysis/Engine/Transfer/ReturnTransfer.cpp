#include "PointerAnalysis/Engine/GlobalState.h"
#include "PointerAnalysis/Engine/TransferFunction.h"
#include "PointerAnalysis/MemoryModel/MemoryManager.h"
#include "PointerAnalysis/MemoryModel/PointerManager.h"

#include <llvm/Support/raw_ostream.h>

using namespace llvm;

namespace tpa
{

std::pair<bool, bool> TransferFunction::evalReturnValue(const context::Context* ctx, const ReturnCFGNode& retNode, const ProgramPoint& retSite)
{
	assert(retSite.getCFGNode()->isCallNode());
	auto const& callNode = static_cast<const CallCFGNode&>(*retSite.getCFGNode());

	auto retVal = retNode.getReturnValue();
	if (retVal == nullptr)
	{
		// calling a void function
		if (auto dstVal = callNode.getDest())
		{
			auto ptr = globalState.getPointerManager().getOrCreatePointer(retSite.getContext(), dstVal);
			return std::make_pair(true, globalState.getEnv().weakUpdate(ptr, PtsSet::getSingletonSet(MemoryManager::getNullObject())));
		}
		else
			return std::make_pair(true, false);
	}

	auto dstVal = callNode.getDest();
	if (dstVal == nullptr)
		// Returned a value, but not used by the caller
		return std::make_pair(true, false);

	auto& ptrManager = globalState.getPointerManager();
	auto retPtr = ptrManager.getOrCreatePointer(ctx, retVal);
	auto& env = globalState.getEnv();
	auto resSet = env.lookup(retPtr);
	if (resSet.empty())
		// Return pointer not ready - this can happen if the return value depends
		// on complex control flow or other dependencies
		return std::make_pair(false, false);

	auto dstPtr = ptrManager.getOrCreatePointer(retSite.getContext(), dstVal);
	return std::make_pair(true, env.weakUpdate(dstPtr, resSet));
}

void TransferFunction::evalReturn(const context::Context* ctx, const ReturnCFGNode& retNode, const ProgramPoint& retSite, EvalResult& evalResult)
{
	bool valid, envChanged;
	std::tie(valid, envChanged) = evalReturnValue(ctx, retNode, retSite);

	if (!valid)
		return;
	if (envChanged)
		addTopLevelSuccessors(retSite, evalResult);
	addMemLevelSuccessors(retSite, *localState, evalResult);
}

void TransferFunction::evalReturnNode(const ProgramPoint& pp, EvalResult& evalResult)
{
	auto ctx = pp.getContext();
	auto const& retNode = static_cast<const ReturnCFGNode&>(*pp.getCFGNode());

	if (retNode.getFunction().getName() == "main")
	{
		// Return from main. Do nothing
		//errs() << "Reached program end\n";
		return;
	}

	// Merge back pruned mappings in store
	//auto prunedStore = globalState.getStorePruner().lookupPrunedStore(FunctionContext(ctx, &retNode.getFunction()));
	//if (prunedStore != nullptr)
	//	evalResult.getStore().mergeWith(*prunedStore);

	auto& callGraph = globalState.getCallGraph();
	auto callers = callGraph.getCallers(FunctionContext(ctx, &retNode.getFunction()));
	for (auto const& caller: callers)
	{
		bool isValid, envChanged;
		std::tie(isValid, envChanged) = evalReturnValue(ctx, retNode, caller);
		if (isValid && envChanged)
			evalResult.addTopLevelProgramPoint(caller);
	}
}

}
