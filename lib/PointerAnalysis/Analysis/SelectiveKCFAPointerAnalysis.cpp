#include "PointerAnalysis/Analysis/SelectiveKCFAPointerAnalysis.h"
#include "PointerAnalysis/Engine/Strategies/SimpleSelectiveKCFAStrategies.h"
#include "PointerAnalysis/Engine/Strategies/IntrospectiveContextSensitivity.h"

#include <llvm/IR/Module.h>
#include <llvm/IR/Function.h>
#include <llvm/Support/CommandLine.h>

namespace tpa
{

// Enum to represent SelectiveKCFA strategy options
enum class SelectiveKCFAStrategy {
    Basic = 0,                // Basic heuristics
    Complex = 1,              // Complex heuristics based on function size
    CallFrequency = 2,        // Call frequency based
    IntrospectiveA = 3,       // Introspective analysis with heuristic A
    IntrospectiveB = 4        // Introspective analysis with heuristic B
};

// Command-line option for SelectiveKCFA strategy
static llvm::cl::opt<unsigned> SelectiveKCFAStrategyOpt(
    "selective-strategy", 
    llvm::cl::desc("SelectiveKCFA strategy selection:\n"
                   "  0 - Basic configuration with simple heuristics (default)\n"
                   "  1 - Complex configuration based on function size and allocation sites\n"
                   "  2 - Configuration based on call frequency analysis\n"
                   "  3 - Introspective analysis with heuristic A (context-insensitive pre-analysis)\n"
                   "  4 - Introspective analysis with heuristic B (context-insensitive pre-analysis)"),
    llvm::cl::init(0)
);

// Command-line options for Introspective heuristic A thresholds
static llvm::cl::opt<unsigned> IntrospectiveKOpt(
    "intro-k", 
    llvm::cl::desc("Threshold K for Introspective heuristic A (pointed-by-vars)"),
    llvm::cl::init(50)
);

static llvm::cl::opt<unsigned> IntrospectiveLOpt(
    "intro-l", 
    llvm::cl::desc("Threshold L for Introspective heuristic A (in-flow)"),
    llvm::cl::init(100)
);

static llvm::cl::opt<unsigned> IntrospectiveMOpt(
    "intro-m", 
    llvm::cl::desc("Threshold M for Introspective heuristic A (max-var-field-points-to)"),
    llvm::cl::init(75)
);

// Command-line options for Introspective heuristic B thresholds
static llvm::cl::opt<unsigned> IntrospectivePOpt(
    "intro-p", 
    llvm::cl::desc("Threshold P for Introspective heuristic B (total-points-to-volume)"),
    llvm::cl::init(200)
);

static llvm::cl::opt<unsigned> IntrospectiveQOpt(
    "intro-q", 
    llvm::cl::desc("Threshold Q for Introspective heuristic B (field-pts-multiplied-by-vars)"),
    llvm::cl::init(5000)
);

void SelectiveKCFAPointerAnalysis::setupSelectiveKCFA(const llvm::Module* module)
{
    // Set a basic default configuration
    context::SelectiveKCFA::setDefaultLimit(1);  // Default k = 1
    
    // Get the strategy from command line options
    SelectiveKCFAStrategy strategy = static_cast<SelectiveKCFAStrategy>(SelectiveKCFAStrategyOpt.getValue());
    
    switch(strategy) {
        case SelectiveKCFAStrategy::Complex:
            // Complex configuration with heuristics
            SelectiveKCFAStrategies::configureKLimitsByHeuristic(module);
            llvm::outs() << "Using SelectiveKCFA with complex heuristics based on function size and allocation sites\n";
            break;
            
        case SelectiveKCFAStrategy::CallFrequency:
            // Configuration based on call frequency
            SelectiveKCFAStrategies::configureKLimitsByCallFrequency(module);
            llvm::outs() << "Using SelectiveKCFA with call frequency heuristics\n";
            break;
            
        case SelectiveKCFAStrategy::IntrospectiveA:
            // Introspective analysis with heuristic A
            {
                // Initialize introspective analysis with heuristic A
                IntrospectiveContextSensitivity introspective;
                
                // Configure thresholds from command line options
                unsigned thresholdK = IntrospectiveKOpt.getValue();
                unsigned thresholdL = IntrospectiveLOpt.getValue();
                unsigned thresholdM = IntrospectiveMOpt.getValue();
                
                // Initialize and configure with custom thresholds 
                introspective.initialize(module, true); // true = use heuristic A
                introspective.configureHeuristicA(thresholdK, thresholdL, thresholdM);
                
                // Print info about the configuration
                llvm::outs() << "Using Introspective analysis with heuristic A:\n";
                llvm::outs() << "  Threshold K (pointed-by-vars): " << thresholdK << "\n";
                llvm::outs() << "  Threshold L (in-flow): " << thresholdL << "\n";
                llvm::outs() << "  Threshold M (max-var-field-points-to): " << thresholdM << "\n";
            }
            break;
            
        case SelectiveKCFAStrategy::IntrospectiveB:
            // Introspective analysis with heuristic B
            {
                // Initialize introspective analysis with heuristic B
                IntrospectiveContextSensitivity introspective;
                
                // Configure thresholds from command line options
                unsigned thresholdP = IntrospectivePOpt.getValue();
                unsigned thresholdQ = IntrospectiveQOpt.getValue();
                
                // Initialize and configure with custom thresholds
                introspective.initialize(module, false); // false = use heuristic B
                introspective.configureHeuristicB(thresholdP, thresholdQ);
                
                // Print info about the configuration
                llvm::outs() << "Using Introspective analysis with heuristic B:\n";
                llvm::outs() << "  Threshold P (total-points-to-volume): " << thresholdP << "\n";
                llvm::outs() << "  Threshold Q (field-pts-multiplied-by-vars): " << thresholdQ << "\n";
            }
            break;
            
        case SelectiveKCFAStrategy::Basic:
        default:
            // Basic configuration with simple heuristics
            SelectiveKCFAStrategies::configureBasicStrategy(module);
            llvm::outs() << "Using SelectiveKCFA with basic heuristics\n";
            break;
    }
}

} // namespace tpa 