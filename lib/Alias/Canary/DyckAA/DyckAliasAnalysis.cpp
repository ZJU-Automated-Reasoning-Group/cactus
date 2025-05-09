#define DEBUG_TYPE "dyckaa"
#include <stdio.h>
#include <algorithm>
#include <stack>
#include <list>
#include <iomanip>

#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/FileSystem.h"

#include "Alias/Canary/DyckAA/DyckAliasAnalysis.h"
#include "Alias/Canary/DyckCG/DyckCallGraph.h"
#include "Alias/Canary/DyckAA/AAAnalyzer.h"
#include "Alias/Canary/DyckAA/EdgeLabel.h"
#include "Alias/Canary/DyckGraph/DyckGraph.h"
#include "FPAnalysis/TypeAnalysis.h"

#include "Util/SparrowUtils/Profiler.h"
#include "Util/SparrowUtils/Common.h"


using namespace Canary;

static cl::opt<bool> PrintAliasSetInformation("sparrow-print-alias-set-info", cl::init(false), cl::Hidden,
                                              cl::desc("Output all alias sets, their relations and the evaluation results."));

static cl::opt<bool> PreserveCallGraph("sparrow-preserve-dyck-callgraph", cl::init(false), cl::Hidden,
                                       cl::desc("Preserve the call graph for usage in other passes."));

static cl::opt<bool> DotCallGraph("sparrow-dot-dyck-callgraph", cl::init(false), cl::Hidden,
                                  cl::desc("Calculate the program's call graph and output into a \"dot\" file."));

static cl::opt<bool> CountFP("sparrow-count-fp", cl::init(false), cl::Hidden, cl::desc("Calculate how many functions a function pointer may point to."));



static const Function *getParent(const Value *V) {
    if (const Instruction * inst = dyn_cast<Instruction>(V))
        return inst->getParent()->getParent();

    if (const Argument * arg = dyn_cast<Argument>(V))
        return arg->getParent();

    return NULL;
}

static bool notDifferentParent(const Value *O1, const Value *O2) {

    const Function *F1 = getParent(O1);
    const Function *F2 = getParent(O2);

    return !F1 || !F2 || F1 == F2;
}

DyckAliasAnalysis::DyckAliasAnalysis() :
        ModulePass(ID) {

    dyck_graph = new DyckGraph;
    call_graph = new DyckCallGraph;
    DEREF_LABEL = new DerefEdgeLabel;

}

DyckAliasAnalysis::~DyckAliasAnalysis() {
    delete call_graph;
    delete dyck_graph;

    // delete edge labels
    delete DEREF_LABEL;

    auto olIt = OFFSET_LABEL_MAP.begin();
    while (olIt != OFFSET_LABEL_MAP.end()) {
        delete olIt->second;
        olIt++;
    }

    auto ilIt = INDEX_LABEL_MAP.begin();
    while (ilIt != INDEX_LABEL_MAP.end()) {
        delete ilIt->second;
        ilIt++;
    }

    for (auto& it : vertexMemAllocaMap) {
        delete it.second;
    }
}

void DyckAliasAnalysis::getAnalysisUsage(AnalysisUsage &AU) const {
    AU.setPreservesAll();
    AU.addRequired<AliasAnalysis>();
    AU.addRequired<TargetLibraryInfo>();
    AU.addRequired<DataLayoutPass>();
}


