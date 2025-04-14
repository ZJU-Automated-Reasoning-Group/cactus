#pragma once

#include "Context/IntrospectiveSelectiveKCFA.h"
#include "PointerAnalysis/Engine/ContextSensitivity.h"
#include "PointerAnalysis/Analysis/PointerAnalysisQueries.h"

#include <llvm/IR/Module.h>
#include <memory>

namespace tpa
{

// Forward declaration 
class PointerAnalysisQueries;

/**
 * IntrospectiveContextSensitivity - A utility class that runs a context-insensitive
 * pre-analysis and then configures context-sensitive analysis using the 
 * IntrospectiveSelectiveKCFA approach with heuristics.
 */
class IntrospectiveContextSensitivity
{
private:
    // The pointer analysis queries interface for the pre-analysis
    std::unique_ptr<PointerAnalysisQueries> preAnalysisQueries;
    
    // Module being analyzed
    const llvm::Module* module;
    
    // Setup a context-insensitive analysis
    void setupPreAnalysis(const llvm::Module* m);
    
public:
    IntrospectiveContextSensitivity() = default;
    
    // Initialize and run the introspective selective k-CFA approach
    void initialize(const llvm::Module* m, bool useHeuristicA = true);
    
    // Configure custom thresholds for heuristic A
    void configureHeuristicA(unsigned K, unsigned L, unsigned M);
    
    // Configure custom thresholds for heuristic B
    void configureHeuristicB(unsigned P, unsigned Q);
};

} // namespace tpa 