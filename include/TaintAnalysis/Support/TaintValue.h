#pragma once

#include "Util/Hashing.h"

namespace context
{
	class Context;
}

namespace llvm
{
	class Value;
}

namespace taint
{

/**
 * @class TaintValue
 * @brief Represents a context-sensitive LLVM value for taint tracking.
 *
 * TaintValue combines an LLVM Value with its execution context to enable
 * context-sensitive taint analysis. This is critical for tracking how data
 * flows through the program with respect to different calling contexts.
 * The class provides comparison operators to support use as a key in maps
 * and sets, and a hash function specialization for use in unordered containers.
 */
class TaintValue
{
private:
	const context::Context* context;  ///< The execution context
	const llvm::Value* value;         ///< The LLVM value

	using PairType = std::pair<const context::Context*, const llvm::Value*>;
public:
	/**
	 * @brief Constructs a TaintValue from separate context and value pointers
	 * @param c The execution context
	 * @param v The LLVM value
	 */
	TaintValue(const context::Context* c, const llvm::Value* v): context(c), value(v) {}
	
	/**
	 * @brief Constructs a TaintValue from a pair of context and value pointers
	 * @param p A pair containing the context and value
	 */
	TaintValue(const PairType& p): context(p.first), value(p.second) {}

	/**
	 * @brief Gets the execution context
	 * @return The context pointer
	 */
	const context::Context* getContext() const { return context; }
	
	/**
	 * @brief Gets the LLVM value
	 * @return The value pointer
	 */
	const llvm::Value* getValue() const { return value; }

	/**
	 * @brief Equality comparison operator
	 * @param other The TaintValue to compare with
	 * @return true if both context and value are equal
	 */
	bool operator==(const TaintValue& other) const
	{
		return (context == other.context) && (value == other.value);
	}
	
	/**
	 * @brief Inequality comparison operator
	 * @param other The TaintValue to compare with
	 * @return true if either context or value differs
	 */
	bool operator!=(const TaintValue& other) const
	{
		return !(*this == other);
	}
	
	/**
	 * @brief Less-than comparison operator for ordering
	 * @param rhs The right-hand side TaintValue to compare with
	 * @return true if this value should be ordered before rhs
	 */
	bool operator<(const TaintValue& rhs) const
	{
		if (context < rhs.context)
			return true;
		else if (rhs.context < context)
			return false;
		else
			return value < rhs.value;
	}
	
	/**
	 * @brief Greater-than comparison operator for ordering
	 * @param rhs The right-hand side TaintValue to compare with
	 * @return true if this value should be ordered after rhs
	 */
	bool operator>(const TaintValue& rhs) const
	{
		return rhs < *this;
	}
	
	/**
	 * @brief Less-than-or-equal comparison operator
	 * @param rhs The right-hand side TaintValue to compare with
	 * @return true if this value should be ordered before or equal to rhs
	 */
	bool operator<=(const TaintValue& rhs) const
	{
		return !(rhs < *this);
	}
	
	/**
	 * @brief Greater-than-or-equal comparison operator
	 * @param rhs The right-hand side TaintValue to compare with
	 * @return true if this value should be ordered after or equal to rhs
	 */
	bool operator>=(const TaintValue& rhs) const
	{
		return !(*this < rhs);
	}

	/**
	 * @brief Conversion operator to pair of context and value
	 * @return A pair containing the context and value
	 */
	operator PairType() const
	{
		return std::make_pair(context, value);
	}
};

}

namespace std
{
	/**
	 * @brief Hash function specialization for TaintValue
	 *
	 * Enables TaintValue to be used as a key in unordered containers
	 * by providing a hash function that combines the hashes of the
	 * context and value pointers.
	 */
	template<> struct hash<taint::TaintValue>
	{
		size_t operator()(const taint::TaintValue& tv) const
		{
			return util::hashPair(tv.getContext(), tv.getValue());	
		}
	};
}
