#pragma once


#include <llvm/IR/Instructions.h>
#include <llvm/IR/Instruction.h>
#include <llvm/IR/CallSite.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/Operator.h>

#include <set>
#include <stack>
// #include <iostream>


using namespace llvm;
using namespace std;


typedef unsigned u32_t;
typedef unsigned NodeID;
typedef std::pair<NodeID, NodeID> NodePair;


/*!
 * Field information of an aggregate object
 */
class FldInfo {
public:
    typedef std::vector<NodePair > ElemNumStridePairVec;

private:
    u32_t offset;
    const llvm::Type* elemTy;
    ElemNumStridePairVec elemNumStridePair;

public:

    FldInfo(u32_t of, const llvm::Type* ty, ElemNumStridePairVec pa):
            offset(of), elemTy(ty), elemNumStridePair(pa) {
    }

    inline u32_t getFlattenOffset() const {
        return offset;
    }

    inline const llvm::Type* getFlattenElemTy() const {
        return elemTy;
    }

    inline const ElemNumStridePairVec& getElemNumStridePairVect() const {
        return elemNumStridePair;
    }

    inline ElemNumStridePairVec::const_iterator elemStridePairBegin() const {
        return elemNumStridePair.begin();
    }

    inline ElemNumStridePairVec::const_iterator elemStridePairEnd() const {
        return elemNumStridePair.end();
    }
};


/*!
 * Struct information
 */
class StructInfo {

private:

    // Offsets of all fields of a struct
    std::vector<u32_t> foffset;
    // All field infos after flattening a struct
    std::vector<FldInfo> finfo;

public:

    // Constructor
    StructInfo() {

    }

    // Destructor
    ~StructInfo() {
    }

    // Get method for fields of a struct
    std::vector<u32_t>& getFieldOffsetVec() {
        return foffset;
    }

    std::vector<FldInfo>& getFlattenFieldInfoVec() {
        return finfo;
    }

};


typedef llvm::DenseMap<const llvm::Type*, StructInfo*> TypeToFieldInfoMap;

class SimpleFPAnalysis {

public:

    static SimpleFPAnalysis* getSimpleFPAnalysis(llvm::Module& _M) {
        if(simpleAA == NULL) {
            simpleAA = new SimpleFPAnalysis(_M);
        }
        return simpleAA;
    }

private:

    static llvm::Module* M;
    static SimpleFPAnalysis* simpleAA;

    // all indirect calls
    std::set<CallInst*> valid_icall_set;

    // simple indirect calls
    std::set<CallInst*> simple_icall_set;

    std::set<Function*> valid_address_taken_functions;

    // pure type analysis
    std::map<llvm::CallInst*,std::set<llvm::Function*>> multi_layer_icall_result;

    // flow-sensitive indirect-call results
    std::map<llvm::CallInst*,std::set<llvm::Function*>> fs_simple_icall_result;

    // flow-insensitive indirect-call results
    std::map<llvm::CallInst*,std::set<llvm::Function*>> fi_simple_icall_result;

    // flow-sensitive indirect-call results with contextual information
    std::map<llvm::CallInst*,std::set<std::pair<llvm::Function*, std::vector<CallInst*>>>> fs_simple_icall_result_ctx;

    llvm::DenseMap<const llvm::Type*, StructInfo*>  typeToFieldInfo;


    // globals
    std::set<GlobalVariable*> gv_with_function_ptr; // the global variables containing function pointers
    std::map<GlobalVariable*, std::map<Function*, u32_t>> gv_map_to_function; // the global variables mapping to their stored function pointers

    // constants stored into non-globals
    std::set<Constant*> con_with_function_ptr; // the constants containing function pointers
    std::map<Constant*, std::map<Function*, u32_t>> con_map_to_function; // the constants mapping to their stored function pointers

    // homogenous functions
    std::set<std::set<Function*>> homo_func;

    // address-taken to functions
    std::set<Instruction*> address_taken_sites;
    std::map<Instruction*, std::set<Function*>> address_taken_site_to_func;

public:

    inline bool isSimpleICall(CallInst* iCall){
        if(simple_icall_set.count(iCall)){
            return true;
        }
        return false;
    }

    inline std::map<llvm::CallInst*,std::set<llvm::Function*>> getSimpleFPICallResult(){
        if(!fs_simple_icall_result.empty()){
            return fs_simple_icall_result;
        }else{
            return fi_simple_icall_result;
        }
    }

    inline bool isPreciseICallTargets(std::set<Function*> callees){
        if(callees.size()==1){
            return true;
        }
        if(homo_func.find(callees)!=homo_func.end()){
            return true;
        }
        return false;
    };

    inline std::map<GlobalVariable*, std::map<Function*, u32_t>> getGVMapToFuncSet(){
        return gv_map_to_function;
    }

    inline std::map<Constant*, std::map<Function*, u32_t>> getConMapToFuncSet(){
        return con_map_to_function;
    }

    inline std::set<GlobalVariable*> getGVWithFP(){
        return gv_with_function_ptr;
    }

