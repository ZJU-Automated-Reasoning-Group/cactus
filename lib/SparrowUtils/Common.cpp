#include <iomanip>
#include <iostream>
#include <fstream>
#include <cxxabi.h>
#include <cmath>
#include <stack>
#include <set>
#include <iostream>
#include <fstream>
#include <string>

#include <llvm/IR/Module.h>
#include <llvm/IR/DebugLoc.h>
#include <llvm/IR/DebugInfo.h>
#include <llvm/IR/InstIterator.h>
#include <llvm/IR/IntrinsicInst.h>
#include <llvm/Support/raw_os_ostream.h>
#include <llvm/Support/Options.h>

#include "SparrowUtils/Common.h"


static cl::opt<bool> lifter_dbg_mode("sparrow-lifter-dbg-info",
        cl::desc("parse dbg info in lifter manner"),
        cl::init(false), cl::NotHidden);


void Common::printICStatistics(std::string passName,std::map<CallInst*,std::set<llvm::Function*>> iCallResult) {

    if(iCallResult.empty()){
        return;
    }

    int total_callees=0;

    int number_indirect_calls=0;
    int number_of_0_points_to=0;
    int number_of_1_points_to=0;
    int number_of_2_points_to=0;
    int number_of_3_points_to=0;
    int number_of_4_points_to=0;
    int number_of_5_to_10_points_to=0;
    int number_of_10_to_20_points_to=0;
    int number_of_above_20_points_to=0;

    int max=0;

    //number_indirect_calls=iCallResult.size();

    for(auto iter=iCallResult.begin();iter!=iCallResult.end();iter++){
        CallInst* icall=iter->first;
        std::set<Function*> callees=iter->second;

        if(!icall||!isIndirectCallSite(icall)){ // FIXME:: icall is null here?
            continue;
        }

        number_indirect_calls++;

        total_callees+=callees.size();

        if(max<callees.size()){
            max=callees.size();
        }

        switch (callees.size()) {
            case 0:
                number_of_0_points_to++;
                break;
            case 1:
                number_of_1_points_to++;
                break;
            case 2:
                number_of_2_points_to++;
                break;
            case 3:
                number_of_3_points_to++;
                break;
            case 4:
                number_of_4_points_to++;
                break;
            case 5:
            case 6:
            case 7:
            case 8:
            case 9:
            case 10:
                number_of_5_to_10_points_to++;
                break;
            case 11:
            case 12:
            case 13:
            case 14:
            case 15:
            case 16:
            case 17:
            case 18:
            case 20:
                number_of_10_to_20_points_to++;
                break;
            default:
                number_of_above_20_points_to++;
        }
    }


    cout<<passName<<">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>"<<endl;
    cout<<passName<<" Total indirect-call callees: "<<total_callees<<endl;
    cout<<passName<<" The average callee size: "<<fixed<<setprecision(2)<<(double)total_callees/(double)number_indirect_calls<<endl;
    cout<<passName<<" The largest callee size: "<<max<<endl;
    cout<<passName<<" Total indirect calls with 1 targets: "<<number_of_1_points_to<<endl;
    cout<<passName<<" 2~9 indirect-call targets: "<<(number_of_4_points_to+number_of_3_points_to+number_of_2_points_to+number_of_5_to_10_points_to)<<endl;
    cout<<passName<<" 10~20 indirect-call targets: "<<number_of_10_to_20_points_to<<endl;
    cout<<passName<<" >20 indirect-call targets: "<<number_of_above_20_points_to<<endl;
    cout<<passName<<"<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<"<<endl;
}

void Common::printBasicICStatistics(std::map<CallInst *, std::set<llvm::Function *>> iCallResult) {

    if(iCallResult.empty()){
        return;
    }

    int number_indirect_calls=0;
    int number_of_0_points_to=0;
    int number_of_1_points_to=0;
    int total_callees=0;
    int max=0;

    //number_indirect_calls=iCallResult.size();

    for(auto iter=iCallResult.begin();iter!=iCallResult.end();iter++){
        CallInst* icall=iter->first;
        std::set<Function*> callees=iter->second;

        if(!isIndirectCallSite(icall)){
            continue;
        }

        number_indirect_calls++;

        total_callees+=callees.size();

        if(max<callees.size()){
            max=callees.size();
        }

        switch (callees.size()) {
            case 0:
                number_of_0_points_to++;
                break;
            case 1:
                number_of_1_points_to++;
                break;
        }
    }


    cout<<"Average callees per indirect call:\t"<<fixed<<setprecision(2)<<(float)total_callees/(float)number_indirect_calls<<endl;
    outs()<<"Largest callees at indirect calls:\t"<<max<<"\n";
    outs()<<"Uniquely-resolved indirect callsites:\t"<<number_of_1_points_to<<"\n";
    //outs()<<"Empty indirect callsites:\t"<<number_of_0_points_to<<"\n";
}

