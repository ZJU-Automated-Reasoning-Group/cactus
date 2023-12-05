#pragma once

#include <set>
#include <vector>
#include <string>
#include <llvm/Analysis/ScalarEvolution.h>
#include <llvm/Analysis/AliasAnalysis.h>
#include <llvm/Analysis/LoopInfo.h>
#include <llvm/IR/Instructions.h>

using namespace llvm;
using namespace std;


class Common{

public:

    static void printICStatistics(std::string, std::map<CallInst*,std::set<llvm::Function*>> );

    static void printBasicICStatistics(std::map<CallInst*,std::set<llvm::Function*>> );

    static float getAverageCalleeSize(std::map<CallInst*,std::set<llvm::Function*>>);

    static void dumpICDetailedInfo(std::map<CallInst*,std::set<llvm::Function*>> );

    static void readICDetailedInfo(string filePath );

    static void dumpICDetailedInfoInJson(std::map<CallInst*,std::set<llvm::Function*>> );

    static void printInstLoc(Instruction*);

    static bool isFunctionPointerType(Type *type);

    static bool isArrayFPType(Type *type);

    static bool isIndirectCallSite(CallInst*);

    static bool isStripFunctionPointerCasts(CallInst*);

    static bool isVirtualCallSite(CallSite cs);

    static bool isVirtualCallSite(CallInst* CI);

    static string LCSubStr(string X, string Y);

    static set<string> split(const string &str, const string &pattern);

    static set<string> splitByCapital(const string &str);

    static Type* getPureType(Type*);

    static std::string demangle(const char* name);


    //  checking the similarity between two strings

    static std::string lcs(std::string* src, std::string* dest);

    static int lcs_len(std::string* src, std::string* dest);

    static float lcs_score(std::string* src, std::string* dest);

    static int levenshtein(std::string* src, std::string* dest);

    static int damerau_levenshtein(std::string* src, std::string* dest);


    static const Function* findEnclosingFunc(const Value* V);

    static const MDNode* findVar(const Value* V, const Function* F);

    static StringRef getOriginalName(const Value* V);

};



