#pragma once

#include <set>
#include <vector>
#include <llvm/Analysis/ScalarEvolution.h>
#include <llvm/Analysis/AliasAnalysis.h>
#include <llvm/Analysis/LoopInfo.h>

#include "Alias/Canary/DyckAA/DyckAliasAnalysis.h"
#include "FPAnalysis/CHA/CHA.h"
#include "FPAnalysis/TypeAnalysis.h"
#include "FPAnalysis/SimpleFPAnalysis.h"


using namespace llvm;


class FPAnalysis: public ModulePass{


private:

    Module* M;

    Canary::DyckAliasAnalysis* dyckAA=nullptr;

    TypeAnalysis* typeAA=nullptr;

    SimpleFPAnalysis* simpleAA= nullptr;

    CHGraph* chgraph= nullptr;


    std::map<CallInst*,std::set<llvm::Function*>> iCallResultByFLTA; // first-layer type analysis

    std::map<CallInst*,std::set<llvm::Function*>> iCallResultByMLTA; // multi-layer type analysis

    std::map<CallInst*,std::set<llvm::Function*>> iCallResultBySimpleFP; // precisely top-level function pointers

    std::map<CallInst*,std::set<llvm::Function*>> iCallResultByCanary; // canary's pointer analysis

    std::map<CallInst*,std::set<llvm::Function*>> iCallResultBySyntax; // heuristics

    std::map<CallInst*,std::set<llvm::Function*>> iCallResultByAnderson; // andersen's pointer analysis

    std::map<CallInst*,std::set<llvm::Function*>> iCallResultBySUPA; // SUPA

    std::map<CallInst*,std::set<llvm::Function*>> latestICallResult; // the final results that are dumped to users

    std::map<Function*, std::set<CallInst*>> CalleeToCallerMap;
    std::map<CallInst*, std::set<Function*>> CallerToCalleeMap;

public:
    static char ID;
    static ModulePass* modulePass;

    FPAnalysis();

    virtual ~FPAnalysis(){ };

    virtual bool runOnModule(Module& ) override ;

    virtual void getAnalysisUsage(AnalysisUsage& au) const override;

    virtual const char * getPassName() const override {
        return "Function Pointer Analysis";
    }


    std::set<Function*> getCallee(CallInst*);

    std::set<CallInst*> getCaller(Function*);


    bool isIndirectCall(CallInst*);

    void dumpICallResult();

    void convertICallToCall();

private:

    void init(Module& );

    void performCanaryRefinement();

    //void performAndersenRefinement();

    //void performSUPARefinement();

    void backupUnsoundResults(std::map<CallInst*,std::set<llvm::Function*>>&, std::map<CallInst*,std::set<llvm::Function*>>&);

    std::map<CallInst*,std::set<llvm::Function*>> synBasedRefinement(std::map<CallInst*,std::set<llvm::Function*>>);

    void processingFinalResult(std::map<CallInst*,std::set<llvm::Function*>>);

private:

    // transform

    Function* fakeFunc;

    void convertICMultipleCalleeToCall(CallInst*, std::set<Function*>);

    void convertICSingleCalleeToCall(CallInst*, Function*);

    void convertICallToCallByInlining(CallInst *, std::set<Function*>);


};



