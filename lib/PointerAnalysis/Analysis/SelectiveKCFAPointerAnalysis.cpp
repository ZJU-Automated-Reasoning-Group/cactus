#include "PointerAnalysis/Analysis/SelectiveKCFAPointerAnalysis.h"
#include "PointerAnalysis/Analysis/SelectiveKCFAExample.h"

#include <llvm/IR/Module.h>
#include <llvm/IR/Function.h>

namespace tpa
{

void SelectiveKCFAPointerAnalysis::setupSelectiveKCFA(const llvm::Module* module)
{
    // Set a basic default configuration
    context::SelectiveKCFA::setDefaultLimit(1);  // Default k = 1
    
    // We can use our example configuration methods
    // Uncomment one of these to use a specific strategy
    
    // Basic configuration with simple heuristics
    SelectiveKCFAExample::configureSelectiveKCFA(module);
    
    // Or use a more complex configuration based on function size, allocation sites, etc.
    // SelectiveKCFAExample::configureKLimitsByHeuristic(module);
    
    // Or configuration based on call frequency
    // SelectiveKCFAExample::configureKLimitsByCallFrequency(module);
    
    // You could also add custom configuration here
    // For example, set k=3 for main and any function called directly from main
    /*
    for (const llvm::Function& F : *module) {
        if (F.getName() == "main") {
            context::SelectiveKCFA::setKLimitForFunctionCallSites(&F, 3);
            
            // Identify functions called from main and set higher k
            for (const auto& BB : F) {
                for (const auto& I : BB) {
                    if (const llvm::CallInst* callInst = llvm::dyn_cast<llvm::CallInst>(&I)) {
                        if (const llvm::Function* callee = callInst->getCalledFunction()) {
                            if (!callee->isDeclaration()) {
                                // Higher sensitivity for functions called from main
                                context::SelectiveKCFA::setKLimitForFunctionCallSites(callee, 2);
                            }
                        }
                    }
                }
            }
        }
    }
    */
}

} // namespace tpa 