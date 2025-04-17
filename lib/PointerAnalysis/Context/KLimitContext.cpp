/*
 * KLimitContext.cpp
 *
 * Limits context depth to a configurable k value
 * When the context depth reaches k, no more contexts are pushed (effectively merging paths)
 */

#include "Context/KLimitContext.h"
#include "Context/ProgramPoint.h"
#include <llvm/Support/raw_ostream.h>

using namespace llvm;

namespace context
{

const Context* KLimitContext::pushContext(const ProgramPoint& pp)
{
	return pushContext(pp.getContext(), pp.getInstruction());
}

const Context* KLimitContext::pushContext(const Context* ctx, const Instruction* inst)
{
	size_t k = defaultLimit;
	
	// Debug output to verify k value being used
	static bool firstTime = true;
	if (firstTime) {
		errs() << "DEBUG: KLimitContext initialized with k=" << k << "\n";
		firstTime = false;
	}
	
	// When k=0, we use the global context only (no call context)
	if (k == 0) {
		return Context::getGlobalContext();
	}
	
	// If context depth is already at or beyond k limit, return the current context
	// This effectively merges paths when context depth reaches k
	if (ctx->size() >= k) {
		// We're limiting context depth - in a real application with deep call chains
		// this should trigger at some point when k is smaller than the deepest call chain
		static size_t limitHits = 0;
		if (limitHits < 5) {
			errs() << "DEBUG: Context limit k=" << k << " reached, context size=" << ctx->size() << "\n";
			limitHits++;
		}
		return ctx;
	}
	else {
		// Creating a new context with call site pushed
		static size_t newContexts = 0;
		if (newContexts < 5) {
			errs() << "DEBUG: Creating new context with depth=" << (ctx->size() + 1) 
				   << " (limit k=" << k << ")\n";
			newContexts++;
		}
		return Context::pushContext(ctx, inst);
	}
}

}