float Common::getAverageCalleeSize(std::map<CallInst *, std::set<llvm::Function *>> iCallResult) {

    if(iCallResult.empty()){
        return 0;
    }

    int number_indirect_calls=0;
    int total_callees=0;

    for(auto iter=iCallResult.begin();iter!=iCallResult.end();iter++){
        CallInst* icall=iter->first;
        std::set<Function*> callees=iter->second;

        if(!isIndirectCallSite(icall)){
            continue;
        }

        number_indirect_calls++;

        total_callees+=callees.size();

    }
    return  (float)total_callees/(float)number_indirect_calls;
}

void Common::dumpICDetailedInfo(std::map<CallInst*,std::set<llvm::Function*>> iCallResult){



    string  output="";
    //outs()<<"<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<\n";
    //outs()<<"Detailed results of indirect calls:\n";

    std::set<CallInst*> visited;
    unsigned size_counter=0;

    while(visited.size()!=iCallResult.size()){
        for(auto iter=iCallResult.begin();iter!=iCallResult.end();iter++){
            CallInst* icall=iter->first;
            std::set<Function*> callees=iter->second;

            if(callees.size()!=size_counter){
                continue;
            }

            visited.insert(icall);
            const llvm::DebugLoc &debugInfo=icall->getDebugLoc();
            int line = debugInfo.getLine();
            MDNode* N=debugInfo.getScope();
            DIGlobalVariable Loc(N);
            StringRef file_name = Loc.getFilename();
            StringRef directory = Loc.getDirectory();

            Function* caller=icall->getParent()->getParent();

            output+=file_name.str()+"\n";
            output+=icall->getParent()->getParent()->getName().str()+"\n";
            output+=to_string(line)+"\n";
            //outs()<<"Github location:\t"<<file_name<<"#L"<<line<<"\n";
            output+=to_string(callees.size())+"\n";
            for(auto iter2=callees.begin();iter2!=callees.end();iter2++){
                Function* callee=*iter2;
                output+=callee->getName().str()+"\n";
            }
        }
        size_counter++;
    }

    ofstream MyFile("indirect-call-targets.txt");
    if(MyFile){
        MyFile<<output;
        MyFile.close();
    }
}


void Common::readICDetailedInfo(string filePath){

    if (filePath.empty()) {
        filePath = "indirect-call-targets.txt"; // read from file.txt in current directory
    }

    std::ifstream file(filePath);
    if (file.is_open()) {
        bool done = false;
        while (!done) {

            std::string file_path;
            std::getline(file, file_path);

            std::string caller;
            std::getline(file, caller);

            std::string icall_line_str;
            std::getline(file, icall_line_str);
            std::string callee_size_str;
            std::getline(file, callee_size_str);
            int icall_line;
            int callee_size;

            try {
                icall_line = std::stoi(icall_line_str);
                callee_size= std::stoi(callee_size_str);
            } catch (const std::invalid_argument& e) {
                std::cout << "Invalid argument: " << e.what() << std::endl;
                return;
            } catch (const std::out_of_range& e) {
                std::cout << "Out of range: " << e.what() << std::endl;
                return;
            }


            std::vector<string> callees_names;
            for (int i = 0; i < callee_size; i++) {
                std::string line;
                std::getline(file, line);
                callees_names.push_back(line);
            }

            // Check if we have reached the end of the file
            if (file.peek() == EOF) {
                done = true;
            }
        }

    } else {
        std::cerr << "Unable to open file: " << filePath << std::endl;
    }

}


