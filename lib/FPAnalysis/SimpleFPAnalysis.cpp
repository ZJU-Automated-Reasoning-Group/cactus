#include <set>
#include <list>
#include <queue>
#include <time.h>
#include <iomanip>
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/CFG.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/Options.h"

#include "FPAnalysis/SimpleFPAnalysis.h"
#include "FPAnalysis/TypeAnalysis.h"
#include "Util/SparrowUtils/Common.h"
#include "Alias/Canary/DyckAA/DyckAliasAnalysis.h"
using namespace llvm;


static cl::opt<int> CallingContextBound( "setting the calling context bound for simple function pointer analysis",
        cl::desc("setting the calling context bound for simple function pointer analysis"),
        cl::init(6), cl::ReallyHidden);

static cl::opt<bool> FlowSensitivity("-flow-sensitive-kelp",
                                     cl::desc("Dump the results of indirect calls"), cl::init(true), cl::NotHidden);




SimpleFPAnalysis* SimpleFPAnalysis::simpleAA = NULL;
llvm::Module* SimpleFPAnalysis::M = NULL;

SimpleFPAnalysis::SimpleFPAnalysis(llvm::Module& _M) {

    M = &_M;

    TypeAnalysis* ty= TypeAnalysis::getTypeAnalysis(_M);

    multi_layer_icall_result=ty->getMLTAResult();

    valid_icall_set=ty->getAllIndirectCalls();

    valid_address_taken_functions = ty->getValidAddrTakenFunc();

    clock_t start, end;
    start=clock();

    analyzeGlobalVariables(_M);


    if(FlowSensitivity){
        performFSDefUseAnalysis();
    }else{
        performFIDefUseAnalysis();
    }

    identifyFuncAddressTakenSite();

    identifyHomogenousFunc();

    end= clock();
    double all_time=(double)(end-start)/CLOCKS_PER_SEC;

    //cout<<"[Kelp] Simple function pointer analysis is finished in "<<fixed<<setprecision(2)<<(float)all_time<<" s"<<endl;
    //Common::printICStatistics("[Kelp]",fs_simple_icall_result);
}

void SimpleFPAnalysis::performFSDefUseAnalysis() {


    std::set<Value*> global_visited;
    std::set<Function*> confined_functions;
    std::set<Function*> pointed_to_by_functions;

    for(auto it=valid_icall_set.begin();it!=valid_icall_set.end();it++){

        CallInst* iCall=*it;

        std::set<Value*> relatedDU; // related def or use sites
        std::set<BasicBlock*> visitedBB; // visited basic blocks
        std::vector<CallInst*> callTrace;
        std::stack<CallInst*> callStack;
        callTrace.push_back(iCall);
        addRelatedDU(iCall->getCalledValue(), relatedDU);

        //outs()<<"---------------------------------------------\n";
        //iCall->getCalledValue()->dump();
        if(isa<Instruction>(iCall->getCalledValue())){
            backwardFSPTAnalysis((Instruction*)iCall->getCalledValue(), relatedDU, visitedBB,callTrace, callStack);
        }else{
            handleConstantFS(iCall->getCalledValue(), relatedDU, visitedBB, callTrace, callStack);
        }
        global_visited.insert(relatedDU.begin(), relatedDU.end());
    }

    //outs()<<"fs_simple_icall_result: \t"<<fs_simple_icall_result.size()<<"\n";

    for(auto it=fs_simple_icall_result.begin();it!=fs_simple_icall_result.end();it++){
        CallInst* indirect_call=it->first;
        simple_icall_set.insert(indirect_call);

        std::set<Function*> callees=it->second;

        for(auto calleeIt=callees.begin();calleeIt!=callees.end();calleeIt++){
            Function* callee=*calleeIt;
            pointed_to_by_functions.insert(callee);
        }
    }


    // look for how the address-taken happens for tracking scopes
    for (Function* func: pointed_to_by_functions) {
        bool isConfined=true;
        for (Value::const_use_iterator I = func->use_begin(), E = func->use_end(); I != E; ++I) {
            User* U = I->getUser();
            if(!global_visited.count(U)){
                isConfined= false;
            }
        }
        if(isConfined){
            confined_functions.insert(func);
        }
    }

    // we use the type analysis to resolve the left indirect calls
    for(auto it=multi_layer_icall_result.begin();it!=multi_layer_icall_result.end();it++){
        CallInst* icall=it->first;
        std::set<Function *> callees=it->second;
        if(fs_simple_icall_result.find(icall)==fs_simple_icall_result.end()){
            fs_simple_icall_result[icall];
            for(auto calleeIt=callees.begin();calleeIt!=callees.end();calleeIt++){
                if(!confined_functions.count(*calleeIt)){
                    fs_simple_icall_result[icall].insert(*calleeIt);
                }
            }
        }
    }

    for(auto it=valid_icall_set.begin();it!=valid_icall_set.end();it++){
        CallInst* icall=*it;
        if(fs_simple_icall_result[icall].empty()){
            fs_simple_icall_result[icall]=multi_layer_icall_result[icall];
        }
    }

}

