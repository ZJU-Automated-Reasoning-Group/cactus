#pragma once

#include "Context/Context.h"
#include "Context/ProgramPoint.h"

#include <llvm/IR/Instruction.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Module.h>
#include <llvm/Support/raw_ostream.h>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <string>

namespace context
{

class SelectiveKCFA
{
private:
    // Default k limit if no specific value is specified
    static unsigned defaultLimit;
    
    // Maps call sites to their specific k values
    static std::unordered_map<const llvm::Instruction*, unsigned> callSiteKLimits;
    
    // Maps allocation sites to their specific k values
    static std::unordered_map<const llvm::Instruction*, unsigned> allocSiteKLimits;
    
public:
    // Set the default k limit
    static void setDefaultLimit(unsigned k) { defaultLimit = k; }
    
    // Get the default k limit
    static unsigned getDefaultLimit() { return defaultLimit; }
    
    // Set k limit for a specific call site
    static void setCallSiteLimit(const llvm::Instruction* callSite, unsigned k);
    
    // Set k limit for a specific allocation site
    static void setAllocSiteLimit(const llvm::Instruction* allocSite, unsigned k);
    
    // Get k limit for a specific call site (returns default if not set)
    static unsigned getCallSiteLimit(const llvm::Instruction* callSite);
    
    // Get k limit for a specific allocation site (returns default if not set)
    static unsigned getAllocSiteLimit(const llvm::Instruction* allocSite);
    
    // Push context considering the selective k limit for the call site
    static const Context* pushContext(const Context* ctx, const llvm::Instruction* inst);
    
    // Push context from a program point
    static const Context* pushContext(const ProgramPoint& pp);
    
    // Utility functions
    
    // Set k limit for all call sites within a specific function
    static void setKLimitForFunctionCallSites(const llvm::Function* func, unsigned k);
    
    // Set k limit for all allocation sites within a specific function
    static void setKLimitForFunctionAllocSites(const llvm::Function* func, unsigned k);
    
    // Set k limit for call sites that match a pattern
    static void setKLimitForCallSitesByName(const llvm::Module* module, const std::string& pattern, unsigned k);
    
    // Set k limit for all call sites in a list of functions
    static void setKLimitForFunctionsList(const std::vector<const llvm::Function*>& funcs, unsigned k);
    
    // Print statistics about the current configuration
    static void printStats(llvm::raw_ostream& os = llvm::errs());
};

} 