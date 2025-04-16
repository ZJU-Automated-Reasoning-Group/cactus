#include "TaintAnalysis/Analysis/TaintAnalysis.h"
#include "TaintAnalysis/Engine/Initializer.h"
#include "TaintAnalysis/Engine/SinkViolationChecker.h"
#include "TaintAnalysis/Engine/TaintPropagator.h"
#include "TaintAnalysis/Engine/TaintGlobalState.h"
#include "TaintAnalysis/Engine/TransferFunction.h"
#include "Util/AnalysisEngine/DataFlowAnalysis.h"
#include "Util/IO/TaintAnalysis/Printer.h"

#include <llvm/IR/Instruction.h>
#include <llvm/Support/raw_ostream.h>

using namespace llvm;
using namespace util::io;

namespace taint
{

/**
 * Helper function to print information about sink violations detected during analysis.
 * Displays detailed information about each violation including the program point,
 * affected argument, and expected vs actual taint values.
 *
 * @param pp The program point where the violation occurred
 * @param list List of sink violations to print
 */
static void printSinkViolation(const ProgramPoint& pp, const SinkViolationList& list)
{
	for (auto const& violation: list)
	{
		errs().changeColor(raw_ostream::RED);

		errs() << "\nSink violation at " << *pp.getContext() << ":: " << *pp.getDefUseInstruction()->getInstruction() << "\n";
		errs() << "\tArgument: " << static_cast<unsigned>(violation.argPos) << "\n";
		errs() << "\tExpected: " << violation.expectVal << "\n";
		errs() << "\tActual:   " << violation.actualVal << "\n";

		errs().resetColor();
	}
}

/**
 * Main entry point for the taint analysis on a DefUseModule.
 * This function initializes the taint global state, runs the data flow analysis,
 * and checks for sink violations after analysis completion.
 *
 * @param duModule The DefUseModule to analyze
 * @return true if no sink violations were found, false otherwise
 */
bool TaintAnalysis::runOnDefUseModule(const DefUseModule& duModule)
{
	auto globalState = TaintGlobalState(duModule, ptrAnalysis, extTable, env, memo);
	auto dfa = util::DataFlowAnalysis<TaintGlobalState, TaintMemo, TransferFunction, TaintPropagator>(globalState, memo);
	dfa.runOnInitialState<Initializer>(TaintStore());

	auto violationRecord = SinkViolationChecker(env, memo, extTable, ptrAnalysis).checkSinkViolation(globalState.getSinks());

	for (auto const& mapping: violationRecord)
		printSinkViolation(mapping.first, mapping.second);

	return violationRecord.empty();
}

}