void SimpleFPAnalysis::analyzeGlobalVariables(llvm::Module &M) {


    // globally collecting the functions in initializers
    for (GlobalVariable& gv : M.getGlobalList()) {
        if(gv.hasInitializer()){
            Constant* initializer = gv.getInitializer();
            if(initializer){
                std::set<Value *> visited;
                std::map<Function*, u32_t> function_info;
                trackFunctionInConstant(initializer, visited, function_info, 0);
                if(!function_info.empty()){
                    gv_with_function_ptr.insert(&gv);
                    gv_map_to_function[&gv]=function_info;
                }
            }
        }
    }

    // locally collecting the functions stored in globals or non-globals by store instructions
    for(auto FI=M.begin();FI!=M.end();FI++){
        Function* F=&*FI;
        for(auto II= inst_begin(*F);II!= inst_end(*F);II++){
            Instruction* I=&*II;
            if(StoreInst* SI= dyn_cast<StoreInst>(I)){

                // Constants are stored into globals
                if(isa<GlobalVariable>(SI->getPointerOperand())){
                    if(isa<Constant>(SI->getValueOperand())){
                        GlobalVariable* gv= dyn_cast<GlobalVariable>(SI->getPointerOperand());
                        Constant* con= dyn_cast<Constant>(SI->getValueOperand());
                        std::set<Value *> visited;
                        std::map<Function*, u32_t> function_info;
                        trackFunctionInConstant(con, visited, function_info,0);
                        if(!function_info.empty()){
                            gv_with_function_ptr.insert(gv);
                            gv_map_to_function[gv]=function_info;
                        }
                    }
                }else{
                    // Constants are stored into non-globals
                    if(isa<Constant>(SI->getValueOperand())&& isa<Function>(SI->getValueOperand())){
                        Constant* con= dyn_cast<Constant>(SI->getValueOperand());
                        std::set<Value *> visited;
                        std::map<Function*, u32_t> function_info;
                        trackFunctionInConstant(con, visited, function_info,0);
                        if(!function_info.empty()){
                            con_with_function_ptr.insert(con);
                            con_map_to_function[con]=function_info;
                        }
                    }
                }
            }
        }
    }


    /***
    for(auto it=gv_map_to_function.begin();it!=gv_map_to_function.end();it++){
        GlobalVariable* gv=it->first;
        std::map<Function*, u32_t> function_info=it->second;
        gv->dump();
        for(auto it2=function_info.begin();it2!=function_info.end();it2++){
            Function* func=it2->first;
            u32_t offset=it2->second;
            outs()<<func->getName().str()<<"\t"<<offset<<"\n";
        }
    }
    ***/
}



void SimpleFPAnalysis::trackFunctionInConstant(Constant * con, std::set<Value *> &visited, std::map<Function*, u32_t>& function_info, u32_t offset) {

    if(visited.count(con)||!con) {
        return;
    }else{
        visited.insert(con);
    }

    if(ConstantArray* con_array= dyn_cast<ConstantArray>(con)){
        for(unsigned i=0;i<con_array->getNumOperands();i++){
            Value* val=con_array->getOperand(i);
            if(Constant* con= dyn_cast<Constant>(val)){
                trackFunctionInConstant(con, visited, function_info, offset);
            }
        }
    }else if(ConstantStruct* con_struct= dyn_cast<ConstantStruct>(con)){
        const StructType *sty = cast<StructType>(con_struct->getType());
        const std::vector<u32_t>& offsetvect = this->getStructOffsetVec(sty);
        for (u32_t i = 0, e = con_struct->getNumOperands(); i != e; i++) {
            u32_t off = offsetvect[i];
            Value* val=con_struct->getOperand(i);
            if(Constant* con= dyn_cast<Constant>(val)){
                trackFunctionInConstant(con, visited, function_info, offset+off);
            }
        }
    }else if(ConstantExpr* con_expr= dyn_cast<ConstantExpr>(con)){
        Value* val=con_expr->getOperand(0);
        if(Constant* con= dyn_cast<Constant>(val)){
            trackFunctionInConstant(con, visited, function_info, offset);
        }
    }else if(GlobalVariable *gv = dyn_cast<GlobalVariable>(con)){
        if(gv->hasInitializer()){
            Constant* con=gv->getInitializer();
            trackFunctionInConstant(con, visited, function_info, offset);
        }
    }else if(Function* callee= dyn_cast<Function>(con)){
        function_info[callee]=offset;
    }else{
        //con->dump();
        //TODO: handle other corner cases
    }
}



