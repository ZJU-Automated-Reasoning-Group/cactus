#pragma once

#include "Annotation/Taint/TaintDescriptor.h"
#include "PointerAnalysis/Support/PtsSet.h"
#include "TaintAnalysis/Engine/EvalResult.h"

namespace annotation
{
	class PipeTaintEntry;
	class SourceTaintEntry;
	class TaintSummary;
}

namespace llvm
{
	class Function;
	class ImmutableCallSite;
	class Instruction;
}

namespace tpa
{
	class FunctionContext;
}

namespace taint
{

class ProgramPoint;
class TaintGlobalState;
class TaintLocalState;
class TaintValue;

/**
 * @class TransferFunction
 * @brief Core component of the taint analysis that defines how taint propagates.
 *
 * This class implements the transfer function for the taint analysis dataflow
 * algorithm. It evaluates how taint flows through different types of LLVM
 * instructions (loads, stores, calls, etc.) and updates the taint state
 * accordingly. The transfer function handles both direct (register-based) and
 * indirect (memory-based) taint propagation.
 */
class TransferFunction
{
private:
	TaintGlobalState& globalState;  ///< The global state of the taint analysis
	const TaintStore* localState;   ///< The local taint store for the current program point

	/**
	 * @brief Adds top-level (direct value) successors to the evaluation result
	 */
	void addTopLevelSuccessors(const ProgramPoint&, EvalResult&);
	
	/**
	 * @brief Adds all memory-level successors to the evaluation result
	 */
	void addMemLevelSuccessors(const ProgramPoint&, EvalResult&);
	
	/**
	 * @brief Adds memory-level successors for a specific memory object
	 */
	void addMemLevelSuccessors(const ProgramPoint&, const tpa::MemoryObject*, EvalResult&);
	
	/**
	 * @brief Computes the taint value for an instruction based on its operands
	 */
	TaintLattice getTaintForOperands(const context::Context*, const llvm::Instruction*);
	
	/**
	 * @brief Loads taint values from memory locations in a points-to set
	 */
	TaintLattice loadTaintFromPtsSet(tpa::PtsSet, const TaintStore&);
	
	/**
	 * @brief Performs a strong update of a memory object's taint value
	 */
	void strongUpdateStore(const tpa::MemoryObject*, TaintLattice, TaintStore&);
	
	/**
	 * @brief Performs a weak update of memory objects' taint values
	 */
	void weakUpdateStore(tpa::PtsSet, TaintLattice, TaintStore&);

	/**
	 * @brief Collects taint values for arguments at a call site
	 */
	std::vector<TaintLattice> collectArgumentTaintValue(const context::Context*, const llvm::ImmutableCallSite&, size_t);
	
	/**
	 * @brief Updates parameter taint values for a function call
	 */
	bool updateParamTaintValue(const context::Context*, const llvm::Function*, const std::vector<TaintLattice>&);
	
	/**
	 * @brief Evaluates a call to an internal function
	 */
	void evalInternalCall(const ProgramPoint&, const tpa::FunctionContext&, EvalResult&, bool);
	
	/**
	 * @brief Applies a return value's taint to a call site
	 */
	void applyReturn(const ProgramPoint&, TaintLattice, EvalResult&);

	/**
	 * @brief Updates taint for direct memory access
	 */
	void updateDirectMemoryTaint(const TaintValue&, TaintLattice, const ProgramPoint&, EvalResult&);
	
	/**
	 * @brief Updates taint for reachable memory (e.g., deep copies)
	 */
	void updateReachableMemoryTaint(const TaintValue&, TaintLattice, const ProgramPoint&, EvalResult&);
	
	/**
	 * @brief Updates taint based on taint class (value, direct memory, reachable memory)
	 */
	void updateTaintValueByTClass(const TaintValue&, annotation::TClass, TaintLattice, const ProgramPoint&, EvalResult&);
	
	/**
	 * @brief Updates taint for a call site based on parameter position
	 */
	void updateTaintCallByTPosition(const ProgramPoint&, annotation::TPosition, annotation::TClass, TaintLattice, EvalResult&);
	
	/**
	 * @brief Evaluates a taint source (entry point for tainted data)
	 */
	void evalTaintSource(const ProgramPoint&, const annotation::SourceTaintEntry&, EvalResult&);
	
	/**
	 * @brief Gets taint value based on taint class
	 */
	TaintLattice getTaintValueByTClass(const TaintValue&, annotation::TClass);
	
	/**
	 * @brief Evaluates memcpy-like operations for taint propagation
	 */
	void evalMemcpy(const TaintValue&, const TaintValue&, const ProgramPoint&, EvalResult&);
	
	/**
	 * @brief Evaluates a taint pipe (propagation between parameters)
	 */
	void evalTaintPipe(const ProgramPoint&, const annotation::PipeTaintEntry&, EvalResult&);
	
	/**
	 * @brief Evaluates a call using a taint summary
	 */
	void evalCallBySummary(const ProgramPoint&, const llvm::Function*, const annotation::TaintSummary&, EvalResult&);
	
	/**
	 * @brief Evaluates a call to an external function
	 */
	void evalExternalCall(const ProgramPoint&, const llvm::Function*, EvalResult&);

	/**
	 * @brief Evaluates a function entry point
	 */
	void evalEntry(const ProgramPoint&, EvalResult&, bool);
	
	/**
	 * @brief Evaluates an alloca instruction
	 */
	void evalAlloca(const ProgramPoint&, EvalResult&);
	
	/**
	 * @brief Evaluates instructions where taint flows from operands to result
	 */
	void evalAllOperands(const ProgramPoint&, EvalResult&);
	
	/**
	 * @brief Evaluates a load instruction
	 */
	void evalLoad(const ProgramPoint&, EvalResult&);
	
	/**
	 * @brief Evaluates a store instruction
	 */
	void evalStore(const ProgramPoint&, EvalResult&);
	
	/**
	 * @brief Evaluates a call instruction
	 */
	void evalCall(const ProgramPoint&, EvalResult&);
	
	/**
	 * @brief Evaluates a return instruction
	 */
	void evalReturn(const ProgramPoint&, EvalResult&);
public:
	/**
	 * @brief Constructs a TransferFunction
	 * @param g The global taint state
	 * @param l The local taint store for the current program point
	 */
	TransferFunction(TaintGlobalState& g, const TaintStore* l): globalState(g), localState(l) {}

	/**
	 * @brief Evaluates taint propagation at a program point
	 * @param pp The program point to evaluate
	 * @return The evaluation result containing updated taint and successor points
	 */
	EvalResult eval(const ProgramPoint& pp);
};

}