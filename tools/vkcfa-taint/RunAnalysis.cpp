#include "CommandLineOptions.h"
#include "RunAnalysis.h"

#include "Context/KLimitContext.h"
#include "Context/SelectiveKCFA.h"
#include "PointerAnalysis/Analysis/SemiSparsePointerAnalysis.h"
#include "PointerAnalysis/Analysis/SelectiveKCFAPointerAnalysis.h"
#include "PointerAnalysis/Analysis/ContextSensitivePointerAnalysis.h"
#include "PointerAnalysis/Engine/ContextSensitivity.h"
#include "PointerAnalysis/FrontEnd/SemiSparseProgramBuilder.h"
#include "TaintAnalysis/Analysis/TrackingTaintAnalysis.h"
#include "TaintAnalysis/FrontEnd/DefUseModuleBuilder.h"
#include "Util/IO/TaintAnalysis/Printer.h"

#include <llvm/IR/Function.h>
#include <llvm/IR/Module.h>
#include <llvm/Support/raw_ostream.h>

using namespace context;
using namespace llvm;
using namespace tpa;
using namespace taint;
using namespace util::io;

bool runAnalysisOnModule(const Module& module, const CommandLineOptions& opts)
{
	// Get policy and k-limit from options
	auto policy = opts.getContextPolicy();
	auto k = opts.getKLimit();
	
	// Apply k-limit to the appropriate context policy
	if (policy == ContextSensitivityPolicy::Policy::UniformKLimit) {
		KLimitContext::setLimit(k);
	} else if (policy == ContextSensitivityPolicy::Policy::SelectiveKCFA) {
		SelectiveKCFA::setDefaultLimit(k);
	}
	
	// Configure the context sensitivity policy
	ContextSensitivityPolicy::configurePolicy(policy, &module);

	SemiSparseProgramBuilder ssProgBuilder;
	auto ssProg = ssProgBuilder.runOnModule(module);

	SemiSparsePointerAnalysis ptrAnalysis;
	ptrAnalysis.loadExternalPointerTable(opts.getPtrConfigFileName().data());
	ptrAnalysis.runOnProgram(ssProg);

	DefUseModuleBuilder builder(ptrAnalysis);
	builder.loadExternalModRefTable(opts.getModRefConfigFileName().data());
	auto duModule = builder.buildDefUseModule(module);

	TrackingTaintAnalysis taintAnalysis(ptrAnalysis);
	taintAnalysis.loadExternalTaintTable(opts.getTaintConfigFileName().data());
	auto ret = taintAnalysis.runOnDefUseModule(duModule);

	for (auto const& pp: ret.second)
		errs() << "Find loss site " << pp << "\n";
		
	// Print the context sensitivity policy stats
	if (ContextSensitivityPolicy::activePolicy == ContextSensitivityPolicy::Policy::SelectiveKCFA) {
		errs() << "\nContext sensitivity policy: SelectiveKCFA (default k=" << k << ")\n";
		SelectiveKCFA::printStats();
	} else if (ContextSensitivityPolicy::activePolicy == ContextSensitivityPolicy::Policy::UniformKLimit) {
		errs() << "\nContext sensitivity policy: Uniform k-limit (k=" << k << ")\n";
	} else {
		errs() << "\nContext sensitivity policy: No context sensitivity (k=0)\n";
	}

	return ret.first;
}