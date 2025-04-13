#pragma once

#include <llvm/IR/Instructions.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/CallSite.h>
#include <llvm/IR/Constants.h>

/*
 * Util class to assist pointer analysis for cpp programs
 */

namespace cppUtil {

    struct DemangledName {
        std::string className;
        std::string funcName;
    };

    struct DemangledName demangle(const std::string name);

    std::string getBeforeBrackets(const std::string name);
    bool isValVtbl(const llvm::Value *val);
    bool isLoadVtblInst(const llvm::LoadInst *loadInst);
    bool isVirtualCallSite(llvm::CallSite cs);
    bool isConstructor(const llvm::Function *F);
    bool isDestructor(const llvm::Function *F);

    const llvm::Value *getVCallThisPtr(llvm::CallSite cs);
    const llvm::Value *getVCallVtblPtr(llvm::CallSite cs);
    std::size_t getVCallIdx(llvm::CallSite cs);

    std::string getClassNameFromType(const llvm::Type *ty);
    std::string getClassNameFromVtblVal(const llvm::Value *value);

}


