#include <set>
#include <list>
#include <climits>
#include <time.h>
#include <iomanip>
#include <queue>

#include <llvm/Pass.h>
#include <llvm/IR/Value.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/CallSite.h>
#include <llvm/Support/Debug.h>
#include <llvm/Support/CommandLine.h>
#include <llvm/IR/InstrTypes.h>
#include <llvm/IR/InstIterator.h>

#include "FPAnalysis/TypeAnalysis.h"
#include "SparrowUtils/Common.h"
#include "SparrowUtils/Profiler.h"


using namespace llvm;

static cl::opt<int> address_taken_analysis_restrict_function_pointer_size(
        "address-taken-analysis-restrict-function-pointer-size",
        cl::desc("Specify the maximal function pointer size computed by address-taken analysis"),
        cl::init(100000000), cl::ReallyHidden);

static cl::opt<bool> address_taken_analysis_enable_field_pruning(
        "address-taken-analysis-enable-field-pruning",
        cl::desc("Enable field-based pruning of address-taken analysis results"),
        cl::init(false), cl::Hidden);

static cl::opt<bool> eager_func_signature_analysis(
        "eager-type-check",
        cl::init(false),
        cl::Hidden,
        cl::desc("Function signature analysis becomes eager."));


static cl::opt<string> MethodMode(
        "dump-fp-result",
        cl::init("no"), cl::NotHidden,
        cl::desc("Please specify the used method to produce the call graph: fsa, mlta, scope..."));

#define CALLED_FUNC_IN_STRUCT_OFFSET INT_MAX
#define FUNC_STORED_IN_STRUCT_OFFSET INT_MAX - 1


TypeAnalysis::TypeAnalysis(llvm::Module& _M) {


    M = &_M;

    default_ptrsz = M->getDataLayout()->getPointerSizeInBits();

    identifyAllAddressTakenFunc(M);
    identifyValidAddressTakenFunc(M);
    identifyValidIndirectCall(M);


    //cout<<"[FLTA] Valid address-taken functions: "<<valid_address_taken_functions.size()<<endl;
    //cout<<"[FLTA] Valid indirect calls: "<<indirect_callsite.size()<<endl;


    clock_t start, end;
    start=clock();
    //performFunctionSignatureAnalysis();
    performFirstLayerTypeAnalysis();
    end= clock();
    double first_layer_time=(double)(end-start)/CLOCKS_PER_SEC;
    //cout<<"[FLTA] First-layer type analysis is finished in "<<fixed<<setprecision(2)<<(float)first_layer_time<<" s"<<endl;
    //Common::printICStatistics("[FLTA]",first_layer_icall_result);

    clock_t start2, end2;
    start2=clock();
    performMultiLayerTypeAnalysis();
    end2= clock();
    double multi_layer_time=(double)(end2-start2)/CLOCKS_PER_SEC;
    //cout<<"[MLTA] Multi-layer type analysis is finished in "<<fixed<<setprecision(2)<<(float)multi_layer_time<<" s"<<endl;
    //Common::printICStatistics("[MLTA]", multi_layer_icall_result);


    if(MethodMode=="flta"){
        Common::dumpICDetailedInfo(first_layer_icall_result);
    }else if(MethodMode=="mlta"){
        Common::dumpICDetailedInfo(multi_layer_icall_result);
    }else{
        //outs()<<"[Type Analysis] Please specify the kinds of type analyzes.\n";
    }

}

TypeAnalysis* TypeAnalysis::addrAA = NULL;
llvm::Module* TypeAnalysis::M = NULL;

