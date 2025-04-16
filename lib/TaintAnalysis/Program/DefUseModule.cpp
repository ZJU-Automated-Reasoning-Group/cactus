#include "TaintAnalysis/Program/DefUseModule.h"

#include <llvm/IR/Instructions.h>
#include <llvm/IR/Module.h>

#include <limits>

using namespace llvm;

namespace taint
{

/**
 * Constructor for a DefUseInstruction that wraps an LLVM Instruction.
 * DefUseInstruction represents a node in the def-use graph.
 * 
 * @param i The LLVM Instruction to wrap
 */
DefUseInstruction::DefUseInstruction(const Instruction& i): inst(&i), rpo(0) {}

/**
 * Constructor for a DefUseInstruction that represents a function entry.
 * This special constructor is used for creating function entry nodes.
 * 
 * @param f The LLVM Function for which this is an entry node
 */
DefUseInstruction::DefUseInstruction(const Function* f): inst(f), rpo(std::numeric_limits<size_t>::max()) {}

/**
 * Gets the underlying LLVM Instruction.
 * 
 * @return Pointer to the wrapped LLVM Instruction
 */
const Instruction* DefUseInstruction::getInstruction() const
{
	return cast<Instruction>(inst);
}

/**
 * Gets the LLVM Function containing this instruction.
 * 
 * @return Pointer to the containing LLVM Function
 */
const Function* DefUseInstruction::getFunction() const
{
	if (auto f = dyn_cast<Function>(inst))
		return f;
	else
	{
		auto i = cast<Instruction>(inst);
		return i->getParent()->getParent();
	}
}

/**
 * Checks if this DefUseInstruction represents a function entry.
 * 
 * @return True if this is a function entry, false otherwise
 */
bool DefUseInstruction::isEntryInstruction() const
{
	return isa<Function>(inst);
}

/**
 * Checks if this DefUseInstruction represents a call instruction.
 * 
 * @return True if this is a call or invoke instruction, false otherwise
 */
bool DefUseInstruction::isCallInstruction() const
{
	return isa<CallInst>(inst) || isa<InvokeInst>(inst);
}

/**
 * Checks if this DefUseInstruction represents a return instruction.
 * 
 * @return True if this is a return instruction, false otherwise
 */
bool DefUseInstruction::isReturnInstruction() const
{
	return isa<ReturnInst>(inst);
}

/**
 * Retrieves the DefUseInstruction for a given LLVM Instruction.
 * This is a non-const version that can be used for modifications.
 * 
 * @param inst The LLVM Instruction to look up
 * @return Pointer to the corresponding DefUseInstruction, or nullptr if not found
 */
DefUseInstruction* DefUseFunction::getDefUseInstruction(const Instruction* inst)
{
	auto itr = instMap.find(inst);
	if (itr != instMap.end())
		return &itr->second;
	else
		return nullptr;
}

/**
 * Retrieves the DefUseInstruction for a given LLVM Instruction.
 * This is a const version for read-only access.
 * 
 * @param inst The LLVM Instruction to look up
 * @return Pointer to the corresponding DefUseInstruction, or nullptr if not found
 */
const DefUseInstruction* DefUseFunction::getDefUseInstruction(const Instruction* inst) const
{
	auto itr = instMap.find(inst);
	if (itr != instMap.end())
		return &itr->second;
	else
		return nullptr;
}

/**
 * Constructor for DefUseFunction that builds a def-use graph for a function.
 * Creates DefUseInstructions for each instruction in the function and
 * establishes the entry and exit points.
 * 
 * @param f The LLVM Function to analyze
 */
DefUseFunction::DefUseFunction(const Function& f): function(f), entryInst(&f), exitInst(nullptr)
{
	for (auto const& bb: f)
	{
		for (auto const& inst: bb)
		{
			if (auto brInst = dyn_cast<BranchInst>(&inst))
				if (brInst->isUnconditional())
					continue;

			auto itr = instMap.emplace(
				std::piecewise_construct,
				std::forward_as_tuple(&inst),
				std::forward_as_tuple(inst)
			).first;

			if (isa<ReturnInst>(&inst))
			{
				if (exitInst == nullptr)
					exitInst = &itr->second;
				else
					llvm_unreachable("Multiple return inst detected!");
			}
		}
	}
}

/**
 * Constructor for DefUseModule that builds a module-level def-use graph.
 * Creates DefUseFunctions for each function in the module and identifies
 * the entry function (main).
 * 
 * @param m The LLVM Module to analyze
 */
DefUseModule::DefUseModule(const Module& m): module(m), entryFunc(nullptr)
{
	for (auto const& f: module)
	{
		if (f.isDeclaration())
			continue;

		auto itr = funMap.emplace(
			std::piecewise_construct,
			std::forward_as_tuple(&f),
			std::forward_as_tuple(f)
		).first;

		if (f.getName() == "main")
		{
			assert(entryFunc == nullptr);
			entryFunc = &itr->second;
		}
	}
}

/**
 * Gets the DefUseFunction for a given LLVM Function.
 * 
 * @param f The LLVM Function to look up
 * @return Reference to the corresponding DefUseFunction
 */
const DefUseFunction& DefUseModule::getDefUseFunction(const Function* f) const
{
	auto itr = funMap.find(f);
	assert(itr != funMap.end());
	return itr->second;
}

}