void Common::printInstLoc(Instruction * inst) {

    const llvm::DebugLoc &debugInfo=inst->getDebugLoc();
    int line = debugInfo.getLine();
    MDNode* N=debugInfo.getScope();
    DIGlobalVariable Loc(N);
    StringRef file_name = Loc.getFilename();
    StringRef directory = Loc.getDirectory();
    outs()<<"Instruction:"<<line<<" Function: "<<inst->getParent()->getParent()->getName()<<" File:"<<file_name<<"\n";

}

bool  Common::isFunctionPointerType(Type *type){
    // Check the type here
    if(PointerType *pointerType=dyn_cast<PointerType>(type)){
        return isFunctionPointerType(pointerType->getElementType());
    }
        //Exit Condition
    else if(type->isFunctionTy()){
        return  true;
    }
    return false;
}

bool Common::isArrayFPType(Type *type) {

    if (PointerType *pointerType = dyn_cast<PointerType>(type)) {
        Type* elementType = pointerType->getElementType();
        if (elementType->isArrayTy()) {
            Type* subType = elementType->getArrayElementType();
            if(isFunctionPointerType(subType)){
                //***
                return true;
            }
        }
    }

    return false;
}

bool Common::isIndirectCallSite(CallInst * CI) {

    if(!CI){
        return false;
    }

    if(!CI->getCalledFunction()){
        CallSite cs(CI);
        if(CI->isInlineAsm()|| isVirtualCallSite(cs)){
            return false;
        }
        if(Function* callee=dyn_cast<Function>(CI->getCalledValue()->stripPointerCasts())){
            return false;
        }
        return true;
    }

    return false;
}

bool Common::isStripFunctionPointerCasts(CallInst * CI) {

    if(!CI->getCalledFunction()){
        CallSite cs(CI);
        if(CI->isInlineAsm()|| isVirtualCallSite(cs)){
            return false;
        }
        if(Function* callee=dyn_cast<Function>(CI->getCalledValue()->stripPointerCasts())){
            return true;
        }
    }
    return false;
}

/*
 * a virtual callsite follows the following instruction sequence pattern:
 * %vtable = load ...
 * %vfn = getelementptr %vtable, idx
 * %x = load %vfn
 * call %x (...)
 */
bool Common::isVirtualCallSite(CallSite cs) {

    if(!cs){
        return false;
    }

    if (cs.getCalledFunction() != NULL)
        return false;

    const Value *vfunc = cs.getCalledValue();
    if (const LoadInst *vfuncloadinst = dyn_cast<LoadInst>(vfunc)) {
        const Value *vfuncptr = vfuncloadinst->getPointerOperand();
        if (const GetElementPtrInst *vfuncptrgepinst =
                dyn_cast<GetElementPtrInst>(vfuncptr)) {
            if (vfuncptrgepinst->getNumIndices() != 1)
                return false;
            const Value *vtbl = vfuncptrgepinst->getPointerOperand();
            if (isa<LoadInst>(vtbl)) {
                return true;
            }
        }
    }
    return false;
}

bool Common::isVirtualCallSite(CallInst *CI) {
    CallSite cs(CI);

    if (cs.getCalledFunction() != NULL)
        return false;

    const Value *vfunc = cs.getCalledValue();
    if (const LoadInst *vfuncloadinst = dyn_cast<LoadInst>(vfunc)) {
        const Value *vfuncptr = vfuncloadinst->getPointerOperand();
        if (const GetElementPtrInst *vfuncptrgepinst =
                dyn_cast<GetElementPtrInst>(vfuncptr)) {
            if (vfuncptrgepinst->getNumIndices() != 1)
                return false;
            const Value *vtbl = vfuncptrgepinst->getPointerOperand();
            if (isa<LoadInst>(vtbl)) {
                return true;
            }
        }
    }
    return false;

}