void TypeAnalysis::identifyValidAddressTakenFunc(Module* M) {

    // refer to SEC'20 "Temporal system call specialization for attack surface reduction"
    // so called address-taken function analysis

    for(auto func_iter=M->begin();func_iter!=M->end();func_iter++){
        Function* func=&*func_iter;
        bool is_only_stripped=true;
        list<Constant*> worklist;
        set<Constant*> visited;
        worklist.push_back(func);
        while(!worklist.empty()){
            Constant* con=worklist.back();
            worklist.pop_back();

            if(visited.count(con)){
                continue;
            }
            visited.insert(con);

            for(User* user:con->users()){
                // capturing the case that is "only" used at stripped indirect calls
                if(Instruction* inst= dyn_cast<Instruction>(user)){
                    if(!isa<CallInst>(inst)){
                        is_only_stripped= false;
                        worklist.clear();
                        break;
                    }else{
                        CallInst* call_inst= dyn_cast<CallInst>(inst);
                        if(!isa<Function>(call_inst->getCalledValue()->stripPointerCasts())){
                            is_only_stripped= false;
                            worklist.clear();
                            break;
                        }
                    }
                }else if(Constant* con= dyn_cast<Constant>(user)){
                    is_only_stripped= false;
                    worklist.push_back(con);
                }
            }
        }
        if(!is_only_stripped){
            valid_address_taken_functions.insert(func);
        }
    }

}

void TypeAnalysis::identifyAllAddressTakenFunc(Module *M) {


    for (llvm::Module::iterator FI = M->begin(), FE = M->end(); FI != FE; ++FI) {

        // The origin is also to check FI->isDeclaration(), which is buggy

        if (FI->isIntrinsic()) {
            continue;
        }
        std::set<Value*> visited_val;
        if (isAddressTaken(FI, visited_val)) {
            std::set<llvm::Instruction*> Info;
            std::set<llvm::User*> visited;
            address_taken_functions.insert(FI);
            getAddressTakenInfo(FI,Info,visited);
            address_taken_functions_info[FI]=Info;
            address_taken_funcs_user_info[FI]=visited;
        }
    }
    //outs()<<address_taken_functions.size()<<"\n";
}

void TypeAnalysis::identifyValidIndirectCall(Module * M) {

    for(auto itF=M->begin();itF!=M->end();itF++) {
        Function *F = &*itF;

        if(F->isIntrinsic()||F->isDeclaration()){
            continue;
        }

        for(auto itB=F->begin();itB!=F->end();itB++){
            BasicBlock* BB=&*itB;
            for(auto itI=BB->begin();itI!=BB->end();itI++){
                Instruction* I=&*itI;

                if(CallInst* CI= dyn_cast<CallInst>(I)){ // indirect calls

                    if(isa<InlineAsm>(CI->getCalledValue())){
                        inline_asm_icall.insert(CI);
                        continue;
                    }

                    if(!CI->getCalledFunction()){
                        if(Function* callee=dyn_cast<Function>(CI->getCalledValue()->stripPointerCasts())){
                            CI->setCalledFunction(callee);
                            stripped_icall.insert(CI);
                        }else{
                            indirect_callsite.insert(CI);
                        }
                    }
                    all_callsite.insert(CI);
                }
            }
        }
    }

}

