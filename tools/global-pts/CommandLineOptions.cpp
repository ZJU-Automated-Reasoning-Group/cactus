#include "CommandLineOptions.h"
#include "Util/CommandLine/TypedCommandLineParser.h"

using namespace util;
using namespace tpa;

CommandLineOptions::CommandLineOptions(int argc, char** argv): 
    outputFileName(""), 
    noPrepassFlag(false), 
    dumpTypeFlag(false),
    contextPolicy(ContextSensitivityPolicy::Policy::UniformKLimit),
    kLimit(1) // Default to k=1
{
	TypedCommandLineParser cmdParser("Global pointer analysis for LLVM IR");
	cmdParser.addStringPositionalFlag("inputFile", "Input LLVM bitcode file name", inputFileName);
	cmdParser.addStringOptionalFlag("o", "Output LLVM bitcode file name", outputFileName);
	cmdParser.addBooleanOptionalFlag("no-prepass", "Do no run IR cannonicalization before the analysis", noPrepassFlag);
	cmdParser.addBooleanOptionalFlag("print-type", "Dump the internal type of the translated values", dumpTypeFlag);

	// Add context sensitivity policy option
	llvm::StringRef policyStr = "uniform-k"; // Default value
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
	    // Default to UniformKLimit if unrecognized
	    contextPolicy = ContextSensitivityPolicy::Policy::UniformKLimit;
	}
	
	// Parse k-limit from string
	try {
	    kLimit = std::stoi(kLimitStr.str());
	} catch (const std::exception&) {
	    kLimit = 1; // Default to 1 if parsing fails
	}
}