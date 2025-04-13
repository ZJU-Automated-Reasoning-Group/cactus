#include <set>
#include <vector>
#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include <llvm/Analysis/ScalarEvolution.h>
#include <llvm/Analysis/AliasAnalysis.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/GlobalObject.h>
#include <llvm/IR/Constants.h>
#include "llvm/DebugInfo/DIContext.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/Transforms/Utils/Cloning.h"

#include "Util/SparrowUtils/Common.h"
#include "Util/SparrowUtils/Profiler.h"

#include "FPAnalysis/FunPtrAnalysis.h"
#include "FPAnalysis/TypeAnalysis.h"
#include "FPAnalysis/Steensgaard/DyckAA/DyckAliasAnalysis.h"

static llvm::RegisterPass<FPAnalysis> X("cg", "Function Pointer Analysis", true, false);


static cl::opt<bool> sound_mode("sound-mode",
                                cl::desc("Making sparrow sound by using type analysis as backstopping"), cl::init(true), cl::Hidden);


static cl::opt<bool> EnableSyntaxRefinement( "enable-syntax-refinement",
                                             cl::desc("Enable syntax-based analysis refinement"),cl::init(false), cl::NotHidden);

static cl::opt<bool> EnableAndersenRefinement( "enable-andersen-refinement",
                                             cl::desc("Enable andersen-style refinement"),cl::init(false), cl::NotHidden);

static cl::opt<bool> EnableSUPARefinement( "enable-cs-refinement",
                                               cl::desc("Enable andersen-style refinement"),cl::init(true), cl::NotHidden);





char FPAnalysis::ID=0;

ModulePass* FPAnalysis::modulePass=NULL;

FPAnalysis::FPAnalysis(): ModulePass(ID){
    this->modulePass=this;

}

void FPAnalysis::getAnalysisUsage(AnalysisUsage& au) const{

    /// do not intend to change the IR in this pass,
    au.setPreservesAll();
    au.addRequired<ScalarEvolution>();
    au.addRequired<LoopInfo>();
    au.addRequired<Steensgaard::DyckAliasAnalysis>();
}

void FPAnalysis::init(Module& M) {

    this->M=&M;

    typeAA=TypeAnalysis::getTypeAnalysis(M);
    simpleAA=SimpleFPAnalysis::getSimpleFPAnalysis(M);
    iCallResultByFLTA=typeAA->getFLTAResult();

    iCallResultByMLTA=typeAA->getMLTAResult();

    iCallResultBySimpleFP=simpleAA->getSimpleFPICallResult();

    latestICallResult=iCallResultByMLTA;
    dyckAA=&getAnalysis<Steensgaard::DyckAliasAnalysis>();
    chgraph = new CHGraph();
    chgraph->buildCHG(M);

}

void FPAnalysis::processingFinalResult(std::map<CallInst*,std::set<llvm::Function*>> iCallResults){

    for(auto iter_func=M->begin();iter_func!=M->end();iter_func++){
        Function* func=*&iter_func;
        CalleeToCallerMap[func];
        for(auto iter_inst= inst_begin(*func);iter_inst!=inst_end(*func);iter_inst++){
            Instruction* inst=&*iter_inst;
            if(CallInst* call_inst=dyn_cast<CallInst>(inst)){
                CallSite cs(call_inst);
                CallerToCalleeMap[call_inst];
                if(Function* callee=call_inst->getCalledFunction()){
                    CallerToCalleeMap[call_inst].insert(callee);
                    CalleeToCallerMap[callee].insert(call_inst);
                }else if(Common::isStripFunctionPointerCasts(call_inst)){
                    Value* val=call_inst->getCalledValue()->stripPointerCasts();
                    if(Function* callee= dyn_cast<Function>(val)){
                        CallerToCalleeMap[call_inst].insert(callee);
                        CalleeToCallerMap[callee].insert(call_inst);
                    }
                }else if(Common::isIndirectCallSite(call_inst)){
                    std::set<Function*> callees= latestICallResult[call_inst];
                    for(auto callee_it=callees.begin();callee_it!=callees.end();callee_it++){
                        Function* callee=*callee_it;
                        CallerToCalleeMap[call_inst].insert(callee);
                        CalleeToCallerMap[callee].insert(call_inst);
                    }
                }else if(Common::isVirtualCallSite(call_inst)){
                    std::set<Function*> temp_callees=iCallResults[call_inst];
                    std::set<Value*> cha_callees;
                    chgraph->getCSVFns(cs, cha_callees);
                    for(auto it=cha_callees.begin();it!=cha_callees.end();it++){
                        Value* val=*it;
                        if(Function* callee= dyn_cast<Function>(val)){
                            if(temp_callees.count(callee)){
                                CallerToCalleeMap[call_inst].insert(callee);
                                CalleeToCallerMap[callee].insert(call_inst);
                            }
                        }
                    }
                }
            }
        }
    }

}