DyckAliasAnalysis::AliasResult DyckAliasAnalysis::alias(const Location &LocA, const Location &LocB) {
    if (LocA.Ptr->stripPointerCastsNoFollowAliases() == LocB.Ptr->stripPointerCastsNoFollowAliases()) {
        return MustAlias;
    }

    AliasResult ret = MayAlias;
    if (notDifferentParent(LocA.Ptr, LocB.Ptr)) {
        ret = AliasAnalysis::alias(LocA, LocB);
        if (ret != MayAlias) {
            return ret;
        }
    }

    if ((isa<Argument>(LocA.Ptr) && ((const Argument*) LocA.Ptr)->getParent()->empty())
        || (isa<Argument>(LocB.Ptr) && ((const Argument*) LocB.Ptr)->getParent()->empty())) {
        outs() << "[WARNING] Arguments of empty functions are not supported, MAYALIAS is returned!\n";
        return ret;
    }

    pair<DyckVertex*, bool> retpair = dyck_graph->retrieveDyckVertex(const_cast<Value*>(LocA.Ptr));
    DyckVertex * VA = retpair.first;

    retpair = dyck_graph->retrieveDyckVertex(const_cast<Value*>(LocB.Ptr));
    DyckVertex * VB = retpair.first;

    if (VA == VB) {
        ret = MayAlias;
    } else if (isPartialAlias(VA, VB) || isPartialAlias(VB, VA)) {
        ret = PartialAlias;
    } else {
        ret = NoAlias;
    }

    if (ret == MayAlias && (isa<Function>(LocA.Ptr) || isa<Function>(LocB.Ptr))) {
        const Function* function = isa<Function>(LocA.Ptr) ? (const Function*) LocA.Ptr : (const Function*) LocB.Ptr;
        const Value* calledValue = function == LocA.Ptr ? LocB.Ptr : LocA.Ptr;

        Value * cvcopy = const_cast<Value*>(calledValue);
        Value * temp = cvcopy;
        do {
            temp = cvcopy;

            while (isa<ConstantExpr>(cvcopy) && ((ConstantExpr*) cvcopy)->isCast()) {
                cvcopy = ((ConstantExpr*) cvcopy)->getOperand(0)->stripPointerCastsNoFollowAliases();
            }

            while (isa<Instruction>(cvcopy) && ((Instruction*) cvcopy)->isCast()) {
                cvcopy = ((Instruction*) cvcopy)->getOperand(0)->stripPointerCastsNoFollowAliases();
            }

            while (isa<GlobalAlias>(cvcopy)) {
                cvcopy = ((GlobalAlias*) cvcopy)->getAliasee()->stripPointerCastsNoFollowAliases();
            }

        } while (cvcopy != temp);

        if (isa<Function>(cvcopy)) {
            if (cvcopy == function) {
                return MustAlias;
            } else {
                return NoAlias;
            }
        }
    }

    return ret;
}


static RegisterAnalysisGroup<AliasAnalysis> Y("Alias Analysis");


static RegisterPass<Canary::DyckAliasAnalysis> X("dyckaa", "Dyck-CFL-Reachability Alias Analysis",true,false);




// Register this pass...
char DyckAliasAnalysis::ID = 0;

const set<Value*>* DyckAliasAnalysis::getAliasSet(Value * ptr) const {
    DyckVertex* v = dyck_graph->retrieveDyckVertex(ptr).first;
    return (const set<Value*>*) v->getEquivalentSet();
}

bool DyckAliasAnalysis::isPartialAlias(DyckVertex *v1, DyckVertex * v2) {
    if (v1 == NULL || v2 == NULL)
        return false;

    if (v1 == v2)
        return false;

    set<DyckVertex*> visited;
    stack<DyckVertex*> workStack;
    workStack.push(v1);

    while (!workStack.empty()) {
        DyckVertex* top = workStack.top();
        workStack.pop();

        // have visited
        if (visited.find(top) != visited.end()) {
            continue;
        }

        if (top == v2) {
            return true;
        }

        visited.insert(top);

        { // push out tars
            set<void*>& outlabels = top->getOutLabels();
            set<void*>::iterator olIt = outlabels.begin();
            while (olIt != outlabels.end()) {
                EdgeLabel* labelValue = (EdgeLabel*) (*olIt);
                if (labelValue->isLabelTy(EdgeLabel::OFFSET_TYPE)) {
                    set<DyckVertex*>* tars = top->getOutVertices(*olIt);

                    set<DyckVertex*>::iterator tit = tars->begin();
                    while (tit != tars->end()) {
                        // if it has not been visited
                        if (visited.find(*tit) == visited.end()) {
                            workStack.push(*tit);
                        }
                        tit++;
                    }

                }
                olIt++;
            }
        }
    }

    return false;
}

void DyckAliasAnalysis::getEscapedPointersFrom(std::vector<const set<Value*>*>* ret, Value * from) {
    assert(ret != NULL);

    set<DyckVertex*> temp;
    getEscapedPointersFrom(&temp, from);

    auto tempIt = temp.begin();
    while (tempIt != temp.end()) {

        DyckVertex* t = *tempIt;
        ret->push_back((const set<Value*>*) t->getEquivalentSet());
        tempIt++;
    }
}

