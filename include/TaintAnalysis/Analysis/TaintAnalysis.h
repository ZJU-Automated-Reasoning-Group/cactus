#pragma once

#include "Annotation/Taint/ExternalTaintTable.h"
#include "TaintAnalysis/Support/TaintEnv.h"
#include "TaintAnalysis/Support/TaintMemo.h"

namespace tpa
{
	class SemiSparsePointerAnalysis;
}

namespace taint
{

class DefUseModule;

/**
 * @class TaintAnalysis
 * @brief Main class for performing dataflow-based taint analysis on LLVM modules.
 *
 * This class coordinates the process of taint analysis, using a def-use graph
 * representation of the program to track the flow of tainted values through
 * the program. The analysis uses pointer analysis results to handle memory
 * operations and relies on an external table of taint specifications for
 * handling external function calls.
 */
class TaintAnalysis
{
private:
	TaintEnv env;        ///< Environment mapping TaintValues to their taint status
	TaintMemo memo;      ///< Memoization table for caching analysis results
	
	annotation::ExternalTaintTable extTable;  ///< Table of taint specs for external functions
	const tpa::SemiSparsePointerAnalysis& ptrAnalysis;  ///< Pointer analysis results
public:
	/**
	 * @brief Constructor for TaintAnalysis
	 * @param p Reference to the pointer analysis results to use
	 */
	TaintAnalysis(const tpa::SemiSparsePointerAnalysis& p): ptrAnalysis(p) {}

	/**
	 * @brief Loads taint specifications for external functions from a file
	 * @param extFileName Path to the file containing external taint specifications
	 */
	void loadExternalTaintTable(const char* extFileName)
	{
		extTable = annotation::ExternalTaintTable::loadFromFile(extFileName);
	}

	/**
	 * @brief Runs the taint analysis on a def-use module
	 * @param duModule The def-use module to analyze
	 * @return true if no taint violations were found, false otherwise
	 */
	bool runOnDefUseModule(const DefUseModule&);
};

}