// Function to find longest common substring.
string Common::LCSubStr(string X, string Y){
    // Find length of both the strings.
    int m = X.length();
    int n = Y.length();

    // Variable to store length of longest
    // common substring.
    int result = 0;

    // Variable to store ending point of
    // longest common substring in X.
    int end;

    // Matrix to store result of two
    // consecutive rows at a time.
    int len[2][n + 1];

    // Variable to represent which row of
    // matrix is current row.
    int currRow = 0;

    // For a particular value of i and j,
    // len[currRow][j] stores length of longest
    // common substring in string X[0..i] and Y[0..j].
    for (int i = 0; i <= m; i++) {
        for (int j = 0; j <= n; j++) {
            if (i == 0 || j == 0) {
                len[currRow][j] = 0;
            }
            else if (X[i - 1] == Y[j - 1]) {
                len[currRow][j] = len[1 - currRow][j - 1] + 1;
                if (len[currRow][j] > result) {
                    result = len[currRow][j];
                    end = i - 1;
                }
            }
            else {
                len[currRow][j] = 0;
            }
        }

        // Make current row as previous row and
        // previous row as new current row.
        currRow = 1 - currRow;
    }

    // If there is no common substring, print -1.
    if (result == 0) {
        return "-1";
    }

    // Longest common substring is from index
    // end - result + 1 to index end in X.
    return X.substr(end - result + 1, result);
}

set<string> Common::split(const string &str, const string &pattern)
{
    set<string> res;
    if(str == "")
        return res;

    string strs = str + pattern;
    size_t pos = strs.find(pattern);

    while(pos != strs.npos)
    {
        string temp = strs.substr(0, pos);
        res.insert(temp);

        strs = strs.substr(pos+1, strs.size());
        pos = strs.find(pattern);
    }

    return res;
}

set<string> Common::splitByCapital(const string &str) { //get RealName // handle the capital

    set<string> res;

    if(str == ""){
        return res;
    }

    std::set<char> capital_set;
    capital_set.insert('Q');
    capital_set.insert('W');
    capital_set.insert('E');
    capital_set.insert('R');
    capital_set.insert('T');
    capital_set.insert('Y');
    capital_set.insert('U');
    capital_set.insert('I');
    capital_set.insert('O');
    capital_set.insert('P');
    capital_set.insert('A');
    capital_set.insert('S');
    capital_set.insert('D');
    capital_set.insert('F');
    capital_set.insert('G');
    capital_set.insert('H');
    capital_set.insert('J');
    capital_set.insert('K');
    capital_set.insert('L');
    capital_set.insert('Z');
    capital_set.insert('X');
    capital_set.insert('C');
    capital_set.insert('V');
    capital_set.insert('B');
    capital_set.insert('N');
    capital_set.insert('M');

    string cur="";
    for(int i=0;i<str.length();i++){
        char ch=str[i];
        if(capital_set.count(ch)){
            //outs()<<cur<<"\n";
            res.insert(cur);
            cur="";
            cur+=ch;
        }else{
            cur+=ch;
        }
        if(i==str.length()-1){
            //outs()<<cur<<"\n";
            res.insert(cur);
        }
    }
    return res;
}

Type* Common::getPureType(Type * type) {

    while(type->isPointerTy()){
        type=type->getPointerElementType();
    }

    return type;
}

std::string Common::demangle(const char* name){
    int status = -1;

    std::unique_ptr<char, void(*)(void*)> res { abi::__cxa_demangle(name, NULL, NULL, &status), std::free };
    return (status == 0) ? res.get() : std::string(name);
}

int Common::damerau_levenshtein(std::string* src,std::string* dest)
{
    int len_src = src->length();
    int len_dest = dest->length();
    int dem_lev_dist = 0;

    if (len_src == 0 && len_dest == 0)
    {
        dem_lev_dist = 0;
    }
    else if (len_src == 0)
    {
        dem_lev_dist = len_dest;
    }
    else if (len_dest ==0)
    {
        dem_lev_dist = len_src;
    }
    else
    {
        int LevMat[len_src+1][len_dest+1];

        for (int i=0; i<= len_src; i++)
        {
            for (int j=0; j<= len_dest; j++)
            {
                if (i==0 || j==0)
                {
                    LevMat[i][j] = std::max(i,j);
                }

                else {
                    int fromLeftCell = LevMat[i-1][j] + 1;
                    int fromUpperCell = LevMat[i][j-1] + 1;
                    int fromDiagCell = LevMat[i-1][j-1];
                    if (src->at(i-1)!=dest->at(j-1))
                    {
                        fromDiagCell += 1;
                    }

                    if (i > 1 && j > 1 && (src->at(i-2) == dest->at(j-1)) && (src->at(i-1)==dest->at(j-2)))
                    {
                        int fromTransposition = LevMat[i-2][j-2] + 1;

                        LevMat[i][j] = std::min(fromTransposition, std::min(fromLeftCell, std::min(fromUpperCell, fromDiagCell)));

                    }

                    else
                    {
                        LevMat[i][j] = std::min(fromLeftCell, std::min(fromUpperCell, fromDiagCell));
                    }
                }
            }
        }

        dem_lev_dist = LevMat[len_src][len_dest];
    }
    return dem_lev_dist;
}

