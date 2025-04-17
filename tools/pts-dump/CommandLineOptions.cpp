#include "CommandLineOptions.h"
#include "Util/CommandLine/TypedCommandLineParser.h"
#include <llvm/Support/raw_ostream.h>

using namespace util;
using namespace llvm;

CommandLineOptions::CommandLineOptions(int argc, char** argv): ptrConfigFileName("ptr.config"), noPrepassFlag(false), dumpPtsFlag(false), k(1)
{
	TypedCommandLineParser cmdParser("Points-to set dumper");
	cmdParser.addStringPositionalFlag("inputFile", "Input LLVM bitcode file name", inputFileName);
	cmdParser.addStringOptionalFlag("ptr-config", "Annotation file for external library points-to analysis (default = <current dir>/ptr.config)", ptrConfigFileName);
	cmdParser.addUIntOptionalFlag("k", "The size limit of the stack for k-CFA (default = 1)", k);
	cmdParser.addBooleanOptionalFlag("no-prepass", "Do no run IR cannonicalization before the analysis", noPrepassFlag);
	cmdParser.addBooleanOptionalFlag("dump-pts", "Dump points-to sets after analysis", dumpPtsFlag);

	cmdParser.parseCommandLineOptions(argc, argv);
	
	// Output the provided k value to confirm it was read correctly
	errs() << "CommandLineOptions: Context sensitivity k=" << k << "\n";
}