std::set<Function*> FPAnalysis::getCallee(CallInst* CI){

    assert(CallerToCalleeMap.find(CI)==CallerToCalleeMap.end());

    return CallerToCalleeMap[CI];

}

std::set<CallInst*> FPAnalysis::getCaller(Function * Func) {

    assert(CalleeToCallerMap.find(Func)==CalleeToCallerMap.end());

    return CalleeToCallerMap[Func];
}


bool FPAnalysis::isIndirectCall(CallInst * CI) {

    if(Function* callee=CI->getCalledFunction()){
        return false;
    }else if(CI->isInlineAsm()){
        return false;
    }else if(Common::isStripFunctionPointerCasts(CI)){
        Value* val=CI->getCalledValue()->stripPointerCasts();
        if(Function* callee= dyn_cast<Function>(val)){
            return true;
        }
    }else if(Common::isIndirectCallSite(CI)){
        return true;
    }else if(Common::isVirtualCallSite(CI)){
        return true;
    }
    return false;
}

bool FPAnalysis::runOnModule(Module& M) {


    init(M);

    performSteensgaardRefinement();

    //TimeMemProfiler.create_snapshot();
    //TimeMemProfiler.print_snapshot_result(outs(), "Indirect Call Resolution");

    outs()<<"Indirect Call Analysis ........Done!\n";

    //processingFinalResult(latestICallResult);

    //convertICallToCall();

    //Common::printICStatistics("[Final Statistics]", latestICallResult);

    return true;
}

void FPAnalysis::performSteensgaardRefinement() {

    dyckAA->performDyckAliasAnalysis(*M);
    iCallResultBySteensgaard=dyckAA->getCanaryFunctionPointerResult();
    if(sound_mode){
        backupUnsoundResults(iCallResultBySteensgaard, latestICallResult);
    }

    if(EnableSyntaxRefinement){
        iCallResultBySteensgaard=synBasedRefinement(iCallResultBySteensgaard);
    }

    latestICallResult=iCallResultBySteensgaard;
}


void FPAnalysis::backupUnsoundResults(std::map<CallInst *, std::set<llvm::Function *>>& succICResult,
                                      std::map<CallInst *, std::set<llvm::Function *>>& preICResult) {

    // if the succeeding results are unsound, using the preceding results as backup

    for(auto it=succICResult.begin();it!=succICResult.end();it++){
        CallInst* CI=it->first;
        std::set<llvm::Function *> callees=it->second;
        if(callees.empty()&&!preICResult[CI].empty()){
            succICResult[CI]=preICResult[CI];
        }
    }

}


void FPAnalysis::convertICallToCall() {


    // create the placeholder function
    FunctionType *NewFTy = FunctionType::get(Type::getInt32Ty(M->getContext()), {Type::getInt32Ty(M->getContext())}, false);
    Function *NewF = Function::Create(NewFTy, Function::ExternalLinkage, "return_all_possible_values", M);
    BasicBlock *NewBB = BasicBlock::Create(M->getContext(), "entry", NewF);
    IRBuilder<> NewBuilder(NewBB);
    Argument *Arg = &*(NewF->arg_begin());
    Value *RetVal = ConstantInt::get(Type::getInt32Ty(M->getContext()), 42); // change this value to adjust the return value
    NewBuilder.CreateRet(RetVal);
    this->fakeFunc=NewF;


    // TODO: transfomation between indirect calls and direct calls may fail when their types are not the same

    for(auto it=latestICallResult.begin();it!=latestICallResult.end();it++){
        CallInst* iCall=it->first;
        std::set<llvm::Function *> calleeSet=it->second;
        if(calleeSet.size()==1){
            convertICSingleCalleeToCall(iCall,*(calleeSet.begin()));
        }else if(!calleeSet.empty()){
            if(iCall->getType() != Type::getVoidTy(M->getContext())){
                convertICMultipleCalleeToCall(iCall, calleeSet);
            }
        }
    }

    // stripped casts
    set<CallInst*> stripped_icalls=typeAA->getAllStrippedICall();
    for(auto it=stripped_icalls.begin();it!=stripped_icalls.end();it++){
        CallInst* icall=*it;
        if(Function* func= dyn_cast<Function>(icall->getCalledValue()->stripPointerCasts())){
            convertICSingleCalleeToCall(icall, func);
        }
    }

}