void DyckAliasAnalysis::getEscapedPointersFrom(set<DyckVertex*>* ret, Value * from) {
    assert(ret != NULL);
    assert(from != NULL);
    if (isa<Argument>(from)) {
        assert(!((Argument* ) from)->getParent()->empty());
    }

    set<DyckVertex*>& visited = *ret;
    stack<DyckVertex*> workStack;

    workStack.push(dyck_graph->retrieveDyckVertex(from).first);

    while (!workStack.empty()) {
        DyckVertex* top = workStack.top();
        workStack.pop();

        // have visited
        if (visited.find(top) != visited.end()) {
            continue;
        }

        visited.insert(top);

        set<DyckVertex*> tars;
        top->getOutVertices(&tars);
        set<DyckVertex*>::iterator tit = tars.begin();
        while (tit != tars.end()) {
            // if it has not been visited
            if (visited.find(*tit) == visited.end()) {
                workStack.push(*tit);
            }
            tit++;
        }
    }
}

void DyckAliasAnalysis::getEscapedPointersTo(std::vector<const set<Value*>*>* ret, Function * func) {

    assert(ret != NULL);
    set<DyckVertex*> temp;
    getEscapedPointersTo(&temp, func);
    auto tempIt = temp.begin();

    while (tempIt != temp.end()) {
        DyckVertex* t = *tempIt;
        ret->push_back((const set<Value*>*) t->getEquivalentSet());
        tempIt++;
    }

}

void DyckAliasAnalysis::getEscapedPointersTo(set<DyckVertex*>* ret, Function * func) {
    assert(ret != NULL);
    assert(func != NULL);

    Module* module = func->getParent();

    set<DyckVertex*>& visited = *ret;
    stack<DyckVertex*> workStack;

    iplist<GlobalVariable>::iterator git = module->global_begin();
    while (git != module->global_end()) {
        if (!git->hasPrivateLinkage() && !git->getName().startswith("llvm.") && git->getName().str() != "stderr"
            && git->getName().str() != "stdout") { // in fact, no such symbols in src codes.
            DyckVertex * rt = dyck_graph->retrieveDyckVertex(git).first;
            workStack.push(rt);
        }
        git++;
    }

    for (ilist_iterator<Function> iterF = module->getFunctionList().begin(); iterF != module->getFunctionList().end(); iterF++) {
        Function* f = iterF;
        for (ilist_iterator<BasicBlock> iterB = f->getBasicBlockList().begin(); iterB != f->getBasicBlockList().end(); iterB++) {
            for (ilist_iterator<Instruction> iterI = iterB->getInstList().begin(); iterI != iterB->getInstList().end(); iterI++) {
                Instruction *rawInst = iterI;
                if (isa<CallInst>(rawInst)) {
                    CallInst *inst = (CallInst*) rawInst; // all invokes are lowered to call
                    AliasResult ar = this->alias(func, inst->getCalledValue());
                    if (ar == MayAlias || ar == MustAlias) {
                        if (func->hasName() && func->getName() == "pthread_create") {
                            DyckVertex * rt = dyck_graph->retrieveDyckVertex(inst->getArgOperand(3)).first;
                            workStack.push(rt);
                        } else {
                            unsigned num = inst->getNumArgOperands();
                            for (unsigned i = 0; i < num; i++) {
                                DyckVertex * rt = dyck_graph->retrieveDyckVertex(inst->getArgOperand(i)).first;
                                workStack.push(rt);
                            }
                        }
                    }
                }
            }
        }
    }

    while (!workStack.empty()) {
        DyckVertex* top = workStack.top();
        workStack.pop();

        // have visited
        if (visited.find(top) != visited.end()) {
            continue;
        }

        visited.insert(top);

        set<DyckVertex*> tars;
        top->getOutVertices(&tars);
        set<DyckVertex*>::iterator tit = tars.begin();
        while (tit != tars.end()) {
            // if it has not been visited
            DyckVertex* dv = (*tit);
            if (visited.find(dv) == visited.end()) {
                workStack.push(dv);
            }
            tit++;
        }
    }
}

