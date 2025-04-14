#include "CommandLineOptions.h"
#include "RunAnalysis.h"

#include "Context/KLimitContext.h"
#include "Context/SelectiveKCFA.h"
#include "PointerAnalysis/Engine/ContextSensitivity.h"
#include "PointerAnalysis/Analysis/ContextSensitivePointerAnalysis.h"
#include "PointerAnalysis/Analysis/GlobalPointerAnalysis.h"
#include "PointerAnalysis/FrontEnd/Type/TypeAnalysis.h"
#include "PointerAnalysis/MemoryModel/MemoryManager.h"
#include "PointerAnalysis/MemoryModel/PointerManager.h"
#include "PointerAnalysis/Support/Env.h"
#include "PointerAnalysis/Support/Store.h"
#include "Util/IO/PointerAnalysis/Printer.h"

#include <llvm/ADT/StringRef.h>
#include <llvm/IR/Argument.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Value.h>

using namespace llvm;
using namespace tpa;
using namespace util::io;

namespace
{

void dumpEnv(const Env& env)
{
	outs() << "Env:\n";
	for (auto const& mapping: env)
	{
		outs() << "  " << *mapping.first << "  ->  " << mapping.second << "\n";
	}
	outs() << "\n";
}

void dumpStore(const Store& store)
{
	outs() << "Store:\n";
	for (auto const& mapping: store)
	{
		outs() << "  " << *mapping.first << "  ->  " << mapping.second << "\n";
	}
	outs() << "\n";
}

void dumpTypeMap(const TypeMap& typeMap)
{
	outs() << "TypeMap:\n";
	for (auto const& mapping: typeMap)
	{
		outs() << "  " << *const_cast<Type*>(mapping.first) << "  ->  " << *mapping.second << "\n";
	}
	outs() << "\n";
}

}

void runAnalysisOnModule(const llvm::Module& module, const CommandLineOptions& opts)
{
	// Get policy and k-limit from options
	auto policy = opts.getContextPolicy();
	auto k = opts.getKLimit();
	
	// Apply k-limit to the appropriate context policy
	if (policy == ContextSensitivityPolicy::Policy::UniformKLimit) {
		context::KLimitContext::setLimit(k);
	} else if (policy == ContextSensitivityPolicy::Policy::SelectiveKCFA) {
		context::SelectiveKCFA::setDefaultLimit(k);
	}
	
	// Configure the context sensitivity policy
	ContextSensitivityPolicy::configurePolicy(policy, &module);

	TypeAnalysis typeAnalysis;
	auto typeMap = typeAnalysis.runOnModule(module);

	PointerManager ptrManager;
	MemoryManager memManager;
	auto envStore = tpa::GlobalPointerAnalysis(ptrManager, memManager, typeMap).runOnModule(module);

	if (opts.shouldPrintType())
		dumpTypeMap(typeMap);

	dumpEnv(envStore.first);
	dumpStore(envStore.second);
	
	// Print statistics about our context sensitivity configuration
	if (ContextSensitivityPolicy::activePolicy == ContextSensitivityPolicy::Policy::SelectiveKCFA) {
		outs() << "\nContext sensitivity policy: SelectiveKCFA (default k=" << k << ")\n";
		context::SelectiveKCFA::printStats();
	} else if (ContextSensitivityPolicy::activePolicy == ContextSensitivityPolicy::Policy::UniformKLimit) {
		outs() << "\nContext sensitivity policy: Uniform k-limit (k=" << k << ")\n";
	} else {
		outs() << "\nContext sensitivity policy: No context sensitivity (k=0)\n";
	}
}