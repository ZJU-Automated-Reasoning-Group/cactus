#pragma once

#include <llvm/ADT/StringRef.h>
#include "PointerAnalysis/Engine/ContextSensitivity.h"

class CommandLineOptions
{
private:
	llvm::StringRef inputFileName;

	llvm::StringRef ptrConfigFileName;
	llvm::StringRef modRefConfigFileName;
	llvm::StringRef taintConfigFileName;
	bool noPrepassFlag;
	tpa::ContextSensitivityPolicy::Policy contextPolicy;
	unsigned kLimit;  // k-limit for context sensitivity

public:
	CommandLineOptions(int argc, char** argv);

	const llvm::StringRef& getInputFileName() const { return inputFileName; }

	const llvm::StringRef& getPtrConfigFileName() const { return ptrConfigFileName; }
	const llvm::StringRef& getModRefConfigFileName() const { return modRefConfigFileName; }
	const llvm::StringRef& getTaintConfigFileName() const { return taintConfigFileName; }
	bool isPrepassDisabled() const { return noPrepassFlag; }
	tpa::ContextSensitivityPolicy::Policy getContextPolicy() const { return contextPolicy; }
	unsigned getKLimit() const { return kLimit; }
};