void DyckAliasAnalysis::getPointstoObjects(std::set<Value*>& objects, Value* pointer) {
    assert(pointer != nullptr);

    DyckVertex * rt = dyck_graph->retrieveDyckVertex(pointer).first;
    auto tars = rt->getOutVertices(DEREF_LABEL);
    if (tars != nullptr && !tars->empty()) {
        assert(tars->size() == 1);
        set<DyckVertex*>::iterator tit = tars->begin();
        DyckVertex* tar = (*tit);
        auto vals = tar->getEquivalentSet();
        for (auto& val : *vals) {
            objects.insert((Value*) val);
        }
    }
}

bool DyckAliasAnalysis::isDefaultMemAllocaFunction(Value* calledValue) {
    if (isa<Function>(calledValue)) {
        if(mem_allocas.count((Function*)calledValue)){
            return true;
        }
    } else {
        for (auto& func : mem_allocas) {
            if (this->alias(calledValue, func)) {
                return true;
            }
        }
    }

    return false;
}

std::vector<Value*>* DyckAliasAnalysis::getDefaultPointstoMemAlloca(Value* ptr) {
    assert(ptr->getType()->isPointerTy());

    DyckVertex* v = dyck_graph->retrieveDyckVertex(ptr).first;
    if (vertexMemAllocaMap.count(v)) {
        return vertexMemAllocaMap[v];
    }

    std::vector<Value*>* objects = new std::vector<Value*>;
    vertexMemAllocaMap[v] = objects;

    // find allocas in v self
    auto aliases = (const set<Value*>*) v->getEquivalentSet();

    for (auto& al : *aliases) {
        if (isa<GlobalVariable>(al) || isa<Function>(al)) {
            objects->push_back(al);
        } else if (isa<AllocaInst>(al)) {
            objects->push_back(al);
        } else if (isa<CallInst>(al) || isa<InvokeInst>(al)) {
            CallSite cs(al);
            Value* calledValue = cs.getCalledValue();
            if (isDefaultMemAllocaFunction(calledValue)) {
                objects->push_back(al);
            }
        }
    }

    return objects;
}

bool DyckAliasAnalysis::callGraphPreserved() {
    return PreserveCallGraph;
}

DyckCallGraph* DyckAliasAnalysis::getCallGraph() {
    assert(this->callGraphPreserved() && "Please add -preserve-dyck-callgraph option when using opt or canary.\n");
    return call_graph;
}

bool DyckAliasAnalysis::runOnModule(Module & M) {

    //performDyckAliasAnalysis(M);

    return false;
}

