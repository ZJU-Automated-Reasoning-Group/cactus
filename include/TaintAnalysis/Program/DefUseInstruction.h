#pragma once

#include "Util/DataStructure/VectorSet.h"
#include "Util/Iterator/IteratorRange.h"

#include <cassert>
#include <unordered_map>

namespace llvm
{
	class Function;
	class Instruction;
	class Value;
}

namespace tpa
{
	class MemoryObject;
}

namespace taint
{

/**
 * @class DefUseInstruction
 * @brief Represents a node in the def-use graph for taint analysis.
 *
 * This class encapsulates an LLVM instruction (or a function entry point) as a node
 * in the def-use graph. It tracks both top-level (direct value) edges and memory-level
 * edges between instructions. The former represent direct value flow via instruction
 * operands, while the latter represent flow through memory operations.
 *
 * Each node has a priority based on a reverse post-order traversal of the CFG,
 * which helps optimize the order of propagation during taint analysis.
 */
class DefUseInstruction
{
private:
	// If entry inst, this field stores the function it belongs to
	// Otherwise, this field stores the corresponding llvm instruction
	const llvm::Value* inst;  ///< The underlying LLVM Value (either Function or Instruction)
	
	size_t rpo;  ///< The reverse postorder number of this node (used for priority)

	using NodeSet = util::VectorSet<DefUseInstruction*>;
	NodeSet topSucc, topPred;  ///< Sets of top-level (direct value) successors and predecessors

	using NodeMap = std::unordered_map<const tpa::MemoryObject*, NodeSet>;
	NodeMap memSucc, memPred;  ///< Maps from memory objects to sets of memory-level successors and predecessors

	/**
	 * @brief Private constructor for creating a function entry node
	 * @param f The LLVM Function for which to create an entry node
	 */
	DefUseInstruction(const llvm::Function* f);
public:
	using iterator = NodeSet::iterator;
	using const_iterator = NodeSet::const_iterator;
	using mem_succ_iterator = NodeMap::iterator;
	using const_mem_succ_iterator = NodeMap::const_iterator;

	/**
	 * @brief Constructs a DefUseInstruction for an LLVM Instruction
	 * @param i The LLVM Instruction to wrap
	 */
	DefUseInstruction(const llvm::Instruction& i);
	
	/**
	 * @brief Gets the underlying LLVM Instruction
	 * @return Pointer to the wrapped LLVM Instruction
	 */
	const llvm::Instruction* getInstruction() const;

	/**
	 * @brief Gets the LLVM Function containing this instruction
	 * @return Pointer to the containing LLVM Function
	 */
	const llvm::Function* getFunction() const;
	
	/**
	 * @brief Checks if this DefUseInstruction represents a function entry
	 * @return True if this is a function entry, false otherwise
	 */
	bool isEntryInstruction() const;
	
	/**
	 * @brief Checks if this DefUseInstruction represents a call instruction
	 * @return True if this is a call or invoke instruction, false otherwise
	 */
	bool isCallInstruction() const;
	
	/**
	 * @brief Checks if this DefUseInstruction represents a return instruction
	 * @return True if this is a return instruction, false otherwise
	 */
	bool isReturnInstruction() const;

	/**
	 * @brief Gets the priority of this node for the worklist algorithm
	 * @return The priority value (lower values are processed first)
	 */
	size_t getPriority() const
	{
		assert(rpo != 0);
		return rpo;
	}
	
	/**
	 * @brief Sets the priority of this node
	 * @param o The priority value to set
	 */
	void setPriority(size_t o)
	{
		assert(o != 0 && "0 cannot be used as a priority num!");
		rpo = o;
	}

	/**
	 * @brief Adds a top-level (direct value) edge to another node
	 * @param node The successor node to add
	 */
	void insertTopLevelEdge(DefUseInstruction* node)
	{
		assert(node != nullptr);
		topSucc.insert(node);
		node->topPred.insert(this);
	}
	
	/**
	 * @brief Adds a memory-level edge via a specific memory object to another node
	 * @param loc The memory object through which the value flows
	 * @param node The successor node to add
	 */
	void insertMemLevelEdge(const tpa::MemoryObject* loc, DefUseInstruction* node)
	{
		assert(loc != nullptr && node != nullptr);
		memSucc[loc].insert(node);
		node->memPred[loc].insert(this);
	}

	/**
	 * @brief Gets all top-level successors
	 * @return A range of top-level successor nodes
	 */
	auto top_succs()
	{
		return util::iteratorRange(topSucc.begin(), topSucc.end());
	}
	
	/**
	 * @brief Gets all top-level successors (const version)
	 * @return A range of top-level successor nodes
	 */
	auto top_succs() const
	{
		return util::iteratorRange(topSucc.begin(), topSucc.end());
	}
	
	/**
	 * @brief Gets all memory-level successors with their associated memory objects
	 * @return A range of pairs mapping memory objects to sets of successor nodes
	 */
	auto mem_succs()
	{
		return util::iteratorRange(memSucc.begin(), memSucc.end());
	}
	
	/**
	 * @brief Gets all memory-level successors with their associated memory objects (const version)
	 * @return A range of pairs mapping memory objects to sets of successor nodes
	 */
	auto mem_succs() const
	{
		return util::iteratorRange(memSucc.begin(), memSucc.end());
	}
	
	/**
	 * @brief Gets memory-level successors for a specific memory object
	 * @param obj The memory object for which to get successors
	 * @return A range of successor nodes for the specified memory object
	 */
	auto mem_succs(const tpa::MemoryObject* obj)
	{
		auto itr = memSucc.find(obj);
		if (itr == memSucc.end())
			return util::iteratorRange(iterator(), iterator());
		else
			return util::iteratorRange(itr->second.begin(), itr->second.end());
	}
	
	/**
	 * @brief Gets memory-level successors for a specific memory object (const version)
	 * @param obj The memory object for which to get successors
	 * @return A range of successor nodes for the specified memory object
	 */
	auto mem_succs(const tpa::MemoryObject* obj) const
	{
		auto itr = memSucc.find(obj);
		if (itr == memSucc.end())
			return util::iteratorRange(const_iterator(), const_iterator());
		else
			return util::iteratorRange(itr->second.begin(), itr->second.end());
	}

	/**
	 * @brief Gets all top-level predecessors
	 * @return A range of top-level predecessor nodes
	 */
	auto top_preds()
	{
		return util::iteratorRange(topPred.begin(), topPred.end());
	}
	
	/**
	 * @brief Gets all top-level predecessors (const version)
	 * @return A range of top-level predecessor nodes
	 */
	auto top_preds() const
	{
		return util::iteratorRange(topPred.begin(), topPred.end());
	}
	
	/**
	 * @brief Gets all memory-level predecessors with their associated memory objects
	 * @return A range of pairs mapping memory objects to sets of predecessor nodes
	 */
	auto mem_preds()
	{
		return util::iteratorRange(memPred.begin(), memPred.end());
	}
	
	/**
	 * @brief Gets all memory-level predecessors with their associated memory objects (const version)
	 * @return A range of pairs mapping memory objects to sets of predecessor nodes
	 */
	auto mem_preds() const
	{
		return util::iteratorRange(memPred.begin(), memPred.end());
	}

	friend class DefUseFunction;
};

}
