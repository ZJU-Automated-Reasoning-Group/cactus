/*
 * SelectiveKCFA.cpp
 *
 * Selective k-CFA approach that allows different k values for different call sites
 * and allocation sites.
 */

#include "Context/SelectiveKCFA.h"
#include "Context/ProgramPoint.h"

#include <llvm/IR/Instructions.h>
#include <llvm/IR/InstIterator.h>
#include <regex>

using namespace llvm;

namespace context
{

// Static members are defined in StaticFields.cpp
// No need to define them here again to avoid duplicate symbols

void SelectiveKCFA::setCallSiteLimit(const Instruction* callSite, unsigned k)
{
    callSiteKLimits[callSite] = k;
}

void SelectiveKCFA::setAllocSiteLimit(const Instruction* allocSite, unsigned k)
{
    allocSiteKLimits[allocSite] = k;
}

unsigned SelectiveKCFA::getCallSiteLimit(const Instruction* callSite)
{
    auto it = callSiteKLimits.find(callSite);
    return (it != callSiteKLimits.end()) ? it->second : defaultLimit;
}

unsigned SelectiveKCFA::getAllocSiteLimit(const Instruction* allocSite)
{
    auto it = allocSiteKLimits.find(allocSite);
    return (it != allocSiteKLimits.end()) ? it->second : defaultLimit;
}

const Context* SelectiveKCFA::pushContext(const ProgramPoint& pp)
{
    return pushContext(pp.getContext(), pp.getInstruction());
}

const Context* SelectiveKCFA::pushContext(const Context* ctx, const Instruction* inst)
{
    // Determine the k limit to use for this instruction
    unsigned k = getCallSiteLimit(inst);
    
    // Check if the context size would exceed k
    assert(ctx->size() <= k);
    if (ctx->size() == k)
        return ctx;  // Do not add new context element, effectively merging paths
    else
        return Context::pushContext(ctx, inst);  // Add new context element
}

void SelectiveKCFA::setKLimitForFunctionCallSites(const Function* func, unsigned k)
{
    for (const_inst_iterator I = inst_begin(func), E = inst_end(func); I != E; ++I) {
        const Instruction* inst = &*I;
        if (isa<CallInst>(inst) || isa<InvokeInst>(inst)) {
            setCallSiteLimit(inst, k);
        }
    }
}

void SelectiveKCFA::setKLimitForFunctionAllocSites(const Function* func, unsigned k)
{
    for (const_inst_iterator I = inst_begin(func), E = inst_end(func); I != E; ++I) {
        const Instruction* inst = &*I;
        // Look for allocation instructions: malloc, calloc, new, etc.
        if (const CallInst* callInst = dyn_cast<CallInst>(inst)) {
            if (const Function* callee = callInst->getCalledFunction()) {
                StringRef name = callee->getName();
                if (name == "malloc" || name == "calloc" || name == "realloc" || 
                    name == "_Znwm" || name == "_Znam" ||  // new, new[]
                    name == "_Znwj" || name == "_Znaj") {  // new, new[] on 32-bit
                    setAllocSiteLimit(inst, k);
                }
            }
        }
    }
}

void SelectiveKCFA::setKLimitForCallSitesByName(const Module* module, const std::string& pattern, unsigned k)
{
    std::regex nameRegex(pattern);
    
    for (const Function& F : *module) {
        // Skip declarations
        if (F.isDeclaration())
            continue;
            
        for (const_inst_iterator I = inst_begin(&F), E = inst_end(&F); I != E; ++I) {
            const Instruction* inst = &*I;
            
            if (const CallInst* callInst = dyn_cast<CallInst>(inst)) {
                if (const Function* callee = callInst->getCalledFunction()) {
                    std::string calleeName = callee->getName().str();
                    if (std::regex_match(calleeName, nameRegex)) {
                        setCallSiteLimit(inst, k);
                    }
                }
            }
            else if (const InvokeInst* invokeInst = dyn_cast<InvokeInst>(inst)) {
                if (const Function* callee = invokeInst->getCalledFunction()) {
                    std::string calleeName = callee->getName().str();
                    if (std::regex_match(calleeName, nameRegex)) {
                        setCallSiteLimit(inst, k);
                    }
                }
            }
        }
    }
}

void SelectiveKCFA::setKLimitForFunctionsList(const std::vector<const Function*>& funcs, unsigned k)
{
    for (const Function* func : funcs) {
        setKLimitForFunctionCallSites(func, k);
    }
}

void SelectiveKCFA::printStats(raw_ostream& os)
{
    os << "SelectiveKCFA Configuration:\n";
    os << "  Default K limit: " << defaultLimit << "\n";
    os << "  Number of customized call sites: " << callSiteKLimits.size() << "\n";
    os << "  Number of customized allocation sites: " << allocSiteKLimits.size() << "\n";
    
    // Print distribution of k values for call sites
    std::unordered_map<unsigned, unsigned> callSiteKDistribution;
    for (const auto& pair : callSiteKLimits) {
        callSiteKDistribution[pair.second]++;
    }
    
    os << "  Call site K distribution:\n";
    for (const auto& pair : callSiteKDistribution) {
        os << "    K=" << pair.first << ": " << pair.second << " call sites\n";
    }
    
    // Print distribution of k values for allocation sites
    std::unordered_map<unsigned, unsigned> allocSiteKDistribution;
    for (const auto& pair : allocSiteKLimits) {
        allocSiteKDistribution[pair.second]++;
    }
    
    os << "  Allocation site K distribution:\n";
    for (const auto& pair : allocSiteKDistribution) {
        os << "    K=" << pair.first << ": " << pair.second << " allocation sites\n";
    }
}

} 