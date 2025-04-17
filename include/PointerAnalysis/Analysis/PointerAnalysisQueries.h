#pragma once

#include "PointerAnalysis/Analysis/PointerAnalysis.h"
#include "PointerAnalysis/MemoryModel/MemoryManager.h"
#include "PointerAnalysis/MemoryModel/PointerManager.h"
#include "PointerAnalysis/MemoryModel/Pointer.h"
#include "PointerAnalysis/MemoryModel/MemoryObject.h"
#include "PointerAnalysis/Support/PtsSet.h"
#include "Context/Context.h"
#include <llvm/IR/CallSite.h>
#include <llvm/Support/raw_ostream.h>

#include <vector>
#include <set>

namespace llvm
{
    class Value;
}

namespace context
{
    class Context;
}

namespace tpa
{

/**
 * PointerAnalysisQueries provides a collection of interfaces for querying the results
 * of pointer analysis. This includes points-to queries, alias analysis queries,
 * and other related queries.
 */
class PointerAnalysisQueries
{
private:
    const PointerManager& ptrManager;
    const MemoryManager& memManager;
    
    // Function to get points-to set for a pointer (internal implementation)
    virtual PtsSet getPtsSetForPointer(const Pointer* ptr) const = 0;

public:
    PointerAnalysisQueries(const PointerManager& pm, const MemoryManager& mm)
        : ptrManager(pm), memManager(mm) {}
    
    virtual ~PointerAnalysisQueries() = default;
    
    /**
     * (1) Points-to queries - what does p point to?
     */
    
    // Get the points-to set for a pointer (identified by context and LLVM value)
    PtsSet getPointsToSet(const context::Context* ctx, const llvm::Value* val) const;
    
    // Get the points-to set for a value (context-insensitive)
    PtsSet getPointsToSet(const llvm::Value* val) const;
    
    // Get the points-to set for a pointer
    PtsSet getPointsToSet(const Pointer* ptr) const;
    
    // Check if a pointer may point to a specific memory object
    bool mayPointTo(const Pointer* ptr, const MemoryObject* obj) const;
    
    // Check if a pointer may point to a specific memory object
    bool mayPointTo(const context::Context* ctx, const llvm::Value* val, const MemoryObject* obj) const;
    
    /**
     * (2) Alias pair queries - do p and q alias?
     */
    
    // Check if two pointers may alias (i.e., may point to the same memory object)
    bool mayAlias(const Pointer* ptr1, const Pointer* ptr2) const;
    
    // Check if two pointers may alias
    bool mayAlias(const context::Context* ctx1, const llvm::Value* val1, 
                 const context::Context* ctx2, const llvm::Value* val2) const;
    
    // Check if two values may alias (context-insensitive)
    bool mayAlias(const llvm::Value* val1, const llvm::Value* val2) const;
    
    /**
     * (3) Pointed-by queries - what pointers point to this object?
     */
    
    // Get all pointers that may point to a specific memory object
    std::vector<const Pointer*> getPointedBy(const MemoryObject* obj) const;
    
    // Get all values that may point to a specific memory object (context-insensitive)
    std::vector<const llvm::Value*> getPointedByValues(const MemoryObject* obj) const;
    
    /**
     * (4) Alias set queries - what is the set of variables that may alias with p?
     */
    
    // Get all pointers that may alias with a specific pointer
    std::vector<const Pointer*> getAliasSet(const Pointer* ptr) const;
    
    // Get all values that may alias with a specific value (context-insensitive)
    std::vector<const llvm::Value*> getAliasSetValues(const llvm::Value* val) const;
    
    // Get all aliasing pairs of pointers from a set of pointers
    std::vector<std::pair<const Pointer*, const Pointer*>> getAllAliasingPairs(
        const std::vector<const Pointer*>& pointers) const;

    // For a call site, what functions might be called?
    virtual std::vector<const llvm::Function*> getCallees(const llvm::ImmutableCallSite& cs, const context::Context* ctx = nullptr) const = 0;
};

/**
 * Implementation of the PointerAnalysisQueries interface that delegates to a specific
 * pointer analysis implementation
 */
template <typename PointerAnalysisType>
class PointerAnalysisQueriesImpl : public PointerAnalysisQueries
{
private:
    const PointerAnalysisType& analysis;

    // Implementation of abstract method
    PtsSet getPtsSetForPointer(const Pointer* ptr) const
    {
        return analysis.getPtsSet(ptr);
    }

public:
    PointerAnalysisQueriesImpl(const PointerAnalysisType& pa)
        : PointerAnalysisQueries(pa.getPointerManager(), pa.getMemoryManager()),
          analysis(pa) {}

    PtsSet getPtsSet(const context::Context* ctx, const llvm::Value* val) const
    {
        return analysis.getPtsSet(ctx, val);
    }

    std::vector<const llvm::Function*> getCallees(const llvm::ImmutableCallSite& cs, const context::Context* ctx = nullptr) const
    {
        // Debug output to verify context sensitivity being used
        static size_t callCount = 0;
        if (callCount < 5) {
            llvm::errs() << "DEBUG: getCallees with context depth=" 
                        << (ctx ? ctx->size() : 0) << " at "
                        << *cs.getInstruction() << "\n";
            callCount++;
        }
        
        return analysis.getCallees(cs, ctx);
    }
};

} // namespace tpa 