void SimpleFPAnalysis::backwardFSPTAnalysis(Instruction* I, std::set<Value*>& relatedDU, std::set<BasicBlock*>& visitedBB,
                                          std::vector<CallInst*>& callTrace, std::stack<CallInst*>& callStack) {

    if(CallingContextBound==callTrace.size()){
        return;
    }

    //flow-sensitive backward CFG traversal

    BasicBlock* curBB=I->getParent();

    if(visitedBB.count(curBB)){
        return ;
    }

    // analyzing the current block
    for (BasicBlock::iterator iterI = I; iterI != curBB->begin(); iterI--) {
        handleInstructionFS(&*iterI, relatedDU, visitedBB, callTrace, callStack);
    }

    handleInstructionFS(&curBB->front(),relatedDU,visitedBB,callTrace, callStack);

    // we have finished the analysis
    visitedBB.insert(curBB);

    //start analyzing previous basic blocks
    std::queue<BasicBlock*> queue;

    for (auto preBBIt=pred_begin(curBB);preBBIt!=pred_end(curBB);preBBIt++){
        BasicBlock* pred=* preBBIt;
        queue.push(pred);
    }

    while (!queue.empty()) {
        BasicBlock *BB = queue.front();
        queue.pop();

        // If the successor has not been visited, add it to the queue and mark it as visited
        if (visitedBB.find(BB) != visitedBB.end()) {
            continue;
        }

        // Traverse the instructions in the basic block backward
        for(auto iterI=BB->rbegin();iterI!=BB->rend();iterI++){
            handleInstructionFS(&*iterI, relatedDU, visitedBB, callTrace, callStack);
        }

        visitedBB.insert(BB);

        // Traverse the successor basic blocks in the CFG
        for (auto predBBIt=pred_begin(BB);predBBIt!=pred_end(BB);predBBIt++){
            BasicBlock* pred=*predBBIt;
            queue.push(pred);
        }
    }

    // analyzing callers
    Function* func=I->getParent()->getParent();
    for(auto argIt=func->arg_begin();argIt!=func->arg_end(); argIt++){
        Argument* arg=&*argIt;
        if(relatedDU.count(arg)){
            unsigned offset = arg->getArgNo();
            Function *parent = arg->getParent();
            for (User *U : parent->users()) {
                if (CallInst *call_inst = dyn_cast<CallInst>(U)){
                    // context-sensitivity
                    bool isAnalyze=false;
                    if(callStack.empty()){
                        isAnalyze=true;
                        callTrace.push_back(call_inst);
                    }else{
                        CallInst* top=callStack.top();
                        if(top==call_inst){
                            callStack.pop();
                            callTrace.pop_back();
                            isAnalyze=true;
                        }
                    }
                    if(isAnalyze){
                        if(call_inst->getNumArgOperands()<offset){  //TODO: check this
                            continue;
                        }
                        Value *real_arg = call_inst->getArgOperand(offset);
                        relatedDU.insert(real_arg);
                        addRelatedDU(real_arg, relatedDU);
                        if(isa<Instruction>(real_arg)){
                            handleInstructionFS((Instruction*)real_arg,relatedDU, visitedBB, callTrace, callStack);
                        }else{
                            handleConstantFS(real_arg, relatedDU, visitedBB, callTrace, callStack);
                        }
                    }
                }else{
                    // indirect calls
                }
            }
        }
    }

}


void SimpleFPAnalysis::handleInstructionFS(Instruction* inst, std::set<Value*>& relatedDU, std::set<BasicBlock*>& visitedBB,
                                         std::vector<CallInst*>& callTrace, std::stack<CallInst*>& callStack) {

    if(!relatedDU.count(inst)&&!isa<StoreInst>(inst)){
        return;
    }

    switch (inst->getOpcode()) {
        case Instruction::Call:{
            CallSite cs(inst);
            CallInst* CI= cast<CallInst>(inst);
            const Function* callee= nullptr;
            if(cs.getCalledFunction()){
                callee = cs.getCalledFunction();
            }else if(const Function* func=dyn_cast<Function>(cs.getCalledValue()->stripPointerCasts())){
                callee = func;
            }else{
                //indirect calls
            }
            if(callee){
                for (auto inst_it = callee->begin(), inst_ie = callee->end(); inst_it != inst_ie; ++inst_it) {
                    if (const ReturnInst *ret = dyn_cast<ReturnInst>(&*inst_it)) {
                        callStack.push(CI);
                        callTrace.push_back(CI);
                        addRelatedDU(ret->getReturnValue(), relatedDU);
                        backwardFSPTAnalysis((ReturnInst *)ret, relatedDU, visitedBB, callTrace, callStack);
                    }
                }
            }
            break;
        }
        case Instruction::Load:{
            LoadInst* CI= cast<LoadInst>(inst);
            addRelatedDU(CI->getPointerOperand(), relatedDU);
            Value* pointer=CI->getPointerOperand();
            if(!isa<Instruction>(pointer)){
                handleConstantFS(pointer, relatedDU, visitedBB, callTrace, callStack);
            }
            break;
        }
        case Instruction::Store:{
            StoreInst* SI= cast<StoreInst>(inst);
            Value* pointer=SI->getPointerOperand();
            Value* pointee=SI->getValueOperand();
            if(relatedDU.count(pointer)){
                addRelatedDU(pointee, relatedDU);
                if(!isa<Instruction>(pointee)){
                    handleConstantFS(pointee, relatedDU, visitedBB, callTrace, callStack);
                }
            }
            break;
        }
        case Instruction::GetElementPtr:{
            GetElementPtrInst* GEP= cast<GetElementPtrInst>(inst);
            Value* val=GEP->getPointerOperand();
            addRelatedDU(val, relatedDU);
            //handle_gep((GEPOperator*)GEP);
            if(!isa<Instruction>(val)){
                handleConstantFS(val, relatedDU, visitedBB, callTrace, callStack);
            }
            break;
        }
        case Instruction::PHI:{
            PHINode* phi= cast<PHINode>(inst);
            for(unsigned index=0; index!=phi->getNumOperands();index++){
                Value* val=phi->getOperand(index);
                addRelatedDU(val, relatedDU);
                if(!isa<Instruction>(val)){
                    handleConstantFS(val, relatedDU, visitedBB, callTrace, callStack);
                }
            }
            break;
        }
        case Instruction::Select:{
            SelectInst* select= cast<SelectInst>(inst);
            for(unsigned index=0;index!=select->getNumOperands(); index++){
                Value* val=select->getOperand(index);
                addRelatedDU(val, relatedDU);
                if(!isa<Instruction>(val)){
                    handleConstantFS(val, relatedDU, visitedBB, callTrace, callStack);
                }
            }
            break;
        }
        case Instruction::BitCast:{
            BitCastInst* BCI= cast<BitCastInst>(inst);

            Value* val=BCI->getOperand(0);
            addRelatedDU(val, relatedDU);
            if(!isa<Instruction>(val)){
                handleConstantFS(val, relatedDU, visitedBB, callTrace, callStack);
            }
            break;
        }
        case Instruction::Ret:{
            ReturnInst* RI= cast<ReturnInst>(inst);
            Value* val=RI->getOperand(0);
            addRelatedDU(val, relatedDU);
            if(!isa<Instruction>(val)){
                handleConstantFS(val, relatedDU, visitedBB, callTrace, callStack);
            }
            break;
        }
        case Instruction::Alloca: {
            break;
        }
        case Instruction::IntToPtr:{
            break;
        }
        case Instruction::VAArg:{
            break;
        }
        default:{
            // not handled yet
        }
    }


}

