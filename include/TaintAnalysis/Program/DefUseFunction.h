#pragma once

#include "TaintAnalysis/Program/DefUseInstruction.h"

namespace taint
{

/**
 * @class DefUseFunction
 * @brief Represents a function in the def-use graph for taint analysis.
 *
 * This class encapsulates an LLVM Function and all its instructions as nodes in
 * the def-use graph. It provides access to special nodes like the function entry
 * and exit points, and maintains a mapping from LLVM Instructions to their
 * corresponding DefUseInstruction nodes.
 */
class DefUseFunction
{
private:
	const llvm::Function& function;  ///< The underlying LLVM Function
	std::unordered_map<const llvm::Instruction*, DefUseInstruction> instMap;  ///< Map from LLVM Instructions to DefUseInstructions

	DefUseInstruction entryInst;  ///< Special node representing the function entry point
	DefUseInstruction* exitInst;  ///< Pointer to the node representing the function exit point (return instruction)
public:
	using NodeType = DefUseInstruction;

	/**
	 * @brief Constructs a DefUseFunction for an LLVM Function
	 * @param f The LLVM Function to represent
	 */
	DefUseFunction(const llvm::Function& f);
	
	/**
	 * @brief Deleted copy constructor (non-copyable)
	 */
	DefUseFunction(const DefUseFunction&) = delete;
	
	/**
	 * @brief Deleted copy assignment operator (non-copyable)
	 */
	DefUseFunction& operator=(const DefUseFunction&) = delete;
	
	/**
	 * @brief Gets the underlying LLVM Function
	 * @return Reference to the LLVM Function
	 */
	const llvm::Function& getFunction() const { return function; }

	/**
	 * @brief Gets the DefUseInstruction for an LLVM Instruction
	 * @param inst The LLVM Instruction to look up
	 * @return Pointer to the corresponding DefUseInstruction, or nullptr if not found
	 */
	DefUseInstruction* getDefUseInstruction(const llvm::Instruction* inst);
	
	/**
	 * @brief Gets the DefUseInstruction for an LLVM Instruction (const version)
	 * @param inst The LLVM Instruction to look up
	 * @return Pointer to the corresponding DefUseInstruction, or nullptr if not found
	 */
	const DefUseInstruction* getDefUseInstruction(const llvm::Instruction* inst) const;	

	/**
	 * @brief Gets the entry node for this function
	 * @return Pointer to the function entry node
	 */
	DefUseInstruction* getEntryInst()
	{
		return &entryInst;
	}
	
	/**
	 * @brief Gets the entry node for this function (const version)
	 * @return Pointer to the function entry node
	 */
	const DefUseInstruction* getEntryInst() const
	{
		return &entryInst;
	}
	
	/**
	 * @brief Gets the exit node for this function
	 * @return Pointer to the function exit node, or nullptr if not found
	 */
	DefUseInstruction* getExitInst()
	{
		return exitInst;
	}
	
	/**
	 * @brief Gets the exit node for this function (const version)
	 * @return Pointer to the function exit node, or nullptr if not found
	 */
	const DefUseInstruction* getExitInst() const
	{
		return exitInst;
	}
};

}
