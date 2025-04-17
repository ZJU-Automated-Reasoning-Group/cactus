#pragma once

#include <llvm/ADT/StringRef.h>

class CommandLineOptions
{
private:
	llvm::StringRef inputFileName;
	llvm::StringRef ptrConfigFileName;

	bool noPrepassFlag;
	bool dumpPtsFlag;
	unsigned k;
public:
	CommandLineOptions(int argc, char** argv);

	const llvm::StringRef& getInputFileName() const { return inputFileName; }
	const llvm::StringRef& getPtrConfigFileName() const { return ptrConfigFileName; }

	bool isPrepassDisabled() const { return noPrepassFlag; }
	bool shouldDumpPts() const { return dumpPtsFlag; }
	unsigned getContextSensitivity() const { return k; }
};
