#include <llvm/IR/LegacyPassManager.h>
#include <llvm/Support/CommandLine.h>
#include <llvm/Transforms/Scalar.h>
#include <llvm/InitializePasses.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IRReader/IRReader.h>
#include <llvm/Support/Debug.h>
#include <llvm/Support/SourceMgr.h>
#include <llvm/Support/TargetRegistry.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/Transforms/IPO/PassManagerBuilder.h>

#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/FileSystem.h"

#include "llvm/Bitcode/ReaderWriter.h"

#include "FPAnalysis/FunPtrAnalysis.h"
#include "SparrowUtils/Profiler.h"



using namespace llvm;
using namespace std;
using namespace Sparrow;


static llvm::cl::opt<std::string> InputFilename(cl::Positional,
                                                llvm::cl::desc("<input bitcode>"), llvm::cl::init("-"));

static cl::opt<bool> Dump_LLVM_BC("dump-bc",
                                   cl::desc("Dump the transformed BC without indirect calls"), cl::init(false), cl::NotHidden);

static cl::opt<bool> Dump_CG_report("dump-report",
                                  cl::desc("Dump the indirect-call information"), cl::init(false), cl::NotHidden);


void initializeLLVMPasses() {
    // Initialize passes
    PassRegistry &Registry = *PassRegistry::getPassRegistry();
    initializeCore(Registry);
    initializeScalarOpts(Registry);
    initializeIPO(Registry);
    initializeAnalysis(Registry);
    initializeTransformUtils(Registry);
    initializeInstCombine(Registry);
    initializeTarget(Registry);
    initializeIndVarSimplifyPass(Registry);
    initializeSimpleInlinerPass(Registry);
    initializeLowerInvokePass(Registry);
    initializeCFGSimplifyPassPass(Registry);
    initializeRegToMemPass(Registry);
}

void addPass(legacy::PassManagerBase &pm, Pass *P, const std::string& phaseName = std::string()) {

    std::string pn = phaseName.empty() ? P->getPassName() : phaseName;

    pm.add(P);
}

int main(int argc, char ** argv) {


    LLVMContext& Context=getGlobalContext();

    //std::string OutputFilename;

    SMDiagnostic Err;

    cl::ParseCommandLineOptions(argc, argv, "Call Graph Construction...\n");

    std::unique_ptr<Module> M = parseIRFile(InputFilename, Err, Context);

    if (!M) {
        Err.print(argv[0], errs());
        return -1;
    }

    std::error_code ErrorInfo;

    StringRef str(InputFilename);
    InputFilename = str.rsplit('.').first;


    if (ErrorInfo) {
        errs() << ErrorInfo.message() << '\n';
        return 1;
    }

    initializeLLVMPasses();

    legacy::PassManager Passes;

    FPAnalysis* fp_analysis=new FPAnalysis();

    addPass(Passes, fp_analysis, "Function Pointer Analysis");

    Passes.run(*M.get());

    if(Dump_LLVM_BC){
        fp_analysis->convertICallToCall();
        std::string OutputFilename = ""+InputFilename + "_sparrow.bc";
        std::error_code EC;
        llvm::raw_fd_ostream OS(OutputFilename, EC, llvm::sys::fs::OpenFlags::F_None);
        if (!EC) {
            llvm::WriteBitcodeToFile(M.get(), OS);
            OS.flush();
        }
    }

    if(Dump_CG_report){
        fp_analysis->dumpICallResult();
    }


    return 0;

}