void TypeAnalysis::performFirstLayerTypeAnalysis() {


    // classifying address-taken functions

    std::map<std::vector<Type*>, std::set<Function*>> arg_type_to_func;
    std::set<Function*> var_arg_func;

    for(auto it=valid_address_taken_functions.begin();it!=valid_address_taken_functions.end();it++){
        Function* func=*it;
        if(!func->isVarArg()){
            auto& arg_list = func->getArgumentList();
            std::vector<Type*> arg_type;
            //arg_type.push_back(func->getReturnType());
            for (Argument& formal_arg : arg_list){
                Type* formal_type = formal_arg.getType();
                arg_type.push_back(formal_type);
            }

            arg_type_to_func[arg_type].insert(func);

        }else{
            var_arg_func.insert(func);
        }
    }

    for(auto it=indirect_callsite.begin();it!=indirect_callsite.end();it++){
        CallInst* iCall=*it;
        std::set<Function*> result;
        std::vector<Type*> real_arg_type;
        //real_arg_type.push_back(iCall->getType());
        CallSite cs(iCall);
        for(auto arg_it=cs.arg_begin();arg_it!=cs.arg_end();arg_it++){
            Value* real_arg= *arg_it;
            Type* real_type = real_arg->getType();
            real_arg_type.push_back(real_type);
        }

        if(arg_type_to_func.find(real_arg_type)!=arg_type_to_func.end()){
            set<Function*> matched_func=arg_type_to_func.find(real_arg_type)->second;
            matched_func.insert(var_arg_func.begin(),var_arg_func.end());
            for(auto it2=matched_func.begin();it2!=matched_func.end();it2++){
                Function* callee=*it2;
                if(isCallsiteFunctionStrictCompatible(iCall,callee)){
                    type_matched_address_taken_functions.insert(callee);
                    result.insert(callee);
                    type_matched_icall.insert(iCall);
                }
            }
        }
        first_layer_icall_result[iCall]=result;
    }
}

void TypeAnalysis::performMultiLayerTypeAnalysis(){

    // multi-layer type analysis

    for(auto icallIt=indirect_callsite.begin();icallIt!=indirect_callsite.end();icallIt++){
        CallInst* icall=*icallIt;
        std::set<Function*> result=first_layer_icall_result[icall];
        CallSite cs(icall);
        performFieldPruning(cs,result);
        multi_layer_icall_result[icall]=result;
    }

}


bool TypeAnalysis::guessCalleesForIndCallSite(CallSite callsite, std::set<Function*> &result) {

    Function *base_func = callsite->getParent()->getParent();
    std::set<Function*> matched_address_taken_funcs;

    for (Function* func : valid_address_taken_functions) {
        if (isCallsiteFunctionStrictCompatible(callsite, func)) {
            matched_address_taken_funcs.insert(func);
        }
    }


    if (matched_address_taken_funcs.size() >= 1) {
        if (matched_address_taken_funcs.size() <= address_taken_analysis_restrict_function_pointer_size) {
            for (Function* func : matched_address_taken_funcs) {
                // block recursive funcs
                if (func == base_func) {
                    continue;
                }
                result.insert(func);
            }
        }
        return true;
    }

    return false;
}

