/*
 * KLimitContext.cpp
 *
 * Limits context depth to a configurable k value
 * When the context depth reaches k, no more contexts are pushed (effectively merging paths)
 */

#include "Context/KLimitContext.h"
#include "Context/ProgramPoint.h"
#include <llvm/Support/raw_ostream.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Function.h>

using namespace llvm;

namespace context
{

// Make sure the default limit is initialized properly
unsigned KLimitContext::defaultLimit = 1u;

const Context* KLimitContext::pushContext(const ProgramPoint& pp)
{
	return pushContext(pp.getContext(), pp.getInstruction());
}

const Context* KLimitContext::pushContext(const Context* ctx, const Instruction* inst)
{
	// IMPORTANT: Track every time we try to push a context
	static size_t pushCount = 0;
	pushCount++;
	
	// Only show detailed debug for first 20 pushes to avoid flooding output
	bool showDebug = pushCount <= 20;
	
	size_t k = defaultLimit;
	
	// Debug output to verify k value being used
	static bool firstTime = true;
	if (firstTime) {
		errs() << "DEBUG: KLimitContext using k=" << k << "\n";
		firstTime = false;
	}
	
	// When k=0, we use the global context only (no call context)
	if (k == 0) {
		if (showDebug) {
			errs() << "DEBUG: [" << pushCount << "] Using global context (k=0)\n";
		}
		return Context::getGlobalContext();
	}
	
	// CRITICAL: Check if this is a valid call instruction that should create a context
	// Only call and invoke instructions should create new contexts
	if (inst && (isa<CallInst>(inst) || isa<InvokeInst>(inst))) {
		const Function* calledFn = nullptr;
		
		// Get the called function
		if (const CallInst* callInst = dyn_cast<CallInst>(inst)) {
			calledFn = callInst->getCalledFunction();
		} else if (const InvokeInst* invokeInst = dyn_cast<InvokeInst>(inst)) {
			calledFn = invokeInst->getCalledFunction();
		}
		
		if (showDebug) {
			errs() << "DEBUG: [" << pushCount << "] Found call instruction: ";
			if (calledFn) {
				errs() << calledFn->getName() << "\n";
			} else {
				errs() << "[indirect call]\n";
			}
		}
		
		// Check if context depth is at or beyond k limit
		if (ctx->size() >= k) {
			// We're limiting context depth
			static size_t limitHits = 0;
			if (limitHits < 5) {
				errs() << "DEBUG: Context limit k=" << k << " reached, context size=" << ctx->size() << "\n";
				limitHits++;
			}
			return ctx;
		}
		else {
			// Create a new context with call site pushed
			static size_t newContexts = 0;
			if (newContexts < 5) {
				errs() << "DEBUG: Creating new context with depth=" << (ctx->size() + 1) 
					<< " (limit k=" << k << ")\n";
				newContexts++;
			}
			return Context::pushContext(ctx, inst);
		}
	} else {
		// Not a call instruction or null instruction - don't create a new context
		if (showDebug) {
			errs() << "DEBUG: [" << pushCount << "] Not creating context for ";
			if (inst) {
				errs() << "instruction type " << inst->getOpcodeName() << "\n";
			} else {
				errs() << "null instruction\n";
			}
		}
		return ctx;
	}
}

}
