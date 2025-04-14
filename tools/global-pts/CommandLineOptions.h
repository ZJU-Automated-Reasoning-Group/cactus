#pragma once

#include <llvm/ADT/StringRef.h>
#include "PointerAnalysis/Engine/ContextSensitivity.h"

class CommandLineOptions
{
private:
	llvm::StringRef inputFileName;
	llvm::StringRef outputFileName;

	bool noPrepassFlag;
	bool dumpTypeFlag;
	tpa::ContextSensitivityPolicy::Policy contextPolicy;
	unsigned kLimit;  // k-limit for context sensitivity
public:
	CommandLineOptions(int argc, char** argv);

	const llvm::StringRef& getInputFileName() { return inputFileName; }
	const llvm::StringRef& getOutputFileName() const { return outputFileName; }

	bool isPrepassDisabled() const { return noPrepassFlag; }
	bool shouldPrintType() const { return dumpTypeFlag; }
	tpa::ContextSensitivityPolicy::Policy getContextPolicy() const { return contextPolicy; }
	unsigned getKLimit() const { return kLimit; }
};

