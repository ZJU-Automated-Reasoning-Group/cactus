#pragma once

#include "DyckAliasAnalysis.h"
#include "EdgeLabel.h"

#include "FPAnalysis/TypeAnalysis.h"
#include "SparrowUtils/ProgressBar.h"

#include <unordered_map>
#include <map>


using namespace std;
using namespace llvm;
using namespace Sparrow;

namespace Steensgaard{


    typedef struct FunctionTypeNode {
        FunctionType * type;
        FunctionTypeNode * root;
        set<Function *> compatibleFuncs;
    } FunctionTypeNode;

    class AAAnalyzer {
    private:
        Module* module;
        DyckAliasAnalysis* aa;
        DyckGraph* dgraph;
        DyckCallGraph* callgraph;
        TypeAnalysis* addrAA;
        ProgressBar progress_bar;

    private:
        map<Type*, FunctionTypeNode*> functionTyNodeMap;
        set<FunctionTypeNode *> tyroots;

        std::set<StructType*> struct_contained_fp_types_cache;

        //DyckAA::ProgressBar PB;

    public:
        AAAnalyzer(Module* m, DyckAliasAnalysis* a, DyckGraph* d, DyckCallGraph* cg);
        ~AAAnalyzer();

        void start_intra_procedure_analysis();
        void end_intra_procedure_analysis();

        void start_inter_procedure_analysis();
        void end_inter_procedure_analysis();

        void intra_procedure_analysis();
        void inter_procedure_analysis();

        void handle_global_variables();

    private:
        void printNoAliasedPointerCalls();

    private:
        void handle_inst(Instruction *inst, DyckCallGraphNode * parent);
        void handle_instrinsic(Instruction *inst);
        void handle_extract_insert_value_inst(Value* aggValue, Type* aggTy, ArrayRef<unsigned>& indices, Value* insertedOrExtractedValue);
        void handle_extract_insert_elmt_inst(Value* vec, Value* elmt);
        void handle_invoke_call_inst(Instruction * ret, Value* cv, vector<Value*>* args, DyckCallGraphNode * parent);
        void handle_lib_invoke_call_inst(Value* ret, Function* f, vector<Value*>* args, DyckCallGraphNode* parent);

    private:
        bool handle_pointer_function_calls(DyckCallGraphNode* caller, int counter);
        void handle_common_function_call(Call* c, DyckCallGraphNode* caller, DyckCallGraphNode* callee);

    private:
        int isCompatible(FunctionType * t1, FunctionType * t2);

        set<Function*>* getCompatibleFunctions(FunctionType * fty);
        void getCompatibleFunctions(CallInst * icall, set<Function*>&);

        FunctionTypeNode* initFunctionGroup(FunctionType* fty);
        void initFunctionGroups();
        void destroyFunctionGroups();
        void combineFunctionGroups(FunctionType * ft1, FunctionType* ft2);

        void collectFPStructs(Module& M);

    private:
        DyckVertex* addField(DyckVertex* val, long fieldIndex, DyckVertex* field);
        DyckVertex* addPtrTo(DyckVertex* address, DyckVertex* val);
        DyckVertex* makeAlias(DyckVertex* x, DyckVertex* y);
        void makeContentAlias(DyckVertex* x, DyckVertex* y);

        DyckVertex* handle_gep(GEPOperator* gep);
        DyckVertex* wrapValue(Value * v);
    };

}