void SimpleFPAnalysis::handleConstantFS(Value * val, std::set<Value *> &relatedDU, std::set<BasicBlock *> &visitedBB,
                                        std::vector<CallInst *> &callTrace, std::stack<CallInst *> &callStack){

    if(Constant* con= dyn_cast<Constant>(val)){
        handleCE(con, callTrace);
    }else if(Argument* arg= dyn_cast<Argument>(val)){
        //arg->dump();
    }else{
        //val->dump();
        // others?
        // val->dump();
    }

}

void SimpleFPAnalysis::handleCE(const llvm::Value *val, std::vector<CallInst*>& callTrace) {

    if (const Constant* ref = dyn_cast<Constant>(val)) {
        if(const Function* func= dyn_cast<Function>(ref)){
            CallInst* iCall=*(callTrace.begin());
            fs_simple_icall_result[iCall].insert((Function*)func);
            std::vector<CallInst*> context=callTrace;
            context.erase(context.begin());
            std::pair<llvm::Function*, std::vector<CallInst*>> callee_ctx((Function*)func, context);
            fs_simple_icall_result_ctx[iCall].insert(callee_ctx);
        }else if(const GlobalVariable* gv=dyn_cast<GlobalVariable>(ref)){
            //assert(gv_with_function_ptr.count((GlobalVariable*)gv)&&"All global variables have been identified in advance.");
            if(gv_with_function_ptr.count((GlobalVariable*)gv)){
                std::map<Function*, u32_t> func_Info=gv_map_to_function[(GlobalVariable*)gv];
                CallInst* iCall=*(callTrace.begin());
                for(auto funcIt=func_Info.begin(); funcIt!=func_Info.end();funcIt++){
                    Function* func=funcIt->first;
                    u32_t offset=funcIt->second;
                    fs_simple_icall_result[iCall].insert(func);
                }
            }else{
                // null?
            }
        }else{
            if(con_with_function_ptr.count((Constant*)ref)){
                std::map<Function *, u32_t> callee_info = con_map_to_function[(Constant*)ref];
                CallInst* iCall=*(callTrace.begin());
                for(auto funcIt=callee_info.begin(); funcIt!=callee_info.end();funcIt++){
                    Function* func=funcIt->first;
                    u32_t offset=funcIt->second;
                    fs_simple_icall_result[iCall].insert(func);
                }
            }
            // remember to handle the constant bit cast opnd after stripping casts off
        }
    }
}


void SimpleFPAnalysis::collectTypeInfo(const llvm::Type* ty) {

    assert(typeToFieldInfo.find_as(ty) == typeToFieldInfo.end() && "this type has been collected before");

    if (const ArrayType* aty = dyn_cast<ArrayType>(ty)){
        collectArrayInfo(aty);
    }else if (const StructType* sty = dyn_cast<StructType>(ty)){
        collectStructInfo(sty);
    }else{
        collectSimpleTypeInfo(ty);
    }
}

void SimpleFPAnalysis::collectArrayInfo(const ArrayType* ty) {
    StructInfo* stinfo = new StructInfo();
    typeToFieldInfo[ty] = stinfo;

    // Array itself only has one field which is the inner most element
    stinfo->getFieldOffsetVec().push_back(0);

    llvm::Type* elemTy = ty->getElementType();
    while (const ArrayType* aty = dyn_cast<ArrayType>(elemTy))
        elemTy = aty->getElementType();

    // Array's flatten field infor is the same as its element's
    // flatten infor.
    StructInfo* elemStInfo = getStructInfo(elemTy);
    u32_t nfE = elemStInfo->getFlattenFieldInfoVec().size();
    for (u32_t j = 0; j < nfE; j++) {
        u32_t off = elemStInfo->getFlattenFieldInfoVec()[j].getFlattenOffset();
        const Type* fieldTy = elemStInfo->getFlattenFieldInfoVec()[j].getFlattenElemTy();
        FldInfo::ElemNumStridePairVec pair = elemStInfo->getFlattenFieldInfoVec()[j].getElemNumStridePairVect();
        /// append the additional number
        pair.push_back(std::make_pair(1, 0));
        FldInfo field(off, fieldTy, pair);
        stinfo->getFlattenFieldInfoVec().push_back(field);
    }
}