void DyckAliasAnalysis::performDyckAliasAnalysis(Module & M) {

    //Profiler TimeMemProfiler(Profiler::TIME|Profiler::MEMORY);

    InitializeAliasAnalysis(this);

    {
        auto addAllocLikeFunc = [this, &M](const char* name) {
        if (Function* F = M.getFunction(name)) {
            this->mem_allocas.insert(F);
        }
    };
    addAllocLikeFunc("malloc");
    addAllocLikeFunc("calloc");
    addAllocLikeFunc("realloc");
    addAllocLikeFunc("valloc");
    addAllocLikeFunc("reallocf");
    addAllocLikeFunc("strdup");
    addAllocLikeFunc("strndup");
    addAllocLikeFunc("_Znaj");
    addAllocLikeFunc("_ZnajRKSt9nothrow_t");
    addAllocLikeFunc("_Znam");
    addAllocLikeFunc("_ZnamRKSt9nothrow_t");
    addAllocLikeFunc("_Znwj");
    addAllocLikeFunc("_ZnwjRKSt9nothrow_t");
    addAllocLikeFunc("_Znwm");
    addAllocLikeFunc("_ZnwmRKSt9nothrow_t");
    }

    AAAnalyzer* aaa = new AAAnalyzer(&M, this, dyck_graph, call_graph);

    //aaa->handle_global_variables();

    clock_t start1, end1;
    start1=clock();
    //outs() << "[Canary] Start intra-procedure analysis...\n";
    /// step 1: intra-procedure analysis
    aaa->start_intra_procedure_analysis();
    aaa->intra_procedure_analysis();
    //outs() << "Done!\n\n";
    aaa->end_intra_procedure_analysis();
    end1= clock();
    double total_time1=(double)(end1-start1)/CLOCKS_PER_SEC;
    //cout<<"[Canary] Intra-procedure analysis is finished in "<<fixed<<setprecision(2)<<(float)total_time1<<" s"<<endl;




    clock_t start2, end2;
    start2=clock();
    //outs() << "[Canary] Start inter-procedure analysis...\n";
    /// step 2: inter-procedure analysis
    aaa->start_inter_procedure_analysis();
    aaa->inter_procedure_analysis();
    //outs() << "\nDone!\n\n";
    aaa->end_inter_procedure_analysis();
    end2= clock();
    double total_time2=(double)(end2-start2)/CLOCKS_PER_SEC;
    //cout<<"[Canary] Inter-procedure analysis is finished in "<<fixed<<setprecision(2)<<(float)total_time2<<" s"<<endl;

    collectFPInfoFromDyckGraph(M);

    //collectThreadEscapedObjects(M);

    /* call graph */
    if (DotCallGraph) {
        outs() << "Printing call graph...\n";
        call_graph->dotCallGraph(M.getModuleIdentifier());
        outs() << "Done!\n\n";
    }

    if (CountFP) {
        outs() << "Printing function pointer information...\n";
        call_graph->printFunctionPointersInformation(M.getModuleIdentifier());
        outs() << "Done!\n\n";
    }

    delete aaa;
    aaa = NULL;

    if (!this->callGraphPreserved()) {
        delete this->call_graph;
        this->call_graph = NULL;
    }

    if (PrintAliasSetInformation) {
        outs() << "Printing alias set information...\n";
        this->printAliasSetInformation(M);
        outs() << "Done!\n\n";
    }

    // DEBUG_WITH_TYPE("validate-dyckgraph", dyck_graph->validation(__FILE__, __LINE__));


    //TimeMemProfiler.create_snapshot();
    //TimeMemProfiler.print_snapshot_result(outs(), "[Canary]");


    //outs()<<"[Canary]: 100%!\n";

}

void DyckAliasAnalysis::collectFPInfoFromDyckGraph(Module &M) {


    std::set<Value*> calledValSet;

    TypeAnalysis* addrAA=TypeAnalysis::getTypeAnalysis(M);

    for(auto iter=call_graph->begin();iter!=call_graph->end();iter++){
        DyckCallGraphNode* node=&*iter->second;
        std::set<PointerCall*> icallSet=node->getPointerCalls();
        for(auto iter2=icallSet.begin();iter2!=icallSet.end();iter2++){
            PointerCall* icall=*iter2;
            CallInst* CI=(CallInst*)icall->instruction;
            if(!CI){
                continue;
            }
            set<Function*> callees=icall->mayAliasedCallees;
            for(auto iter3=callees.begin();iter3!=callees.end();iter3++){
                if(dyn_cast<Function>(*iter3)){
                    //Value* callee=*iter3;
                    Function* callee= dyn_cast<Function>(*iter3);
                    icall_result[CI].insert(callee);
                }
            }
            if(callees.empty()){
                icall_result[CI]=addrAA->getMLTAResult(CI);
            }


            if(Common::isIndirectCallSite(CI)){
                calledValSet.insert(icall->calledValue);
            }

            // FIXME: The results from getAliasSet and mayAliasedCallees are different...
//            if(icall->instruction){
//                if(CallInst* icallInst=dyn_cast<CallInst>(icall->instruction)){
//                    if(!icallInst->getCalledFunction()){
//                        CallSite cs(icallInst);
//                        if (!Common::isIndirectCallSite(icallInst)) {
//                            continue;
//                        } else {
//                            icall_result[icallInst];
//                            const set<Value*>* pointsToSet=this->getAliasSet(icallInst->getCalledValue());
//                            for(auto iter3=pointsToSet->begin();iter3!=pointsToSet->end();iter3++){
//                                if(dyn_cast<Function>(*iter3)){
//                                    //Value* callee=*iter3;
//                                    Function* callee= dyn_cast<Function>(*iter3);
//                                    icall_result[icallInst].insert(callee);
//                                }
//                            }
//                        }
//                    }
//                }
//            }
        }
    }

    for(auto it=calledValSet.begin();it!=calledValSet.end();it++){
        Value* val=*it;
        if(fp_related_values.count(val)){
            continue;
        }
        const set<Value*>* aliasSet=this->getAliasSet(val);
        for(auto aliasIt=aliasSet->begin();aliasIt!=aliasSet->end();aliasIt++){
            fp_related_values.insert(*aliasIt);
        }
    }

    std::set<void*> fpRelatedValSet=dyck_graph->getFPResultValueSet();
    for(auto it=fpRelatedValSet.begin();it!=fpRelatedValSet.end();it++){
        Value* val=(Value*)*it;
        if(val){
            fp_related_values.insert(val);
        }
    }
}