    inline std::set<Constant*> getConWithFP(){
        return con_with_function_ptr;
    }

    inline std::set<Instruction*> getAllValidAddressTakenSites(){
        return address_taken_sites;
    }

    inline std::map<Instruction*, std::set<Function*>> getAllValidAddressTakenSiteInfo(){
        return address_taken_site_to_func;
    }

private:

    SimpleFPAnalysis(llvm::Module& _M);

    void performFSDefUseAnalysis();

    // flow-insensitive def-use analysis
    void backwardFSPTAnalysis(llvm::Instruction*, std::set<Value*>& relatedDU, std::set<BasicBlock*>& visitedBB,
                            std::vector<CallInst*>& callTrace, std::stack<CallInst*>& callStack);

    void backwardFSCSPTAnalysis(llvm::Instruction*, std::set<Value*>& relatedDU, std::set<BasicBlock*>& visitedBB,
                                std::vector<CallInst*>& callTrace, std::stack<CallInst*>& callStack);

    void handleInstructionFS(Instruction*, std::set<Value*>& relatedDU, std::set<BasicBlock*>& visitedBB,
                           std::vector<CallInst*>& callTrace, std::stack<CallInst*>& callStack);

    void handleConstantFS(Value*, std::set<Value*>& relatedDU, std::set<BasicBlock*>& visitedBB,
                              std::vector<CallInst*>& callTrace, std::stack<CallInst*>& callStack);

    void handle_gep(GEPOperator* gep);

    // Handle constant expression
    void handleCE(const llvm::Value *val, std::vector<CallInst*>& callTrace);

    void identifyFuncAddressTakenSite();

    void identifyHomogenousFunc();

    void localAnalysisForHomoFunc();

    inline void addRelatedDU(Value* val, std::set<Value*>& relatedDU){
        for(auto it=val->user_begin();it!=val->user_end();it++){
            relatedDU.insert(val);
        }
    }

private:

    // globally analyzing address-taken functions
    void computeGlobalStorage(Module* M);

    //tracking the address-taken functions stored in constants
    void trackFunctionInConstant(Constant * con, std::set<Value *> &visited, std::map<Function*, u32_t>& function_info, u32_t offset);

    void analyzeGlobalVariables(llvm::Module& _M);

    void analyzeGlobalInitializers(const Constant *c);

private:


    // Get an iterator for StructInfo, designed as internal methods
    TypeToFieldInfoMap::iterator getStructInfoIter(const llvm::Type *T) {
        assert(T);
        TypeToFieldInfoMap::iterator it = typeToFieldInfo.find(T);
        if (it != typeToFieldInfo.end())
            return it;
        else {
            collectTypeInfo(T);
            return typeToFieldInfo.find(T);
        }
    }

    // Get a reference to StructInfo.
    inline StructInfo* getStructInfo(const llvm::Type *T) {
        return getStructInfoIter(T)->second;
    }

    // Get a reference to the components of struct_info.
    const inline std::vector<u32_t>& getStructOffsetVec(const llvm::Type *T) {
        return getStructInfoIter(T)->second->getFieldOffsetVec();
    }

    const inline std::vector<FldInfo>& getFlattenFieldInfoVec(const llvm::Type *T) {
        return getStructInfoIter(T)->second->getFlattenFieldInfoVec();
    }

private:

    // Collect type info
    void collectTypeInfo(const llvm::Type* T);

    // Collect the struct info
    void collectStructInfo(const llvm::StructType *T);

    // Collect the array info
    void collectArrayInfo(const llvm::ArrayType* T);

    //  Collect simple type (non-aggregate) info
    void collectSimpleTypeInfo(const llvm::Type* T);

    inline const llvm::ConstantExpr *isGepConstantExpr(const llvm::Value *val) {
        if(const llvm::ConstantExpr* constExpr = llvm::dyn_cast<llvm::ConstantExpr>(val)) {
            if(constExpr->getOpcode() == llvm::Instruction::GetElementPtr)
                return constExpr;
        }
        return NULL;
    }

    inline const llvm::ConstantExpr *isCastConstantExpr(const llvm::Value *val) {
        if(const llvm::ConstantExpr* constExpr = llvm::dyn_cast<llvm::ConstantExpr>(val)) {
            if(constExpr->getOpcode() == llvm::Instruction::BitCast)
                return constExpr;
        }
        return NULL;
    }

    inline const llvm::ConstantExpr *isSelectConstantExpr(const llvm::Value *val) {
        if(const llvm::ConstantExpr* constExpr = llvm::dyn_cast<llvm::ConstantExpr>(val)) {
            if(constExpr->getOpcode() == llvm::Instruction::Select)
                return constExpr;
        }
        return NULL;
    }


private:

    // flow-insensitive def-use analysis
    void performFIDefUseAnalysis();

    void handleInstructionFI(CallInst* icall, Value* val, std::set<Value*>& visited, std::set<Function*>& _callee_info);

    void handleConstantFI(CallInst *icall, Constant *con, std::set<Value *> &visited, std::set<Function*>& _callee_info);

};