bool TypeAnalysis::performFieldPruning(llvm::CallSite callsite, std::set<llvm::Function *> &matched_address_taken_funcs) {

    Value* callee_val = callsite.getCalledValue();
    Function *base_func = callsite->getParent()->getParent();

    std::set<Function*> matched_after_pruning;

    int called_func_index = CALLED_FUNC_IN_STRUCT_OFFSET;

    // First, get the offset of the called function pointer
    if (LoadInst* load = dyn_cast<LoadInst>(callee_val)) {
        Value* load_ptr = load->getPointerOperand();
        if (GetElementPtrInst* gep = dyn_cast<GetElementPtrInst>(load_ptr)) {
            int offset = getGepConstantOffset(gep);
            if (offset != CALLED_FUNC_IN_STRUCT_OFFSET) {
                called_func_index = offset;
            }
        } else if (CastInst* cast = dyn_cast<CastInst>(load_ptr)) {
            if (GetElementPtrInst* gep = dyn_cast<GetElementPtrInst>(cast->getOperand(0))) {
                int offset = getGepConstantOffset(gep);
                if (offset != CALLED_FUNC_IN_STRUCT_OFFSET) {
                    called_func_index = offset;
                }
            }
        }
    } else if (CastInst* cast = dyn_cast<CastInst>(callee_val)) {
        if (LoadInst* load = dyn_cast<LoadInst>(cast->getOperand(0))) {
            if (GetElementPtrInst* gep = dyn_cast<GetElementPtrInst>(load->getPointerOperand())) {
                int offset = getGepConstantOffset(gep);
                if (offset != CALLED_FUNC_IN_STRUCT_OFFSET) {
                    called_func_index = offset;
                }
            }
        }
    } else if (PHINode* phi = dyn_cast<PHINode>(callee_val)) {
        unsigned num_values = phi->getNumIncomingValues();
        if (num_values >= 1) {
            for (unsigned i = 0; i < num_values; i++) {
                Value* val_i = phi->getIncomingValue(i);
                if (LoadInst* load_i = dyn_cast<LoadInst>(val_i)) {
                    if (GetElementPtrInst* gep = dyn_cast<GetElementPtrInst>(load_i->getPointerOperand())) {
                        int offset = getGepConstantOffset(gep);
                        if (offset != CALLED_FUNC_IN_STRUCT_OFFSET) {
                            called_func_index = offset;
                        }
                    } else if (CastInst* cast = dyn_cast<CastInst>(load_i->getPointerOperand())) {
                        if (GetElementPtrInst* gep = dyn_cast<GetElementPtrInst>(cast->getOperand(0))) {
                            int offset = getGepConstantOffset(gep);
                            if (offset != CALLED_FUNC_IN_STRUCT_OFFSET) {
                                called_func_index = offset;
                            }
                        }
                    }
                }
            }
        }
    }


    if (called_func_index != CALLED_FUNC_IN_STRUCT_OFFSET) {
        // Second, guess the offset of stored address-taken functions
        for (Function* func : matched_address_taken_funcs) {
            //int canidate_func_index = addressTakenFuncStoreIndexBase(func);
            //int canidate_func_index = addressTakenFuncStoreIndex(func);

            int canidate_func_index=CALLED_FUNC_IN_STRUCT_OFFSET;
            std::set<Instruction*> funcInfo=address_taken_functions_info.find(func)->second;
            std::set<User*> func_user_info=address_taken_funcs_user_info.find(func)->second;

            bool has_store=false;
            for(auto info:func_user_info){
                if(StoreInst* store_inst=dyn_cast<StoreInst>(info)){
                    has_store=true;
                }
            }
            if(has_store){
                for(auto info:func_user_info){
                    if(StoreInst* store_inst=dyn_cast<StoreInst>(info)){
                        has_store=true;
                        if (store_inst->getNumOperands() >= 2) {
                            Value* target = store_inst->getOperand(1);
                            if (GetElementPtrInst *gep = dyn_cast<GetElementPtrInst>(target)) {
                                int offset = getGepConstantOffset(gep);
                                canidate_func_index=offset;
                                if (called_func_index == canidate_func_index) {
                                    matched_after_pruning.insert(func);
                                    break;
                                }
                            }
                        }
                    }
                }
            }else{
                matched_after_pruning.insert(func);
            }
        }


        // Finally, check the pruning results
        if (matched_after_pruning.size() >=1 &&
            matched_after_pruning.size() <= address_taken_analysis_restrict_function_pointer_size) {

            matched_address_taken_funcs.clear();

            for (Function* func : matched_after_pruning) {
                if (func == base_func) {
                    continue;
                }
                matched_address_taken_funcs.insert(func);
            }
            return true;
        }
    }

    return false;

}

int TypeAnalysis::getGepConstantOffset(GetElementPtrInst* gep) {
    int ret = CALLED_FUNC_IN_STRUCT_OFFSET;
    Value* val = gep->getOperand(0);
    if (val->getType()->isPointerTy()) {
        if (val->getType()->getPointerElementType()->isStructTy()) {
            if (gep->getNumOperands() >= 3) {
                Value* offset_val = gep->getOperand(2);
                if (llvm::ConstantInt* CI = dyn_cast<llvm::ConstantInt>(offset_val)) {
                    ret = CI->getSExtValue();
                }
            }
        }
    }
    return ret;
}