void FPAnalysis::convertICMultipleCalleeToCall(CallInst * callInst, std::set<Function *> callees){

    // Step 0: prepare

    Function *curFunc = callInst->getParent()->getParent();
    BasicBlock* pred=callInst->getParent();
    LLVMContext &Context = M->getContext();

    std::vector<Value*> Args;
    for(auto it=callInst->arg_operands().begin();it!=callInst->arg_operands().end();it++){
        Args.push_back(*it);
    }

    vector<Function*> knownCallees;

    for(auto it=callees.begin(); it!=callees.end();it++){
        Function* callee=*it;
        FunctionType* calleeType = callee->getFunctionType();
        if(Args.size() != calleeType->getNumParams()){
            continue;
        }
        for (size_t i = 0; i < Args.size(); i++) {
            if ((calleeType->getParamType(i) != Args[i]->getType())) {
                continue;
            }
        }
        if((callee->getType()!=callInst->getType())){
            continue;
        }
        knownCallees.push_back(*it);
    }

    BasicBlock *sub_bb= nullptr;

    if(pred->getTerminator()&&!callInst->isTerminator()){
        sub_bb=pred->splitBasicBlock(callInst, "split_bb");
    }else{
        sub_bb=pred;
    }


    //BasicBlock *sub_bb = pred->splitBasicBlock(callInst, "split_bb");

    // Step 2: Create a direct call to the new function
    std::vector<Value*> fake_args;
    IRBuilder<> EntryBuilder(pred, pred->end());
    Value* real_arg=ConstantInt::get(Type::getInt32Ty(Context), 42);
    fake_args.push_back(real_arg);
    //{ConstantInt::get(Type::getInt32Ty(Context), 0)}
    Value *NewRetVal = EntryBuilder.CreateCall(fakeFunc, fake_args);
    // Step 3: Check whether the return value equals a specific integer and transfer control to the corresponding branches
    std::vector<BasicBlock *> calleeBlocks;
    for (auto &callee : knownCallees) {
        BasicBlock *BB = BasicBlock::Create(Context, "transformed_call", curFunc, pred);
        calleeBlocks.push_back(BB);
    }

    BasicBlock *confluenceBB = BasicBlock::Create(Context, "confluence_br", curFunc);

    for (int i = 0; i < knownCallees.size(); i++) {
        Value *CmpVal = EntryBuilder.CreateICmpEQ(NewRetVal, ConstantInt::get(Type::getInt32Ty(Context), i));
        EntryBuilder.CreateCondBr(CmpVal, calleeBlocks[i], confluenceBB);
        IRBuilder<> Builder(calleeBlocks[i]);
        Builder.CreateBr(confluenceBB);
    }


    std::vector<CallInst *> directCalls;
    for (size_t i = 0; i < knownCallees.size(); i++) {
        Function* callee=knownCallees[i];

        CallInst *call = CallInst::Create(callee, Args, "", calleeBlocks[i]->getTerminator());
        //call->insertBefore(calleeBlocks[i]->getTerminator());
        call->setDebugLoc(callInst->getDebugLoc());
        directCalls.push_back(call);
    }



    if(callInst->getType() != Type::getVoidTy(Context)){
        PHINode *phi = PHINode::Create(callInst->getType(), knownCallees.size(), "call_result", confluenceBB);

        for (size_t i = 0; i < knownCallees.size(); i++) {
            phi->addIncoming(directCalls[i], calleeBlocks[i]);
        }
        CallInst *indirectCall = callInst;
        indirectCall->replaceAllUsesWith(phi);
        indirectCall->eraseFromParent();
    }



    IRBuilder<> Builder(confluenceBB);
    Builder.CreateBr(sub_bb);


    //curFunc->dump();
}