void SimpleFPAnalysis::collectStructInfo(const StructType *sty) {
    /// The struct info should not be processed before
    StructInfo* stinfo = new StructInfo();
    typeToFieldInfo[sty] = stinfo;

    // Number of fields have been placed in the expanded struct
    u32_t nf = 0;

    for (StructType::element_iterator it = sty->element_begin(), ie =
            sty->element_end(); it != ie; ++it) {
        //The offset is where this element will be placed in the exp. struct.
        stinfo->getFieldOffsetVec().push_back(nf);

        const Type *et = *it;
        if (isa<StructType>(et) || isa<ArrayType>(et)) {
            StructInfo * subStinfo = getStructInfo(et);
            u32_t nfE = subStinfo->getFlattenFieldInfoVec().size();
            //Copy ST's info, whose element 0 is the size of ST itself.
            for (u32_t j = 0; j < nfE; j++) {
                u32_t off = nf + subStinfo->getFlattenFieldInfoVec()[j].getFlattenOffset();
                const Type* elemTy = subStinfo->getFlattenFieldInfoVec()[j].getFlattenElemTy();
                FldInfo::ElemNumStridePairVec pair = subStinfo->getFlattenFieldInfoVec()[j].getElemNumStridePairVect();
                FldInfo field(off,elemTy,pair);
                stinfo->getFlattenFieldInfoVec().push_back(field);
            }
            nf += nfE;
        } else { //simple type
            FldInfo::ElemNumStridePairVec pair;
            pair.push_back(std::make_pair(1,0));
            FldInfo field(nf,et,pair);
            stinfo->getFlattenFieldInfoVec().push_back(field);
            ++nf;
        }
    }
}

void SimpleFPAnalysis::collectSimpleTypeInfo(const llvm::Type* ty){
    StructInfo* stinfo = new StructInfo();
    typeToFieldInfo[ty] = stinfo;

    /// Only one field
    stinfo->getFieldOffsetVec().push_back(0);

    FldInfo::ElemNumStridePairVec pair;
    pair.push_back(std::make_pair(1,0));
    FldInfo field(0, ty, pair);
    stinfo->getFlattenFieldInfoVec().push_back(field);
}

void SimpleFPAnalysis::handle_gep(GEPOperator* gep) {

    Value * ptr = gep->getPointerOperand();
    gep_type_iterator GTI = gep_type_begin(gep); // preGTI is the PointerTy of ptr
    int num_indices = gep->getNumIndices();
    int idxidx = 0;
    while (idxidx < num_indices) {
        Value * idx = gep->getOperand(++idxidx);

        Type * AggOrPointerTy = *(GTI++);

        // Type * ElmtTy = *GTI;
        ConstantInt * ci = dyn_cast<ConstantInt>(idx);

        if (AggOrPointerTy->isStructTy()) {

            // example: gep y 0 constIdx
            // s1: y--deref-->?1--(fieldIdx idxLabel)-->?2

            assert(ci && "ERROR: when dealing with gep");

            // s2: ?3--deref-->?2
            unsigned fieldIdx = (unsigned) (*(ci->getValue().getRawData()));

            const std::vector<u32_t>& offsetvect = this->getStructOffsetVec(AggOrPointerTy);
            for (u32_t i = 0, e = offsetvect.size(); i != e; i++) {
                u32_t off = offsetvect[i];
                outs()<<off<<"\n";
            }
        } else if (AggOrPointerTy->isPointerTy() || AggOrPointerTy->isArrayTy() || AggOrPointerTy->isVectorTy()) {
            if (!ci){
                //wrapValue(idx);
            }
        } else {
            errs() << "ERROR in handle_gep: unknown type:\n";
            errs() << "Type Id: " << AggOrPointerTy->getTypeID() << "\n";
            exit(1);
        }
    }

}


