#include "CommandLineOptions.h"
#include "Util/CommandLine/TypedCommandLineParser.h"

using namespace util;
using namespace tpa;

CommandLineOptions::CommandLineOptions(int argc, char** argv): 
    ptrConfigFileName("ptr.config"), 
    modRefConfigFileName("modref.config"), 
    taintConfigFileName("taint.config"),
    noPrepassFlag(false),
    contextPolicy(ContextSensitivityPolicy::Policy::SelectiveKCFA), // Default to SelectiveKCFA
    kLimit(1) // Default to k=1
{
	TypedCommandLineParser cmdParser("Points-to analysis verifier");
	cmdParser.addStringPositionalFlag("irFile", "Input LLVM bitcode file name", inputFileName);
	cmdParser.addStringOptionalFlag("ptr-config", "Annotation file for external library points-to analysis (default = <current dir>/ptr.config)", ptrConfigFileName);
	cmdParser.addStringOptionalFlag("modref-config", "Annotation file for external library mod/ref analysis (default = <current dir>/modref.config)", modRefConfigFileName);
	cmdParser.addStringOptionalFlag("taint-config", "Annotation file for external library taint analysis (default = <current dir>/taint.config)", taintConfigFileName);
	cmdParser.addBooleanOptionalFlag("no-prepass", "Do no run IR cannonicalization before the analysis", noPrepassFlag);

	// Add context sensitivity policy option
	llvm::StringRef policyStr = "selective-kcfa"; // Default value
	cmdParser.addStringOptionalFlag("context-policy", "Context sensitivity policy (no-context, uniform-k, selective-kcfa)", policyStr);
	
	// Add k-limit option
	llvm::StringRef kLimitStr = "1"; // Default to k=1
	cmdParser.addStringOptionalFlag("k", "Context sensitivity k limit (default = 1)", kLimitStr);

	cmdParser.parseCommandLineOptions(argc, argv);
	
	// Convert string to policy enum
	if (policyStr == "no-context") {
	    contextPolicy = ContextSensitivityPolicy::Policy::NoContext;
	} else if (policyStr == "uniform-k") {
	    contextPolicy = ContextSensitivityPolicy::Policy::UniformKLimit;
	} else if (policyStr == "selective-kcfa") {
	    contextPolicy = ContextSensitivityPolicy::Policy::SelectiveKCFA;
	} else {
	    // Default to SelectiveKCFA if unrecognized
	    contextPolicy = ContextSensitivityPolicy::Policy::SelectiveKCFA;
	}

	// Parse k-limit from string
	try {
	    kLimit = std::stoi(kLimitStr.str());
	} catch (const std::exception&) {
	    kLimit = 1; // Default to 1 if parsing fails
	}
}