void FPAnalysis::convertICSingleCalleeToCall(CallInst * callInst, Function * callee) {

    //BasicBlock* cur_bb=callInst->getParent();
    // Assume `callInst` is a pointer to the indirect call instruction
    // Assume `callee` is a pointer to the known callee function

    // Get the function type of the callee
    FunctionType* calleeType = callee->getFunctionType();

    std::vector<Value*> Args;
    for(auto it=callInst->arg_operands().begin();it!=callInst->arg_operands().end();it++){
        Args.push_back(*it);
    }

    if(Args.size() != calleeType->getNumParams()){
        return;
    }
    for (size_t i = 0; i < Args.size(); i++) {
        if ((calleeType->getParamType(i) != Args[i]->getType())) {
            return;
        }
    }
    if((callee->getType()!=callInst->getType())){
        return;
    }


    //cur_bb->dump();
    // Create a new direct call instruction that calls the callee
    CallInst* directCallInst = CallInst::Create(callee, Args,"", callInst);

    if (!directCallInst->getType()->isVoidTy()) {
        directCallInst->setName("transformed_call");
    }

    // Preserve debug information
    directCallInst->setDebugLoc(callInst->getDebugLoc());

    if(callInst->getType() != Type::getVoidTy(M->getContext())){
        callInst->replaceAllUsesWith(directCallInst);
    }
    // Replace the indirect call instruction with the direct call instruction

    callInst->eraseFromParent();
    //cur_bb->dump();
}


void FPAnalysis::convertICallToCallByInlining(CallInst * callInst, std::set<Function*> callees) {

    // TODO: Unfinished

    Function* curFunc=callInst->getParent()->getParent();
    std::vector<Function*> cloned_func;

    for(unsigned index=0; index!= callees.size()-1; index++){
        Function* foo_new = Function::Create(curFunc->getFunctionType(), curFunc->getLinkage(), "foo_new", M);
        foo_new->copyAttributesFrom(curFunc);
        cloned_func.push_back(foo_new);
    }

}


void FPAnalysis::dumpICallResult() {
    Common::dumpICDetailedInfo(latestICallResult);
}

std::map<CallInst *, std::set<llvm::Function *>> FPAnalysis::synBasedRefinement(std::map<CallInst *, std::set<llvm::Function *>> result) {

    std::map<CallInst *, std::set<llvm::Function *>> refinedResult;

    for(auto it=result.begin();it!=result.end();it++){
        CallInst* icall=it->first;

        set<Function*> callees=it->second;

        if(callees.size()>3){
            std::map<std::string,int> commonSubName;

            for(auto it2=callees.begin();it2!=callees.end();it2++){
                for(auto it3=it2;it3!=callees.end();it3++){
                    if(it3==it2){
                        continue;
                    }

                    Function* callee1=*it2;
                    string prename1=callee1->getName();
                    const char *cstr1 =prename1.c_str();
                    string name1=Common::demangle(cstr1);

                    Function* callee2=*it3;
                    string prename2=callee2->getName();
                    const char *cstr2 =prename2.c_str();
                    string name2=Common::demangle(cstr2);

                    string LCS=Common::lcs(&name1, &name2);

                    if(commonSubName.find(LCS)!=commonSubName.end()){
                        commonSubName[LCS]++;
                    }else{
                        commonSubName[LCS]=1;
                    }
                }
            }

            int max=0;
            for(auto it2=commonSubName.begin();it2!=commonSubName.end();it2++){
                if(it2->second>max){
                    max=it2->second;
                }
            }

            //outs()<<"-------------Keystring-----------\n";

            std::set<string> keyString;
            for(auto it2=commonSubName.begin();it2!=commonSubName.end();it2++){
                if(it2->second==max){
                    //outs()<<it2->first<<"\n";
                    keyString.insert(it2->first);
                }
            }

            set<Function*> newCallees;
            set<Function*> exCallees;

            for(auto it2=callees.begin();it2!=callees.end();it2++){
                Function* callee=*it2;
                string name=callee->getName();
                const char *cstr =name.c_str();
                name=Common::demangle(cstr);

                bool isFit=false;
                for(auto itS=keyString.begin();itS!=keyString.end();itS++){
                    if(name.find(*itS)!=string::npos){
                        //outs()<<name<<"\n";
                        newCallees.insert(callee);
                        isFit=true;
                    }
                }
                if(!isFit){
                    exCallees.insert(callee);
                }
            }

            if(newCallees.empty()){
                refinedResult[icall]=callees;
            }else{
                refinedResult[icall]=newCallees;
            }

        }else{
            refinedResult[icall]=callees;
        }
    }

    return refinedResult;

}