void SimpleFPAnalysis::performFIDefUseAnalysis() {


    std::set<Value*> global_visited;
    std::set<Function*> confined_functions;
    std::set<Function*> pointed_to_by_functions;

    for(auto it=valid_icall_set.begin();it!=valid_icall_set.end();it++){
        CallInst* icall=*it;
        std::set<Function *> _callee_info;
        std::set<Value*> visited;
        handleInstructionFI(icall, icall->getCalledValue(), visited, _callee_info); // simple backward def-use checking
        if(!_callee_info.empty()){
            simple_icall_set.insert(icall);
            for(auto calleeIt=_callee_info.begin();calleeIt!=_callee_info.end();calleeIt++){
                Function* callee=*calleeIt;
                if(multi_layer_icall_result[icall].count(callee)){
                    pointed_to_by_functions.insert(callee);
                    fi_simple_icall_result[icall].insert(callee);
                }
            }
            global_visited.insert(visited.begin(),visited.end());
        }
    }

    //outs()<<"fi_simple_icall_result: \t"<<fi_simple_icall_result.size()<<"\n";

    // look for how the address-taken happens for tracking scopes
    for (Function* func: pointed_to_by_functions) {
        bool isConfined=true;
        for (Value::const_use_iterator I = func->use_begin(), E = func->use_end(); I != E; ++I) {
            User* U = I->getUser();
            if(!global_visited.count(U)){
                isConfined= false;
            }
        }
        if(isConfined){
            confined_functions.insert(func);
        }
    }


    for(auto it=multi_layer_icall_result.begin();it!=multi_layer_icall_result.end();it++){
        CallInst* icall=it->first;
        std::set<Function *> callees=it->second;
        if(fi_simple_icall_result.find(icall)==fi_simple_icall_result.end()){
            fi_simple_icall_result[icall];
            for(auto calleeIt=callees.begin();calleeIt!=callees.end();calleeIt++){
                if(!confined_functions.count(*calleeIt)){
                    fi_simple_icall_result[icall].insert(*calleeIt);
                }
            }
        }
    }

    for(auto it=valid_icall_set.begin();it!=valid_icall_set.end();it++){
        CallInst* icall=*it;
        if(fi_simple_icall_result[icall].empty()){
            fi_simple_icall_result[icall]=multi_layer_icall_result[icall];
        }
    }

}

void SimpleFPAnalysis::handleInstructionFI(CallInst* icall, Value* val, std::set<Value*>& visited, std::set<Function*>& _callee_info){

    if(visited.count(val)||!val){
        return;
    }
    visited.insert(val);

    if (LoadInst *load_inst = dyn_cast<LoadInst>(val)) {
        Value *loaded_val = load_inst->getPointerOperand();
        handleInstructionFI(icall, loaded_val, visited, _callee_info);
    }else if (Argument *argument = dyn_cast<Argument>(val)) { // the function pointer may be passed by the arg, get the use of the argument
        unsigned offset = argument->getArgNo();
        Function *parent = argument->getParent();
        for (User *U : parent->users()) {
            if (CallInst *call_inst = dyn_cast<CallInst>(U)){
                if(call_inst->getNumArgOperands()<offset){  //TODO: check this
                    continue;
                }
                Value *real_arg = call_inst->getArgOperand(offset);
                handleInstructionFI(icall, real_arg, visited,_callee_info);
            }
        }
    }else if (PHINode *phi_node = dyn_cast<PHINode>(val)) {
        for(auto it=phi_node->op_begin();it!=phi_node->op_end();it++){
            Value *incoming_val=*it;
            handleInstructionFI(icall, incoming_val, visited,_callee_info);
        }
    }else if (CallInst *call_inst = dyn_cast<CallInst>(val)) {  // the function pointer is a call instruction,recursively process
        Function *func = call_inst->getCalledFunction();
        if(func){
            // we do the backward?
            for (auto inst_it = func->begin(), inst_ie = func->end(); inst_it != inst_ie; ++inst_it) {
                if (ReturnInst *ret = dyn_cast<ReturnInst>(&*inst_it)) {
                    Value *ret_val = ret->getReturnValue();
                    handleInstructionFI(icall, ret_val, visited, _callee_info);
                }
            }
        }
    }else if(BitCastInst* bit_cast= dyn_cast<BitCastInst>(val)){
        handleInstructionFI(icall, bit_cast->getOperand(0),visited,_callee_info);
    }else if(CastInst* cast_inst = dyn_cast<CastInst>(val)){ // this case has been excluded
        handleInstructionFI(icall, cast_inst->getOperand(0),visited,_callee_info);
    }else if(SelectInst* select_inst= dyn_cast<SelectInst>(val)){
        for(auto it=select_inst->op_begin();it!=select_inst->op_end();it++) {
            Value *select_val = *it;
            handleInstructionFI(icall, select_val,visited,_callee_info);
        }
    }else if(GetElementPtrInst* gep= dyn_cast<GetElementPtrInst>(val)){
        Value *v = gep->getPointerOperand();
        handleInstructionFI(icall, v, visited,_callee_info);
    }else if(AllocaInst* alloca_inst= dyn_cast<AllocaInst>(val)){
        for (User *U : alloca_inst->users()){
            if(StoreInst* store_inst= dyn_cast<StoreInst>(U)){
                Value *value = store_inst->getValueOperand();
                if(Constant* con= dyn_cast<Constant>(value)){
                    handleConstantFI(icall, con, visited, _callee_info);
                }else{
                    //other complex cases?
                }
            }
        }
    }else if(Constant* con= dyn_cast<Constant>(val)){
        handleConstantFI(icall, con, visited, _callee_info);
    }else{
        //val->dump(); //if any
    }

}

