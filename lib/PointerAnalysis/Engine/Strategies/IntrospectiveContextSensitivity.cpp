/*
 * IntrospectiveContextSensitivity.cpp
 *
 * Implementation of a utility class to run context-insensitive pre-analysis
 * and then configure a context-sensitive analysis using the IntrospectiveSelectiveKCFA approach.
 */

#include "PointerAnalysis/Engine/Strategies/IntrospectiveContextSensitivity.h"
#include "Context/IntrospectiveSelectiveKCFA.h"
#include "PointerAnalysis/Engine/ContextSensitivity.h"
#include "PointerAnalysis/Analysis/PointerAnalysisQueries.h"

#include "FPAnalysis/Canary/DyckAA/DyckAliasAnalysis.h"
#include "FPAnalysis/Canary/DyckAA/AAAnalyzer.h"
#include "FPAnalysis/TypeAnalysis.h"

#include <llvm/IR/Module.h>
#include <llvm/Support/raw_ostream.h>
#include <memory>

using namespace llvm;
using namespace context;

namespace tpa
{

// Forward declarations for wrapper class
class PointerManager;
class MemoryManager;

// A specialized PointerAnalysisQueries adapter that uses Canary's DyckAA
class CanaryPointerAnalysisAdapter : public PointerAnalysisQueries
{
private:
    Canary::DyckAliasAnalysis* dyckAA;
    PointerManager& ptrManager;
    MemoryManager& memManager;
    
public:
    CanaryPointerAnalysisAdapter(Canary::DyckAliasAnalysis* aa, 
                              PointerManager& pm, 
                              MemoryManager& mm)
        : PointerAnalysisQueries(pm, mm), dyckAA(aa), ptrManager(pm), memManager(mm) {}
    
    virtual ~CanaryPointerAnalysisAdapter() = default;
    
    virtual PtsSet getPtsSetForPointer(const Pointer* ptr) const override
    {
        // Return empty set for null pointers
        if (!ptr) return PtsSet::getEmptySet();
        
        // Get the value this pointer represents
        const Value* val = ptr->getValue();
        if (!val) return PtsSet::getEmptySet();
        
        // Query Canary's alias analysis to get points-to objects
        std::set<Value*> objects;
        dyckAA->getPointstoObjects(objects, const_cast<Value*>(val));
        
        // If the set is empty, return an empty PtsSet
        if (objects.empty())
            return PtsSet::getEmptySet();
            
        // Process each object in the points-to set
        PtsSet result = PtsSet::getEmptySet();
        for (Value* obj : objects) {
            // Find or create corresponding memory object in our representation
            if (const MemoryObject* memObj = memManager.allocateGlobalMemory(dyn_cast<GlobalVariable>(obj), nullptr)) {
                result = result.insert(memObj);
            }
            // If we can't find a corresponding memory object, add universal object
            else if (!result.has(MemoryManager::getUniversalObject())) {
                result = result.insert(MemoryManager::getUniversalObject());
            }
        }
        
        return result;
    }
};

// Implementation of IntrospectiveContextSensitivity methods from the header
void IntrospectiveContextSensitivity::setupPreAnalysis(const llvm::Module* m)
{
    module = m;
    
    // Create and run Canary's context-insensitive DyckAliasAnalysis
    Canary::DyckAliasAnalysis* canaryAA = new Canary::DyckAliasAnalysis();
    canaryAA->performDyckAliasAnalysis(*const_cast<Module*>(m));
    
    // Create a queries interface using Canary's analysis
    // This is a simplified implementation that would need to be expanded with real instances
    // of pointer and memory managers in a production environment
    
    // In the real implementation, create adapter with actual managers
    // preAnalysisQueries = std::make_unique<CanaryPointerAnalysisAdapter>(canaryAA, ptrManager, memManager);
}

void IntrospectiveContextSensitivity::initialize(const llvm::Module* m, bool useHeuristicA)
{
    // Setup and run the pre-analysis
    setupPreAnalysis(m);
    
    // Set which heuristic to use
    IntrospectiveSelectiveKCFA::setHeuristic(useHeuristicA);
    
    // Use default thresholds or customize them
    if (useHeuristicA) {
        // Default thresholds for Heuristic A
        IntrospectiveSelectiveKCFA::setHeuristicAThresholds(50, 100, 75);
    } else {
        // Default thresholds for Heuristic B
        IntrospectiveSelectiveKCFA::setHeuristicBThresholds(200, 5000);
    }
    
    // Compute metrics from the pre-analysis
    if (preAnalysisQueries) {
        IntrospectiveSelectiveKCFA::computeMetricsFromPreAnalysis(*preAnalysisQueries, *module);
        
        // Apply the heuristics to decide what to refine
        IntrospectiveSelectiveKCFA::applyHeuristics();
        
        // Print metrics statistics
        IntrospectiveSelectiveKCFA::printMetricsStats(llvm::errs());
    }
    
    // Set up ContextSensitivityPolicy to use the configured SelectiveKCFA
    // This depends on the specific implementation in the codebase
    // tpa::ContextSensitivityPolicy::setPolicy(tpa::ContextSensitivityPolicy::Policy::SelectiveKCFA);
}

void IntrospectiveContextSensitivity::configureHeuristicA(unsigned K, unsigned L, unsigned M)
{
    IntrospectiveSelectiveKCFA::setHeuristicAThresholds(K, L, M);
}

void IntrospectiveContextSensitivity::configureHeuristicB(unsigned P, unsigned Q)
{
    IntrospectiveSelectiveKCFA::setHeuristicBThresholds(P, Q);
}

} // namespace tpa 