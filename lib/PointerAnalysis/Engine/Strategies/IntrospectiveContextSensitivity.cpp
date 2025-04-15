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
#include "PointerAnalysis/MemoryModel/MemoryManager.h"
#include "PointerAnalysis/MemoryModel/PointerManager.h"

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

// Constructor
IntrospectiveContextSensitivity::IntrospectiveContextSensitivity()
    : module(nullptr), ptrManager(nullptr), memManager(nullptr), canaryAA(nullptr)
{
}

// Destructor
IntrospectiveContextSensitivity::~IntrospectiveContextSensitivity()
{
    // Clean up resources
    delete ptrManager;
    delete memManager;
    delete canaryAA;
}

// Implementation of IntrospectiveContextSensitivity methods from the header
void IntrospectiveContextSensitivity::setupPreAnalysis(const llvm::Module* m)
{
    module = m;
    
    // Create and run Canary's context-insensitive DyckAliasAnalysis
    canaryAA = new Canary::DyckAliasAnalysis();
    canaryAA->performDyckAliasAnalysis(*const_cast<Module*>(m));
    
    // Create pointer and memory managers
    ptrManager = new PointerManager();
    memManager = new MemoryManager();
    
    // Create a queries interface using Canary's analysis
    preAnalysisQueries = std::make_unique<CanaryPointerAnalysisAdapter>(
        canaryAA, *ptrManager, *memManager);

    // Check if the pre-analysis was successful
    llvm::outs() << "Introspective pre-analysis using Canary completed.\n";
    if (preAnalysisQueries) {
        llvm::outs() << "PointerAnalysisQueries interface created successfully.\n";
    } else {
        llvm::errs() << "Warning: Failed to create PointerAnalysisQueries interface.\n";
    }
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
        llvm::outs() << "Using Heuristic A with thresholds: K=50, L=100, M=75\n";
    } else {
        // Default thresholds for Heuristic B
        IntrospectiveSelectiveKCFA::setHeuristicBThresholds(200, 5000);
        llvm::outs() << "Using Heuristic B with thresholds: P=200, Q=5000\n";
    }
    
    // Compute metrics from the pre-analysis
    if (preAnalysisQueries) {
        llvm::outs() << "Computing metrics from pre-analysis results...\n";
        IntrospectiveSelectiveKCFA::computeMetricsFromPreAnalysis(*preAnalysisQueries, *module);
        
        // Apply the heuristics to decide what to refine
        llvm::outs() << "Applying heuristics to decide which call sites and allocation sites to refine...\n";
        IntrospectiveSelectiveKCFA::applyHeuristics();
        
        // Print metrics statistics
        IntrospectiveSelectiveKCFA::printMetricsStats(llvm::errs());
        
        llvm::outs() << "Introspective analysis completed successfully.\n";
    } else {
        llvm::errs() << "Error: Pre-analysis did not complete successfully. Cannot compute metrics.\n";
    }
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