void DyckAliasAnalysis::collectThreadEscapedObjects(Module &M) {

    int thread_create_site=0;
    int obj_num=0;
    std::set<Value*> sharedValSet;

    std::list<Function*> worklist;

    for(auto funcIt=M.begin();funcIt!=M.end();funcIt++){
        Function* func=funcIt;
        for(auto bbIt=func->begin();bbIt!=func->end();bbIt++){
            BasicBlock* bb=bbIt;
            for(auto instIt=bb->begin();instIt!=bb->end();instIt++){
                Instruction* I=instIt;
                if(CallInst* call= dyn_cast<CallInst>(I)){
                    if(Function* callee=call->getCalledFunction()){
                        if(!callee->hasName()){
                            continue;
                        }
                        if (callee->getName() == "pthread_create"){
                            thread_create_site++;
                            Value* val=call->getArgOperand(3);
                            std::set<Value*> objSet;
                            this->getPointstoObjects(objSet, val);
                            for(auto objIt=objSet.begin();objIt!=objSet.end();objIt++){
                                obj_num++;
                                sharedValSet.insert(*objIt);
                            }
                        }else if(callee->getName() == "pthread_mutex_lock"||callee->getName() == "pthread_mutex_unlock"){
                            worklist.push_back(func);
                        }
                    }
                }
            }
        }
    }



//    std::set<Function*> visited;
//
//    while(!worklist.empty()){
//        Function* func=worklist.back();
//        worklist.pop_back();
//
//        if(visited.count(func)){
//            continue;
//        }
//        visited.insert(func);
//
//        for(auto bbIt=func->begin();bbIt!=func->end();bbIt++){
//            BasicBlock* bb=bbIt;
//            for(auto instIt=bb->begin();instIt!=bb->end();instIt++){
//                Instruction* I=instIt;
//                if(CallInst* call= dyn_cast<CallInst>(I)){
//                    if(Function* callee=call->getCalledFunction()){
//                        worklist.push_back(callee);
//                    }else if(Function* callee= dyn_cast<Function>(call->getCalledValue()->stripPointerCasts())){
//                        worklist.push_back(callee);
//                    }
//                }
//            }
//        }
//    }
//
//    for(auto funcIt=visited.begin();funcIt!=visited.end();funcIt++){
//        Function* func=*funcIt;
//        for(auto bbIt=func->begin();bbIt!=func->end();bbIt++) {
//            BasicBlock *bb = bbIt;
//            for (auto instIt = bb->begin(); instIt != bb->end(); instIt++) {
//                Instruction* I=instIt;
//                if(LoadInst* LI= dyn_cast<LoadInst>(I)){
//                    Value* val=LI->getPointerOperand();
//                    std::set<Value*> objSet;
//                    this->getPointstoObjects(objSet, val);
//                    for(auto objIt=objSet.begin();objIt!=objSet.end();objIt++){
//                        obj_num++;
//                        sharedValSet.insert(*objIt);
//                    }
//                }else if(StoreInst* SI= dyn_cast<StoreInst>(I)){
//                    Value* val= SI->getPointerOperand();
//                    std::set<Value*> objSet;
//                    this->getPointstoObjects(objSet, val);
//                    for(auto objIt=objSet.begin();objIt!=objSet.end();objIt++){
//                        obj_num++;
//                        sharedValSet.insert(*objIt);
//                    }
//                }
//            }
//        }
//    }

//    outs()<<"Thread function:\t"<<visited.size()<<"\n";
//    outs()<<"Escaped objects:\t"<<sharedValSet.size()<<"\n";

}

