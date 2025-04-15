#include "PointerAnalysis/Engine/Strategies/SimpleSelectiveKCFAStrategies.h"

#include <llvm/IR/Module.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/InstVisitor.h>
#include <llvm/IR/InstIterator.h>
#include <llvm/Support/raw_ostream.h>

using namespace llvm;

namespace tpa
{

// A visitor to count different types of instructions in a function
class InstructionCounter : public InstVisitor<InstructionCounter>
{
private:
    unsigned allocationCount = 0;
    unsigned callCount = 0;
    unsigned totalInsts = 0;
    
public:
    void visitAllocaInst(AllocaInst &AI) { allocationCount++; totalInsts++; }
    void visitCallInst(CallInst &CI) { 
        callCount++; 
        totalInsts++;
        
        // Check if this is a memory allocation function
        if (Function* calledFn = CI.getCalledFunction()) {
            StringRef name = calledFn->getName();
            if (name == "malloc" || name == "calloc" || name == "realloc" || 
                name.startswith("_Znw") || name.startswith("_Zna")) {
                allocationCount++;
            }
        }
    }
    void visitInstruction(Instruction &I) { totalInsts++; }
    
    unsigned getAllocationCount() const { return allocationCount; }
    unsigned getCallCount() const { return callCount; }
    unsigned getTotalInsts() const { return totalInsts; }
};

void SelectiveKCFAStrategies::configureBasicStrategy(const Module* module)
{
    // Set default k limit to 1
    context::SelectiveKCFA::setDefaultLimit(1);
    
    // For this strategy, we'll set a higher k limit (3) for functions that are small
    // and a lower k limit (0) for functions that are large or have many call sites
    for (const Function& F : *module) {
        if (F.isDeclaration())
            continue;
            
        // Count instructions and call sites
        InstructionCounter counter;
        counter.visit(const_cast<Function&>(F));
        
        unsigned totalInsts = counter.getTotalInsts();
        unsigned callCount = counter.getCallCount();
        
        if (totalInsts < 50) {
            // Small function - use high k limit
            context::SelectiveKCFA::setKLimitForFunctionCallSites(&F, 3);
        } else if (totalInsts > 200 || callCount > 20) {
            // Large or call-heavy function - use context insensitivity
            context::SelectiveKCFA::setKLimitForFunctionCallSites(&F, 0);
        }
    }
    
    // Also set a high k limit (4) for specific important functions
    context::SelectiveKCFA::setKLimitForCallSitesByName(module, "main", 4);
    context::SelectiveKCFA::setKLimitForCallSitesByName(module, "process.*", 4);
}

void SelectiveKCFAStrategies::configureKLimitsByHeuristic(const Module* module)
{
    // Set a default k limit of 1
    context::SelectiveKCFA::setDefaultLimit(1);
    
    for (const Function& F : *module) {
        if (F.isDeclaration())
            continue;
            
        // Count instructions and allocations
        InstructionCounter counter;
        counter.visit(const_cast<Function&>(F));
        
        unsigned totalInsts = counter.getTotalInsts();
        unsigned allocCount = counter.getAllocationCount();
        
        if (totalInsts < 50) {
            // Small function - high context sensitivity (k=3)
            context::SelectiveKCFA::setKLimitForFunctionCallSites(&F, 3);
        } else if (totalInsts > 200) {
            // Large function - low context sensitivity (k=0)
            context::SelectiveKCFA::setKLimitForFunctionCallSites(&F, 0);
        }
        
        // For allocation-heavy functions, use lower k for allocation sites
        if (allocCount > 5) {
            context::SelectiveKCFA::setKLimitForFunctionAllocSites(&F, 0);
        } else if (allocCount > 0) {
            // Functions with fewer allocations get higher k for precision
            context::SelectiveKCFA::setKLimitForFunctionAllocSites(&F, 2);
        }
    }
}

void SelectiveKCFAStrategies::configureKLimitsByCallFrequency(const Module* module)
{
    // Set a default k limit of 1
    context::SelectiveKCFA::setDefaultLimit(1);
    
    // First, count how many times each function is called
    std::unordered_map<const Function*, unsigned> callFrequency;
    
    for (const Function& F : *module) {
        if (F.isDeclaration())
            continue;
            
        for (const_inst_iterator I = inst_begin(&F), E = inst_end(&F); I != E; ++I) {
            const Instruction* inst = &*I;
            
            if (const CallInst* callInst = dyn_cast<CallInst>(inst)) {
                if (const Function* callee = callInst->getCalledFunction()) {
                    callFrequency[callee]++;
                }
            } else if (const InvokeInst* invokeInst = dyn_cast<InvokeInst>(inst)) {
                if (const Function* callee = invokeInst->getCalledFunction()) {
                    callFrequency[callee]++;
                }
            }
        }
    }
    
    // Now set k values based on call frequency
    for (const auto& pair : callFrequency) {
        const Function* func = pair.first;
        unsigned frequency = pair.second;
        
        if (frequency > 10) {
            // Frequently called function - low context sensitivity to avoid explosion
            context::SelectiveKCFA::setKLimitForFunctionCallSites(func, 0);
        } else if (frequency > 5) {
            // Moderately called - medium sensitivity
            context::SelectiveKCFA::setKLimitForFunctionCallSites(func, 1);
        } else {
            // Rarely called - high sensitivity for precision
            context::SelectiveKCFA::setKLimitForFunctionCallSites(func, 3);
        }
    }
}

void SelectiveKCFAStrategies::printStats(raw_ostream& os)
{
    os << "SelectiveKCFA Strategy Configuration Statistics:\n";
    context::SelectiveKCFA::printStats(os);
}

} // namespace tpa 