#pragma once

#include <llvm/IR/Instructions.h>
#include <llvm/IR/Instruction.h>
#include <llvm/IR/CallSite.h>
#include <llvm/IR/Module.h>

#include <set>
#include <iostream>

using namespace llvm;
using namespace std;


class TypeAnalysis {
private:

    static llvm::Module* M;
    static TypeAnalysis* addrAA;
    uint64_t default_ptrsz = 0;

    // address-taken functions
    std::set<llvm::Function*> address_taken_functions;
    std::set<llvm::Function*> valid_address_taken_functions;
    std::set<llvm::Function*> type_matched_address_taken_functions; // functions that do not match to any indirect calls by using type analysis

    std::set<Function*> confined_functions;

    //indirect calls
    std::set<CallInst*> all_callsite;
    std::set<CallInst*> indirect_callsite;
    std::set<CallInst*> virtual_callsite;
    std::set<CallInst*> inline_asm_icall;
    std::set<CallInst*> stripped_icall;
    std::set<CallInst*> type_matched_icall; // indirect calls with address-taken functions matched by using types

    // address-taken information
    std::map<llvm::Function*, std::set<llvm::Instruction*>> address_taken_functions_info; // old ones

    std::map<llvm::Function*, std::set<llvm::User*>> address_taken_funcs_user_info;

    //std::map<llvm::Function*,std::set<User*>> valid_address_taken_functions_info; // newly added
    std::set<Function*> address_taken_once_functions;

    //indirect calls with their targets
    std::map<llvm::CallInst*,std::set<llvm::Function*>> first_layer_icall_result;
    std::map<llvm::CallInst*,std::set<llvm::Function*>> multi_layer_icall_result;

    // the current mostly precise one
    std::map<llvm::CallInst*,std::set<llvm::Function*>> latest_icall_result;

public:

    static TypeAnalysis* getTypeAnalysis(llvm::Module& _M) {
        if(addrAA == NULL) {
            addrAA = new TypeAnalysis(_M);
        }
        return addrAA;
    }

    void printAnalysisInfo();

private:

    TypeAnalysis(llvm::Module& _M);

    ~TypeAnalysis() {
    }

    // if a function is address-taken or not
    bool isAddressTaken(llvm::Value* V, std::set<Value*>);

    void getAddressTakenInfo(llvm::Value* V,std::set<llvm::Instruction*>&, std::set<llvm::User*>&);

    // check type compatibility
    bool isTypeCompatible(llvm::Type* t1, llvm::Type* t2);

    bool isCallsiteFunctionStrictCompatible(llvm::CallSite callsite, llvm::Function* func);

    // too slow for large programs with astronomical functions and icalls
    void performFunctionSignatureAnalysis();

    // the optimized function signature analysis
    void performFirstLayerTypeAnalysis();

    void performMultiLayerTypeAnalysis();

    // identifying all address-taken functions
    void identifyAllAddressTakenFunc(Module*);

    // identifying all address-taken functions except the ones used only at stripped indirect calls
    void identifyValidAddressTakenFunc(Module*);

    void identifyValidIndirectCall(Module*);

    // guess callees for indirect call sites
    bool guessCalleesForIndCallSite(llvm::CallSite callsite, std::set<llvm::Function*> &result);

    bool performFieldPruning(llvm::CallSite callsite, std::set<llvm::Function*> &result);

    int getGepConstantOffset(llvm::GetElementPtrInst* gep);

    // guess the struct index where function addresses are stored into.
    int addressTakenFuncStoreIndexBase(llvm::Function* func);

    int addressTakenFuncStoreIndex(llvm::Function* func);

    uint64_t getTypeSizeInBits(llvm::Type* Ty);


public:

    inline std::set<llvm::Function*> getAddressTakenFunctions() {
        return address_taken_functions;
    }

    inline std::map<llvm::CallInst*,std::set<llvm::Function*>> getFLTAResult(){
        return first_layer_icall_result;
    }


    inline std::map<llvm::CallInst*,std::set<llvm::Function*>> getMLTAResult(){
        return multi_layer_icall_result;
    }

    inline std::set<CallInst*> getAllStrippedICall(){
        return stripped_icall;
    }

    inline std::set<llvm::Function*> getMLTAResult(llvm::CallInst* icall){

        // this is a protection, since some icall are absent from the map that would occupy the map
        if(this->multi_layer_icall_result.find(icall)==multi_layer_icall_result.end()){
            std::set<llvm::Function*> nullSet;
            return nullSet;
        }

        return this->multi_layer_icall_result[icall];
    }


    inline std::set<llvm::Function*> getFLTAResult(llvm::CallInst* icall){

        // this is a protection, since some icall are absent from the map that would occupy the map
        if(this->first_layer_icall_result.find(icall)==first_layer_icall_result.end()){
            std::set<llvm::Function*> nullSet;
            return nullSet;
        }

        return this->first_layer_icall_result[icall];
    }


    inline void setLatestICallResult(std::map<llvm::CallInst*,std::set<llvm::Function*>> icall_result){
        this->latest_icall_result=icall_result;
    }

    inline std::set<llvm::Function*> getLatestICallResult(llvm::CallInst* icall){

        // this is a protection, since some icall are absent from the map that would occupy the map
        if(this->latest_icall_result.find(icall)==latest_icall_result.end()){
            std::set<llvm::Function*> nullSet;
            return nullSet;
        }

        return this->latest_icall_result[icall];
    }

    inline std::set<CallInst*> getAllIndirectCalls(){
      return indirect_callsite;
    }

    inline std::set<llvm::Function*> getValidAddrTakenFunc(){
        return valid_address_taken_functions;
    }



};