std::set<Value*> DyckAliasAnalysis::collectCanaryCluster(Value *calledValue) {

    std::set<Value*> cluster;

    std::set<Value*> visited;
    std::set<Value*> toHandleVal;
    toHandleVal.insert(calledValue);

    while(!toHandleVal.empty()){

        std::set<Value*> newHandleVal;
        for(auto itV=toHandleVal.begin();itV!=toHandleVal.end();itV++){
            Value* curVal=*itV;
            if(visited.count(curVal)){
                continue;
            }
            visited.insert(curVal);
            const std::set<Value*>* aliasSet=this->getAliasSet(curVal);
            for(auto itAlias=aliasSet->begin();itAlias!=aliasSet->end();itAlias++){
                Value* aliasVal=*itAlias;
                visited.insert(aliasVal);
                //aliasVal->dump();
                for(auto user:aliasVal->users()){
                    if(Instruction* inst= dyn_cast<Instruction>(user)){
                        if(cluster.count(inst)){
                            continue;
                        }
                        cluster.insert(inst);
                        if(StoreInst* SI= dyn_cast<StoreInst>(inst)){
                            newHandleVal.insert(SI->getPointerOperand());
                        }else if(LoadInst* LI= dyn_cast<LoadInst>(inst)){
                            newHandleVal.insert(LI->getPointerOperand());
                        }else if(GetElementPtrInst* GEP= dyn_cast<GetElementPtrInst>(inst)){
                            newHandleVal.insert(GEP->getPointerOperand());
                        }
                    }
                }
                if(Instruction* inst=dyn_cast<Instruction>(aliasVal)){ //may excluding the functions
                    //if(cluster.count(inst)){
                    //    continue;
                    //}
                    cluster.insert(inst);
                }
            }
        }
        toHandleVal.clear();
        toHandleVal=newHandleVal;
    }
    return cluster;
}

