#pragma once

#include "Context/SelectiveKCFA.h"
#include "PointerAnalysis/Analysis/PointerAnalysis.h"

namespace llvm
{
    class Module;
    class Function;
}

namespace tpa
{

// Example class demonstrating how to configure and use SelectiveKCFA
class SelectiveKCFAExample
{
public:
    // Configure SelectiveKCFA for a module
    static void configureSelectiveKCFA(const llvm::Module* module);
    
    // Configure k limits based on a heuristic: higher k for small functions,
    // lower k for large functions, and even lower k for allocation-heavy functions
    static void configureKLimitsByHeuristic(const llvm::Module* module);
    
    // Configure k limits based on call site frequency
    static void configureKLimitsByCallFrequency(const llvm::Module* module);
    
    // Example usage in pointer analysis
    template <typename PtrAnalysisType>
    static void runPointerAnalysisWithSelectiveKCFA(const llvm::Module* module)
    {
        // Configure selective k-CFA
        configureSelectiveKCFA(module);
        
        // Create and initialize pointer analysis
        PtrAnalysisType ptrAnalysis;
        
        // Instead of using KLimitContext::pushContext, use SelectiveKCFA::pushContext
        // This requires modifying the transfer functions to use SelectiveKCFA
        
        // Run pointer analysis using SelectiveKCFA context-sensitivity
        ptrAnalysis.runOnModule(*module);
        
        // Print stats
        context::SelectiveKCFA::printStats();
    }
};

} // namespace tpa 