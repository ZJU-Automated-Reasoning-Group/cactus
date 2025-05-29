#include "PointerAnalysis/Engine/GlobalState.h"
#include "PointerAnalysis/Engine/TransferFunction.h"
#include "PointerAnalysis/MemoryModel/MemoryManager.h"
#include "PointerAnalysis/MemoryModel/PointerManager.h"
#include <llvm/Support/raw_ostream.h>

namespace tpa
{

void TransferFunction::strongUpdateStore(const MemoryObject* obj, PtsSet srcSet, Store& store)
{
	store.strongUpdate(obj, srcSet);
}

void TransferFunction::weakUpdateStore(PtsSet dstSet, PtsSet srcSet, Store& store)
{
	for (auto obj: dstSet)
		store.weakUpdate(obj, srcSet);
}

void TransferFunction::evalStore(const Pointer* dst, const Pointer* src, const ProgramPoint& pp, EvalResult& evalResult)
{
	// Debug - verify pointer contexts match the program point
	static size_t storeOpCount = 0;
	bool showDebug = storeOpCount < 20;
	storeOpCount++;
	
	if (showDebug) {
		auto ctx = pp.getContext();
		llvm::errs() << "DEBUG: [Store:" << storeOpCount << "] pp ctx depth=" << ctx->size()
		             << ", dst ptr ctx depth=" << dst->getContext()->size()
					 << ", src ptr ctx depth=" << src->getContext()->size();
					 
		if (ctx != dst->getContext() || ctx != src->getContext()) {
			llvm::errs() << " (CONTEXT MISMATCH!)";
		}
		llvm::errs() << "\n";
	}

	auto& env = globalState.getEnv();

	auto srcSet = env.lookup(src);
	if (srcSet.empty()) {
		if (showDebug) {
			llvm::errs() << "DEBUG: [Store:" << storeOpCount << "] srcSet is empty, returning\n";
		}
		return;
	}

	auto dstSet = env.lookup(dst);
	if (dstSet.empty()) {
		if (showDebug) {
			llvm::errs() << "DEBUG: [Store:" << storeOpCount << "] dstSet is empty, returning\n";
		}
		return;
	}

	if (showDebug) {
		llvm::errs() << "DEBUG: [Store:" << storeOpCount << "] srcSet size=" << srcSet.size() 
		             << ", dstSet size=" << dstSet.size() << "\n";
		llvm::errs() << "DEBUG: [Store:" << storeOpCount << "] src value: ";
		if (src->getValue()) {
			src->getValue()->print(llvm::errs());
		} else {
			llvm::errs() << "null";
		}
		llvm::errs() << "\n";
		llvm::errs() << "DEBUG: [Store:" << storeOpCount << "] dst value: ";
		if (dst->getValue()) {
			dst->getValue()->print(llvm::errs());
		} else {
			llvm::errs() << "null";
		}
		llvm::errs() << "\n";
	}

	auto& store = evalResult.getNewStore(*localState);

	auto dstObj = *dstSet.begin();
	// If the store target is precise and the target location is not unknown
	// TOOD: if the dstSet may grow, under what conditions can we perform the strong update here (is it because we are perfomring a flow-sensitive analysis)?
	if (dstSet.size() == 1 && !dstObj->isSummaryObject()) {
		if (showDebug) {
			llvm::errs() << "DEBUG: [Store:" << storeOpCount << "] Using strongUpdate (dstSet.size()=1)\n";
		}
		strongUpdateStore(dstObj, srcSet, store);
	} else {
		if (showDebug) {
			llvm::errs() << "DEBUG: [Store:" << storeOpCount << "] Using weakUpdate (dstSet.size()=" << dstSet.size() << ")\n";
		}
		weakUpdateStore(dstSet, srcSet, store);
	}

	addMemLevelSuccessors(pp, store, evalResult);
}

void TransferFunction::evalStoreNode(const ProgramPoint& pp, EvalResult& evalResult)
{
	auto ctx = pp.getContext();
	auto const& storeNode = static_cast<const StoreCFGNode&>(*pp.getCFGNode());

	auto& ptrManager = globalState.getPointerManager();
	// FIXED: Use getOrCreatePointer instead of getPointer to enable context sensitivity
	auto srcPtr = ptrManager.getOrCreatePointer(ctx, storeNode.getSrc());
	auto dstPtr = ptrManager.getOrCreatePointer(ctx, storeNode.getDest());

	// Debug - track when pointers are missing
	static size_t storeNodeCount = 0;
	bool showDebug = storeNodeCount < 20;
	storeNodeCount++;
	
	if (showDebug) {
		llvm::errs() << "DEBUG: [StoreNode:" << storeNodeCount << "] Created pointers: "
		             << "src ctx depth=" << srcPtr->getContext()->size()
					 << ", dst ctx depth=" << dstPtr->getContext()->size()
					 << ", pp ctx depth=" << ctx->size() << "\n";
	}

	evalStore(dstPtr, srcPtr, pp, evalResult);
}

}
