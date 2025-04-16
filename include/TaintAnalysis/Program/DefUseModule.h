#pragma once

#include "TaintAnalysis/Program/DefUseFunction.h"
#include "Util/Iterator/MapValueIterator.h"

namespace llvm
{
	class Module;
}

namespace taint
{

/**
 * @class DefUseModule
 * @brief Represents an entire LLVM Module as a collection of def-use graphs.
 *
 * This class encapsulates an LLVM Module and maintains a def-use graph for each
 * function in the module. It provides access to the entry function (main) and
 * supports iteration over all functions in the module.
 */
class DefUseModule
{
private:
	const llvm::Module& module;  ///< The underlying LLVM Module
	
	using FunctionMap = std::unordered_map<const llvm::Function*, DefUseFunction>;
	FunctionMap funMap;  ///< Map from LLVM Functions to DefUseFunctions
	
	const DefUseFunction* entryFunc;  ///< Pointer to the entry function (main)
public:
	using iterator = util::MapValueIterator<FunctionMap::iterator>;
	using const_iterator = util::MapValueConstIterator<FunctionMap::const_iterator>;

	/**
	 * @brief Constructs a DefUseModule for an LLVM Module
	 * @param m The LLVM Module to represent
	 */
	DefUseModule(const llvm::Module& m);
	
	/**
	 * @brief Gets the underlying LLVM Module
	 * @return Reference to the LLVM Module
	 */
	const llvm::Module& getModule() const { return module; }

	/**
	 * @brief Gets the entry function (main) as a DefUseFunction
	 * @return Reference to the entry function's DefUseFunction
	 */
	const DefUseFunction& getEntryFunction() const
	{
		assert(entryFunc != nullptr);
		return *entryFunc;
	};
	
	/**
	 * @brief Gets the DefUseFunction for an LLVM Function
	 * @param f The LLVM Function to look up
	 * @return Reference to the corresponding DefUseFunction
	 */
	const DefUseFunction& getDefUseFunction(const llvm::Function* f) const;

	/**
	 * @brief Gets an iterator to the beginning of the function collection
	 * @return Iterator pointing to the first DefUseFunction
	 */
	iterator begin() { return iterator(funMap.begin()); }
	
	/**
	 * @brief Gets an iterator to the end of the function collection
	 * @return Iterator pointing past the last DefUseFunction
	 */
	iterator end() { return iterator(funMap.end()); }
	
	/**
	 * @brief Gets a const iterator to the beginning of the function collection
	 * @return Const iterator pointing to the first DefUseFunction
	 */
	const_iterator begin() const { return const_iterator(funMap.begin()); }
	
	/**
	 * @brief Gets a const iterator to the end of the function collection
	 * @return Const iterator pointing past the last DefUseFunction
	 */
	const_iterator end() const { return const_iterator(funMap.end()); }
};

}