std::string Common::lcs(std::string* src, std::string* dest){
    int len_src = src->length();
    int len_dest = dest->length();
    std::string lcs = "";
    int lcs_len = 0;
    int src_end = 0;

    if (len_src > 0 and len_dest > 0)
    {
        int LCSMat[len_src+1][len_dest+1];

        for (int i=0; i<= len_src; i++)
        {
            for (int j=0; j<= len_dest; j++)
            {
                if (i==0 || j==0)
                {
                    LCSMat[i][j] = 0;
                }

                else if ( src->at(i-1) == dest->at(j-1))
                {
                    LCSMat[i][j] = LCSMat[i-1][j-1] + 1;
                    if (lcs_len <= LCSMat[i][j])
                    {
                        lcs_len = LCSMat[i][j];
                        src_end = i;
                    }
                }

                else
                {
                    LCSMat[i][j] = 0;
                }
            }
        }

        if (lcs_len > 0)
        {
            lcs = src->substr(src_end - lcs_len, lcs_len);
        }
    }

    return lcs;
}

int Common::lcs_len(std::string* src, std::string* dest){
    int len = lcs(src, dest).length();
    return len;
}

float Common::lcs_score(std::string* src, std::string* dest){
    int len_cs = lcs(src, dest).length();
    int len_src = src->length();
    int len_dest = dest->length();

    float score = 0;

    if (len_src > 0 || len_dest > 0)
    {
        score = len_cs/(std::sqrt(len_src) * std::sqrt(len_dest));
    }

    return score;
}

int Common::levenshtein(std::string* src, std::string* dest)
{
    int len_src = src->length();
    int len_dest = dest->length();
    int lev_dist = 0;

    if (len_src == 0 && len_dest == 0)
    {
        lev_dist = 0;
    }
    else if (len_src == 0)
    {
        lev_dist = len_dest;
    }
    else if (len_dest ==0)
    {
        lev_dist = len_src;
    }
    else
    {
        int LevMat[len_src+1][len_dest+1];

        for (int i=0; i<= len_src; i++)
        {
            for (int j=0; j<= len_dest; j++)
            {
                if (i==0 || j==0)
                {
                    LevMat[i][j] = std::max(i,j);
                }

                else {
                    int fromLeftCell = LevMat[i-1][j] + 1;
                    int fromUpperCell = LevMat[i][j-1] + 1;
                    int fromDiagCell = LevMat[i-1][j-1];
                    if (src->at(i-1)!=dest->at(j-1))
                    {
                        fromDiagCell += 1;
                    }
                    LevMat[i][j] = std::min(fromLeftCell, std::min(fromUpperCell, fromDiagCell));
                }
            }
        }

        lev_dist = LevMat[len_src][len_dest];
    }
    return lev_dist;
}

const Function* Common::findEnclosingFunc(const Value* V) {
    if (const Argument* Arg = dyn_cast<Argument>(V)) {
        return Arg->getParent();
    }
    if (const Instruction* I = dyn_cast<Instruction>(V)) {
        return I->getParent()->getParent();
    }
    return NULL;
}

const MDNode* Common::findVar(const Value* V, const Function* F) {
    for (auto Iter = inst_begin(F), End = inst_end(F); Iter != End; ++Iter) {
        const Instruction* I = &*Iter;
        if (const DbgDeclareInst* DbgDeclare = dyn_cast<DbgDeclareInst>(I)) {
            if (DbgDeclare->getAddress() == V){
                return DbgDeclare->getVariable();
            }
        } else if (const DbgValueInst* DbgValue = dyn_cast<DbgValueInst>(I)) {
            if (DbgValue->getValue() == V){
                return DbgValue->getVariable();
            }
        }
    }
    return NULL;
}

StringRef Common::getOriginalName(const Value* V) {
    // TODO handle globals as well

    const Function* F = findEnclosingFunc(V);
    if (!F) return V->getName();

    const MDNode* Var = findVar(V, F);
    if (!Var) return "tmp";

    return DIVariable(Var).getName();
}

