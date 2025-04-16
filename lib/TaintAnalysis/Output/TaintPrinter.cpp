#include "TaintAnalysis/Program/DefUseInstruction.h"
#include "TaintAnalysis/Support/ProgramPoint.h"
#include "Util/IO/TaintAnalysis/Printer.h"

#include <llvm/IR/Function.h>
#include <llvm/IR/Instruction.h>

using namespace taint;

namespace util
{
namespace io
{

/**
 * Stream output operator for TaintLattice enumeration.
 * Converts the taint lattice value to a human-readable string representation.
 *
 * @param os Output stream to write to
 * @param l TaintLattice value to print
 * @return Reference to the output stream for chaining
 */
llvm::raw_ostream& operator<< (llvm::raw_ostream& os, TaintLattice l)
{
	switch (l)
	{
		case TaintLattice::Unknown:
			os << "Unknown";
			break;
		case TaintLattice::Untainted:
			os << "Untainted";
			break;
		case TaintLattice::Tainted:
			os << "Tainted";
			break;
		case TaintLattice::Either:
			os << "Either";
			break;
	}
	return os;
}

/**
 * Stream output operator for DefUseInstruction.
 * Formats the instruction for display, showing either the entry point
 * information or the instruction itself within its function context.
 *
 * @param os Output stream to write to
 * @param duInst DefUseInstruction to print
 * @return Reference to the output stream for chaining
 */
llvm::raw_ostream& operator<< (llvm::raw_ostream& os, const taint::DefUseInstruction& duInst)
{
	if (duInst.isEntryInstruction())
		os << "[entry duInst for " << duInst.getFunction()->getName() << "]";
	else
		os << "[" << duInst.getFunction()->getName() << *duInst.getInstruction() << "]";
	return os;
}

/**
 * Stream output operator for ProgramPoint.
 * Formats a program point showing both its context and instruction.
 *
 * @param os Output stream to write to
 * @param pp ProgramPoint to print
 * @return Reference to the output stream for chaining
 */
llvm::raw_ostream& operator<< (llvm::raw_ostream& os, const taint::ProgramPoint& pp)
{
	os << "(" << *pp.getContext() << ", " << *pp.getDefUseInstruction() << ")";
	return os;
}

}
}