void SimpleFPAnalysis::handleConstantFI(CallInst *icall, Constant *con, std::set<Value *> &visited, std::set<Function*>& _callee_info) {



    if(ConstantArray* con_array= dyn_cast<ConstantArray>(con)){
        for(unsigned i=0;i<con_array->getNumOperands();i++){
            Value* val=con_array->getOperand(i);
            if(Constant* con= dyn_cast<Constant>(val)){
                if(visited.count(con)||!con) {
                    return;
                }else{
                    visited.insert(con);
                }
                handleConstantFI(icall, con, visited, _callee_info);
            }
        }
    }else if(ConstantStruct* con_struct= dyn_cast<ConstantStruct>(con)){
        for(unsigned i=0;i<con_struct->getNumOperands();i++){
            Value* val=con_struct->getOperand(i);
            if(Constant* con= dyn_cast<Constant>(val)){
                if(visited.count(con)||!con) {
                    return;
                }else{
                    visited.insert(con);
                }
                handleConstantFI(icall, con, visited, _callee_info);
            }
        }
    }else if(ConstantExpr* con_expr= dyn_cast<ConstantExpr>(con)){
        Value* val=con_expr->getOperand(0);
        if(Constant* con= dyn_cast<Constant>(val)){
            if(visited.count(con)||!con) {
                return;
            }else{
                visited.insert(con);
            }
            handleConstantFI(icall, con, visited, _callee_info);
        }
    }else if(GlobalVariable *gv = dyn_cast<GlobalVariable>(con)){
        if(gv->hasInitializer()){
            Constant* con=gv->getInitializer();
            if(visited.count(con)||!con) {
                return;
            }else{
                visited.insert(con);
            }
            handleConstantFI(icall, con, visited, _callee_info);
        }
    }else if(Function* callee= dyn_cast<Function>(con)){
        _callee_info.insert(callee);
    }else{
        //        con->dump(); //if any
    }

}

void SimpleFPAnalysis::identifyFuncAddressTakenSite() {

    // the instructions with operands being as functions
    for(auto it=valid_address_taken_functions.begin();it!=valid_address_taken_functions.end();it++){
        Function* func=*it;
        for(auto userIt=func->user_begin();userIt!=func->user_end();userIt++){
            User* user=*userIt;
            if(isa<Instruction>(user)){
                Instruction* I= cast<Instruction>(user);
                address_taken_sites.insert(I);
                address_taken_site_to_func[I].insert(func);
            }
        }
    }

    // the instructions with operands being as globals containing functions
    for(auto it=gv_map_to_function.begin(); it!=gv_map_to_function.end();it++){
        GlobalVariable* gv=it->first;
        std::map<Function*, u32_t> func_set=it->second;
        for(auto userIt=gv->user_begin();userIt!=gv->user_end();userIt++){
            User* user=*userIt;
            if(Instruction* I= dyn_cast<Instruction>(user)){
                std::map<u32_t, std::set<Function*>> field_map_to_func;
                for(auto funcIt=func_set.begin();funcIt!=func_set.end();funcIt++){
                    field_map_to_func[funcIt->second].insert(funcIt->first);
                }
                for(auto fieldIt=field_map_to_func.begin();fieldIt!=field_map_to_func.end();fieldIt++){
                    std::set<Function*> field_func =fieldIt->second;
                    address_taken_sites.insert(I);
                    // may be I have been recorded previously
                    if(address_taken_site_to_func.find(I)==address_taken_site_to_func.end()){
                        address_taken_site_to_func[I]=field_func;
                    }else{
                        address_taken_site_to_func[I].insert(field_func.begin(), field_func.end());
                    }
                }
            }
        }
    }

    // the instructions with operands being as constants containing functions
    for(auto it=con_map_to_function.begin(); it!=con_map_to_function.end();it++){
        Constant* con=it->first;
        std::map<Function*, u32_t> func_set=it->second;
        for(auto userIt=con->user_begin();userIt!=con->user_end();userIt++){
            User* user=*userIt;
            if(Instruction* I= dyn_cast<Instruction>(user)){
                std::map<u32_t, std::set<Function*>> field_map_to_func;
                for(auto funcIt=func_set.begin();funcIt!=func_set.end();funcIt++){
                    field_map_to_func[funcIt->second].insert(funcIt->first);
                }
                for(auto fieldIt=field_map_to_func.begin();fieldIt!=field_map_to_func.end();fieldIt++){
                    std::set<Function*> field_func =fieldIt->second;
                    address_taken_sites.insert(I);
                    // may be I have been recorded previously
                    if(address_taken_site_to_func.find(I)==address_taken_site_to_func.end()){
                        address_taken_site_to_func[I]=field_func;
                    }else{
                        address_taken_site_to_func[I].insert(field_func.begin(), field_func.end());
                    }
                }
            }
        }
    }
}