void DyckAliasAnalysis::printAliasSetInformation(Module& M) {
    /*if (InterAAEval)*/
    {
        set<DyckVertex*>& allreps = dyck_graph->getVertices();

        outs() << "Printing distribution.log... ";
        outs().flush();
        FILE * log = fopen("distribution.log", "w+");

        vector<unsigned long> aliasSetSizes;
        double totalSize = 0;
        set<DyckVertex*>::iterator it = allreps.begin();
        while (it != allreps.end()) {
            set<void*>* aliasset = (*it)->getEquivalentSet();

            unsigned long size = 0;

            set<void*>::iterator asIt = aliasset->begin();
            while (asIt != aliasset->end()) {
                Value * val = ((Value*) (*asIt));
                if (val->getType()->isPointerTy()) {
                    size++;
                }

                asIt++;
            }

            if (size != 0) {
                totalSize = totalSize + size;
                aliasSetSizes.push_back(size);
                fprintf(log, "%lu\n", size);
            }

            it++;
        }
        errs() << totalSize << "\n";
        double pairNum = (((totalSize - 1) / 2) * totalSize);

        double noAliasNum = 0;
        for (unsigned i = 0; i < aliasSetSizes.size(); i++) {
            double isize = aliasSetSizes[i];
            for (unsigned j = i + 1; j < aliasSetSizes.size(); j++) {
                noAliasNum = noAliasNum + isize * aliasSetSizes[j];
            }
        }
        //errs() << noAliasNum << "\n";

        double percentOfNoAlias = noAliasNum / (double) pairNum * 100;

        fclose(log);
        outs() << "Done!\n";

        outs() << "===== Alias Analysis Evaluator Report =====\n";
        outs() << "   " << pairNum << " Total Alias Queries Performed\n";
        outs() << "   " << noAliasNum << " no alias responses (" << (unsigned long) percentOfNoAlias << "%)\n\n";
    }

    /*if (DotAliasSet) */
    {
        outs() << "Printing alias_rel.dot... ";
        outs().flush();

        FILE * aliasRel = fopen("alias_rel.dot", "w");
        fprintf(aliasRel, "digraph rel{\n");

        set<DyckVertex*> svs;
        Function* PThreadCreate = M.getFunction("pthread_create");
        if (PThreadCreate != NULL) {
            this->getEscapedPointersTo(&svs, PThreadCreate);
        }

        map<DyckVertex*, int> theMap;
        int idx = 0;
        set<DyckVertex*>& reps = dyck_graph->getVertices();
        auto repIt = reps.begin();
        while (repIt != reps.end()) {
            idx++;
            if (svs.count(*repIt)) {
                fprintf(aliasRel, "a%d[label=%d color=red];\n", idx, idx);
            } else {
                fprintf(aliasRel, "a%d[label=%d];\n", idx, idx);
            }
            theMap.insert(pair<DyckVertex*, int>(*repIt, idx));
            repIt++;
        }

        int thread_sharing=0;

        repIt = reps.begin();
        while (repIt != reps.end()) {
            DyckVertex* dv = *repIt;

            map<void*, set<DyckVertex*>>& outVs = dv->getOutVertices();

            auto ovIt = outVs.begin();
            while (ovIt != outVs.end()) {
                EdgeLabel* label = (EdgeLabel*) ovIt->first;
                set<DyckVertex*>* oVs = &ovIt->second;

                set<DyckVertex*>::iterator olIt = oVs->begin();
                while (olIt != oVs->end()) {
                    DyckVertex * rep1 = dv;
                    DyckVertex * rep2 = (*olIt);

                    assert(theMap.count(rep1) && "ERROR in DotAliasSet (1)\n");
                    assert(theMap.count(rep2) && "ERROR in DotAliasSet (2)\n");

                    int idx1 = theMap[rep1];
                    int idx2 = theMap[rep2];

                    if (svs.count(rep1) && svs.count(rep2)) {
                        thread_sharing++;
                        fprintf(aliasRel, "a%d->a%d[label=\"%s\" color=red];\n", idx1, idx2, label->getEdgeLabelDescription().data());
                    } else {
                        fprintf(aliasRel, "a%d->a%d[label=\"%s\"];\n", idx1, idx2, label->getEdgeLabelDescription().data());
                    }

                    olIt++;
                }

                ovIt++;
            }

            theMap.insert(pair<DyckVertex*, int>(*repIt, idx));
            repIt++;
        }

        fprintf(aliasRel, "}\n");
        fclose(aliasRel);
        outs()<<svs.size()<<"...";
        outs() << "Done!\n";

    }

    /*if (OutputAliasSet)*/
    /*
    {
        outs() << "Printing alias_sets.log... ";
        outs().flush();

        std::error_code EC;
        raw_fd_ostream log("alias_sets.log", EC, sys::fs::OpenFlags::F_RW);

        set<DyckVertex*> svs;
        Function* PThreadCreate = M.getFunction("pthread_create");
        if (PThreadCreate != NULL) {
            this->getEscapedPointersTo(&svs, PThreadCreate);
        }

        log << "================= Alias Sets ==================\n";
        log << "===== {.} means pthread escaped alias set =====\n";

        int idx = 0;
        set<DyckVertex*>& reps = dyck_graph->getVertices();
        set<DyckVertex*>::iterator repsIt = reps.begin();
        while (repsIt != reps.end()) {
            idx++;
            DyckVertex* rep = *repsIt;

            bool pthread_escaped = false;
            if (svs.count(rep)) {
                pthread_escaped = true;
            }

            set<void*>* eset = rep->getEquivalentSet();
            set<void*>::iterator eit = eset->begin();
            while (eit != eset->end()) {
                Value* val = (Value*) ((*eit));
                assert(val != NULL && "Error: val is null in an equiv set!");
                if (pthread_escaped) {
                    log << "{" << idx << "}";
                } else {
                    log << "[" << idx << "]";
                }

                if (isa<Function>(val)) {
                    log << ((Function*) val)->getName() << "\n";
                } else {
                    log << *val << "\n";
                }
                eit++;
            }
            repsIt++;
            log << "\n------------------------------\n";
        }

        log.flush();
        log.close();
        outs() << "Done! \n";
    }
     */
}

//ModulePass *createDyckAliasAnalysisPass() {
//    return new DyckAliasAnalysis();
//}