int TypeAnalysis::addressTakenFuncStoreIndexBase(Function* func) {
    int ret = FUNC_STORED_IN_STRUCT_OFFSET;
    for (Value::const_use_iterator I = func->use_begin(), E = func->use_end(); I != E; ++I) {
        User* U = I->getUser ();
        if (isa<StoreInst>(U)) {
            if (StoreInst* store_inst = dyn_cast<StoreInst>(U)) {
                if (store_inst->getNumOperands() >= 2) {
                    Value* target = store_inst->getOperand(1);
                    if (GetElementPtrInst *gep = dyn_cast<GetElementPtrInst>(target)) {
                        int offset = getGepConstantOffset(gep);
                        if (offset != CALLED_FUNC_IN_STRUCT_OFFSET) {
                            return offset;
                        }
                    }
                }
            }
        } else if (isa<Constant>(U)) {
            if (ConstantStruct* pt_val = dyn_cast<ConstantStruct>(U)) {
                StructType *struct_type = dyn_cast<StructType>(pt_val->getType());
                unsigned struct_size = getTypeSizeInBits(struct_type);
                unsigned num_fields = pt_val->getType()->getStructNumElements();
                for (unsigned i = 0; i < num_fields; i++) {
                    if (struct_type->getElementType(i)->isPointerTy()) {
                        if (struct_type->getElementType(i)->getPointerElementType()->isFunctionTy()) {
                            Constant* elem_val = pt_val->getAggregateElement(i);
                            if (Function* pt_func = dyn_cast<Function>(elem_val)) {
                                if (pt_func == func) {
                                    ret = i;
                                    return ret;
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    return ret;
}

bool TypeAnalysis::isAddressTaken(Value* V, std::set<Value*> visited) {

    if(visited.count(V)){
        return true;
    }
    visited.insert(V);

    for (Value::const_use_iterator I = V->use_begin(), E = V->use_end(); I != E; ++I) {
        User *U = I->getUser ();

        if (isa<StoreInst>(U)){
            return true;
        }else if (!isa<CallInst>(U) && !isa<InvokeInst>(U)) {
            if (U->use_empty()){
                continue;
            }else if (isa<GlobalAlias>(U)) {
                if (isAddressTaken(U, visited))
                    return true;
            } else {
                if (Constant *C = dyn_cast<Constant>(U)) {
                    if (ConstantExpr *CE = dyn_cast<ConstantExpr>(C)) {
                        if (CE->getOpcode() == Instruction::BitCast) {
                            //return isAddressTaken(CE);
                            return true;
                        }
                    }
                }
                return true;
            }
            // FIXME: Can be more robust here for weak aliases that are never used
        } else {
            llvm::CallSite CS(cast<Instruction>(U));
            if (!CS.isCallee(&*I)){
                return true;
            }
        }
    }

    return false;
}

void TypeAnalysis::getAddressTakenInfo(llvm::Value *V, std::set<llvm::Instruction *>& Info, std::set<llvm::User*>& visited) {

    for (Value::const_use_iterator I = V->use_begin(), E = V->use_end(); I != E; ++I) {
        User *U = I->getUser ();

        if(visited.find(U)!=visited.end()){
            return;
        }
        visited.insert(U);
        if (isa<StoreInst>(U)){
            StoreInst* SI=dyn_cast<StoreInst>(U);
            Info.insert(SI);
        } else if(isa<CallInst>(U)){ //invoke has lowered to callInst
            CallInst* CI=dyn_cast<CallInst>(U);
            Info.insert(CI);
        }else{
            getAddressTakenInfo(U,Info,visited);
        }
    }
}

bool TypeAnalysis::isTypeCompatible(Type* t1, Type* t2) {
    if ((!t1) || (!t2))
        return false;
    if (t1->isPointerTy() && t2->isPointerTy())
        return true;
    return t1 == t2;
}

bool TypeAnalysis::isCallsiteFunctionStrictCompatible(CallSite callsite, Function* func) {

    // block recursive functions
    Function* base=callsite->getParent()->getParent();
    if(base==func){
        return false;
    }

    auto& arg_list = func->getArgumentList();
    unsigned callsite_arg_size = callsite.arg_size();
    unsigned func_arg_size = arg_list.size();
    if (!isTypeCompatible(callsite->getType(), func->getReturnType())) {
        // return type incompatible
        return false;
    }
    if ((!func->isVarArg()) && callsite_arg_size != func_arg_size) {
        // non-va-arg, function and callsite must have the same amount of arguments to be compatible
        return false;
    }

    unsigned idx = 0;
    for (Argument& formal_arg : arg_list) {
        if (idx >= callsite_arg_size) {
            // not enough callsite arguments
            return false;
        }
        Value* real_arg = callsite.getArgument(idx);
        Type* real_type = real_arg->getType();
        Type* formal_type = formal_arg.getType();

        if ((!real_type) || (!formal_type)){
            return false;
        }

        if(real_type==formal_type){
            return true;
        }

        if (real_type->isPointerTy() && formal_type->isPointerTy()) {
            if (real_type->getPointerElementType()->isStructTy()) {
                if (!formal_type->getPointerElementType()->isStructTy()) {
                    return false;
                } else {
                    if (real_type->getPointerElementType()->getStructNumElements() !=
                        formal_type->getPointerElementType()->getStructNumElements()) {
                        return false;
                    }
                }
            }
        } else if (real_type->isStructTy() && formal_type->isStructTy()) {
            // here we only match the number of fields (this is not safe)
            if (real_type->getStructNumElements() != formal_type->getStructNumElements()) {
                return false;
            }
            if(!real_type->getStructName().equals(formal_type->getStructName())){
                return false;
            }
        }

        if(eager_func_signature_analysis){
            // TODO: instead, we should check type compatibility here
            if (real_type != formal_type) {
                return false;
            }
        }
        idx++;
    }

    return true;
}

uint64_t TypeAnalysis::getTypeSizeInBits(Type *Ty) {
    assert(Ty);
    switch (Ty->getTypeID()) {
        case Type::HalfTyID: return 16;
        case Type::FloatTyID: return 32;
        case Type::DoubleTyID: return 64;
        case Type::X86_FP80TyID: return 80;
        case Type::FP128TyID: return 128;
        case Type::PPC_FP128TyID: return 128;
        case Type::X86_MMXTyID: return 64;
        case Type::PointerTyID: return default_ptrsz;
        case Type::IntegerTyID: return Ty->getIntegerBitWidth();
        case Type::VectorTyID:
            return Ty->getVectorNumElements() * getTypeSizeInBits(Ty->getVectorElementType());
        case Type::ArrayTyID:
            return Ty->getArrayNumElements() * getTypeSizeInBits(Ty->getArrayElementType());
        case Type::StructTyID: {
            uint64_t Sz = 0;
            for (unsigned I = 0; I < Ty->getStructNumElements(); I++) {
                Sz += getTypeSizeInBits(Ty->getStructElementType(I));
            }
            return Sz;
        }
        default:
            return 0;
    }
}

void TypeAnalysis::printAnalysisInfo() {

    outs()<<"----------------------------------------------------------\n";
    Common::printBasicICStatistics(first_layer_icall_result);
    outs()<<"----------------------------------------------------------\n";
    Common::printBasicICStatistics(multi_layer_icall_result);
}


/*
 * If a function is address-taken, we track where the function address flows
 *  - First, if the function address is directly stored into a C structure,
 *     then we get the offset.
 *   - Second, we track how a function address can flow in a top-down way,
 *      to see where it is assigned to a function pointer field.
 *   Currently, we only track two-levels of direct calls
 */

int TypeAnalysis::addressTakenFuncStoreIndex(Function* func) {
    int ret = addressTakenFuncStoreIndexBase(func);
    if (ret != FUNC_STORED_IN_STRUCT_OFFSET) {
        return ret;
    } else {
        // Currently, we only track two-levels of direct calls.
        for (Value::const_use_iterator I = func->use_begin(), E = func->use_end(); I != E; ++I) {
            User* U = I->getUser ();
            if (isa<CallInst>(U)) {
                CallSite CS(cast<Instruction>(U));

                unsigned arg_size = CS.arg_size();
                unsigned index = UINT_MAX;
                for (unsigned i = 0; i < arg_size; i++) {
                    Value* val_i = CS.getArgument(i);
                    if (Function* func_i = dyn_cast<Function>(val_i)) {
                        index = i;
                        break;
                    }
                }
                if (index != UINT_MAX) {
                    Function* callee = CS.getCalledFunction();
                    if (callee) {
                        std::vector<Value*> callee_formal_args;
                        auto& arg_list = callee->getArgumentList();
                        for (Argument& arg_val : arg_list) {
                            callee_formal_args.push_back(&arg_val);
                        }
                        for (inst_iterator callee_i = inst_begin(callee), callee_e = inst_end(callee); callee_i != callee_e; ++callee_i) {
                            Instruction* inst = &*callee_i;
                            if (StoreInst* store = dyn_cast<StoreInst>(inst)) {
                                if (store->getNumOperands() >= 2) {
                                    if (store->getOperand(0) == callee_formal_args[index]) {
                                        if (GetElementPtrInst* gep = dyn_cast<GetElementPtrInst>(store->getOperand(1))) {
                                            int offset = getGepConstantOffset(gep);
                                            if (offset != CALLED_FUNC_IN_STRUCT_OFFSET)  return offset;
                                        }
                                    }
                                }
                            } else if (CallInst* call = dyn_cast<CallInst>(inst)) {

                                CallSite callee_cs(cast<Instruction>(inst));
                                unsigned lvltwo_index = UINT_MAX;
                                for (unsigned i = 0; i < callee_cs.arg_size(); i++) {
                                    if (callee_cs.getArgument(i) == callee_formal_args[index]) {
                                        lvltwo_index = i;
                                        break;
                                    }
                                }
                                if (lvltwo_index != UINT_MAX) {
                                    Function* callee_lvltwo = callee_cs.getCalledFunction();
                                    if (callee_lvltwo) {
                                        std::vector<Value* > callee_lvltwo_formal_args;
                                        for (Argument& arg_val : callee_lvltwo->getArgumentList()) {
                                            callee_lvltwo_formal_args.push_back(&arg_val);
                                        }
                                        for (inst_iterator callee_two_i = inst_begin(callee_lvltwo), callee_two_e = inst_end(callee_lvltwo);
                                             callee_two_i != callee_two_e; ++callee_two_i) {
                                            Instruction* inst_i = &*callee_two_i;
                                            if (StoreInst* store_inst_i = dyn_cast<StoreInst>(inst_i)) {
                                                if (store_inst_i->getNumOperands() >= 2) {
                                                    if (store_inst_i->getOperand(0) == callee_lvltwo_formal_args[lvltwo_index]) {
                                                        if (GetElementPtrInst* gep = dyn_cast<GetElementPtrInst>(store_inst_i->getOperand(1))) {
                                                            int offset = getGepConstantOffset(gep);
                                                            if (offset != CALLED_FUNC_IN_STRUCT_OFFSET) return offset;
                                                        }
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    return ret;
}

void TypeAnalysis::performFunctionSignatureAnalysis() {

    // function signature analysis
    for(auto icallIt=indirect_callsite.begin();icallIt!=indirect_callsite.end();icallIt++){
        CallInst* icall=*icallIt;
        std::set<Function*> result;
        CallSite cs(icall);
        guessCalleesForIndCallSite(cs,result); // too slow for large programs: O^2
        first_layer_icall_result[icall]=result;
    }
}
