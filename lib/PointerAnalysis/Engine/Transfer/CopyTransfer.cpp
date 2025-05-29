#include "PointerAnalysis/Engine/GlobalState.h"
#include "PointerAnalysis/Engine/TransferFunction.h"
#include "PointerAnalysis/MemoryModel/MemoryManager.h"
#include "PointerAnalysis/MemoryModel/PointerManager.h"

namespace tpa
{

void TransferFunction::evalCopyNode(const ProgramPoint& pp, EvalResult& evalResult)
{
	auto ctx = pp.getContext();
	auto const& copyNode = static_cast<const CopyCFGNode&>(*pp.getCFGNode());

	std::vector<PtsSet> srcPtsSets;
	srcPtsSets.reserve(copyNode.getNumSrc());

	auto& ptrManager = globalState.getPointerManager();
	auto& env = globalState.getEnv();
	for (auto src: copyNode)
	{
		auto srcPtr = ptrManager.getOrCreatePointer(ctx, src);

		if (srcPtr == nullptr)
			continue;

		auto pSet = env.lookup(srcPtr);
		if (pSet.empty())
			// Operand not ready
			continue;

		srcPtsSets.emplace_back(pSet);
	}

	auto dstPtr = ptrManager.getOrCreatePointer(ctx, copyNode.getDest());
	auto dstSet = PtsSet::mergeAll(srcPtsSets);
	auto envChanged = globalState.getEnv().weakUpdate(dstPtr, dstSet);
	
	if (envChanged)
		addTopLevelSuccessors(pp, evalResult);
}

}