void SimpleFPAnalysis::identifyHomogenousFunc() {

    // global analysis

    for(auto gvIt=gv_map_to_function.begin();gvIt!=gv_map_to_function.end();gvIt++){
        std::map<Function*, u32_t> function_info=gvIt->second;
        std::map<u32_t, std::set<Function*>> offset_to_func;
        for(auto funIt=function_info.begin();funIt!=function_info.end(); funIt++){
            Function* func=funIt->first;
            u32_t offset=funIt->second;
            offset_to_func[offset].insert(func);
        }
        for(auto offIt=offset_to_func.begin();offIt!=offset_to_func.end();offIt++){
            std::set<Function*> funcSet=offIt->second;
            if(funcSet.size()!=1){ // we ignore the singleton
                homo_func.insert(funcSet);
            }
        }
    }

    for(auto conIt=con_map_to_function.begin();conIt!=con_map_to_function.end();conIt++){
        std::map<Function*, u32_t> function_info=conIt->second;
        std::map<u32_t, std::set<Function*>> offset_to_func;
        for(auto funIt=function_info.begin();funIt!=function_info.end(); funIt++){
            Function* func=funIt->first;
            u32_t offset=funIt->second;
            offset_to_func[offset].insert(func);
        }
        for(auto offIt=offset_to_func.begin();offIt!=offset_to_func.end();offIt++){
            std::set<Function*> funcSet=offIt->second;
            if(funcSet.size()!=1){ // we ignore the singleton
                homo_func.insert(funcSet);
            }
        }
    }

//    outs()<<"after global analysis:\t"<<homo_func.size()<<"\n";
    // local analysis
    localAnalysisForHomoFunc();

//    outs()<<"after local analysis:\t"<<homo_func.size()<<"\n";

//    for(auto it=homo_func.begin();it!=homo_func.end();it++){
//        outs()<<it->size()<<"\n";
//    }
}

// homogeneous functions are often stored in the same memory locations decided by external inputs
void SimpleFPAnalysis::localAnalysisForHomoFunc() {


    std::set<Value*> globalVisited;

    for(auto it=valid_address_taken_functions.begin();it!=valid_address_taken_functions.end();it++){
        Function* func=*it;
        for(auto userIt=func->user_begin();userIt!=func->user_end();userIt++){

            // avoid computing repetitively
            if(globalVisited.count(*userIt)){
                continue;
            }

            // here, we only handle instructions, as globals are handled separately
            if(Instruction* I= dyn_cast<Instruction>(*userIt)){
                std::set<Value*> visited;
                std::queue<Value*> queue;
                queue.push(I);
                while(!queue.empty()){
                    Value* val=queue.front();
                    queue.pop();
                    if(visited.count(val)){
                        continue;
                    }else{
                        visited.insert(val);
                    }
                    if(CallInst* CI= dyn_cast<CallInst>(val)){
                        for(unsigned real_in=0;real_in!=CI->getNumArgOperands();real_in++){
                            if(visited.count(CI->getArgOperand(real_in))){
                                const Function* callee= nullptr;
                                if(CI->getCalledFunction()){
                                    callee = CI->getCalledFunction();
                                }else if(const Function* func=dyn_cast<Function>(CI->getCalledValue()->stripPointerCasts())){
                                    callee = func;
                                }else{
                                    //indirect calls
                                }
                                if(callee&&!callee->isVarArg()){
                                    for(auto formal_arg=callee->arg_begin();formal_arg!=callee->arg_end();formal_arg++){
                                        if(formal_arg->getArgNo()==real_in){
                                            const Argument* arg=formal_arg;
                                            for(auto userIt=arg->user_begin();userIt!=arg->user_end();userIt++){
                                                const User* user=*userIt;
                                                queue.push((User*)user);
                                            }
                                        }
                                    }
                                }

                            }
                        }
                    }else if(Argument* arg= dyn_cast<Argument>(val)){
                        Function* callee=arg->getParent();
                        if(!callee->isVarArg()){
                            unsigned  formal_index=arg->getArgNo();
                            for(auto userIt=callee->user_begin();userIt!=callee->user_end();userIt++){
                                if(CallInst* CI= dyn_cast<CallInst>(*userIt)){
                                    Value* real_arg=CI->getOperand(formal_index);
                                    for(auto userIt=real_arg->user_begin();userIt!=real_arg->user_end();userIt++){
                                        const User* user=*userIt;
                                        queue.push((User*)user);
                                    }
                                }
                            }
                        }
                    }else if(ReturnInst* RI= dyn_cast<ReturnInst>(val)){
                        Function* callee=RI->getParent()->getParent();
                        if(!callee->isVarArg()){
                            for(auto userIt=callee->user_begin();userIt!=callee->user_end();userIt++){
                                if(CallInst* CI= dyn_cast<CallInst>(*userIt)){
                                    for(auto userIt= CI->user_begin();userIt!= CI->user_end();userIt++){
                                        const User* user=*userIt;
                                        queue.push((User*)user);
                                    }
                                }
                            }
                        }
                    }else{ // normal statements
                        for(auto userIt=I->user_begin();userIt!=I->user_end();userIt++){
                            User* user=*userIt;
                            queue.push(user);
                        }
                    }
                }

                globalVisited.insert(visited.begin(), visited.end());

                for(auto it=visited.begin();it!=visited.end();it++){
                    Value* val=*it;
                    std::set<Function*> sub_homo_func;
                    if(isa<Instruction>(val)){
                        Instruction* I= cast<Instruction>(val);
                        if(address_taken_site_to_func.find(I)!=address_taken_site_to_func.end()){
                            std::set<Function*> funcSet=address_taken_site_to_func[I];
                            for(auto funcIt=funcSet.begin();funcIt!=funcSet.end();funcIt++){
                                Function* func=*funcIt;
                                sub_homo_func.insert(func);
                                //outs()<<func->getName().str()<<"\n";
                            }
                        }
                    }
                    if(sub_homo_func.size()!=1){ // singleton is always precise
                        homo_func.insert(sub_homo_func);
                    }
                }
                //outs()<<"------------------------------------------\n";
            }
        }
    }
}
