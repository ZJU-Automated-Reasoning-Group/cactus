#include "CommandLineOptions.h"
#include "RunAnalysis.h"

#include "Context/KLimitContext.h"
#include "PointerAnalysis/Analysis/SemiSparsePointerAnalysis.h"
#include "PointerAnalysis/FrontEnd/SemiSparseProgramBuilder.h"
#include "Util/DataStructure/VectorSet.h"
#include "Util/Iterator/MapValueIterator.h"
#include "Util/IO/PointerAnalysis/Printer.h"

#include <llvm/Support/raw_ostream.h>
#include <sstream>
#include <iomanip>

using namespace llvm;
using namespace tpa;
using namespace util::io;

static void dumpPtsSetForValue(const Value* value, const SemiSparsePointerAnalysis& ptrAnalysis)
{
	assert(value != nullptr);
	if (!value->getType()->isPointerTy())
		return;

	auto ptrs = ptrAnalysis.getPointerManager().getPointersWithValue(value);
	if (ptrs.empty())
	{
		errs() << "val = " << *value << "\n";
		if (!(isa<PHINode>(value) || isa<IntToPtrInst>(value) || isa<CallInst>(value) || isa<InvokeInst>(value)))
			errs() << "Warning: cannot find corresponding ptr for value of type " << value->getValueID() << "\n";
		return;
	}

	for (auto const& ptr: ptrs)
	{
		errs() << *ptr->getContext() << "::";
		dumpValue(errs(), *value);
		errs() << "  -->>  " << ptrAnalysis.getPtsSet(ptr) << "\n";
	}
}

static void dumpPtsSetInBasicBlock(const BasicBlock& bb, const SemiSparsePointerAnalysis& ptrAnalysis)
{
	for (auto const& inst: bb)
		dumpPtsSetForValue(&inst, ptrAnalysis);
}

static void dumpPtsSetInFunction(const Function& f, const SemiSparsePointerAnalysis& ptrAnalysis)
{
	for (auto const& arg: f.args())
		dumpPtsSetForValue(&arg, ptrAnalysis);
	for (auto const& bb: f)
		dumpPtsSetInBasicBlock(bb, ptrAnalysis);
}

static void dumpAll(const Module& module, const SemiSparsePointerAnalysis& ptrAnalysis)
{
	for (auto const& g: module.globals())
		dumpPtsSetForValue(&g, ptrAnalysis);
	for (auto const& f: module)
	{
		if (!f.isDeclaration())
			dumpPtsSetInFunction(f, ptrAnalysis);
	}
}

void runAnalysisOnModule(const Module& module, const CommandLineOptions& opts)
{
	// Display K value setting at the beginning
	unsigned k = opts.getContextSensitivity();
	outs() << "\n============================================\n";
	outs() << "Running analysis with context sensitivity k=" << k << "\n";
	outs() << "============================================\n\n";

	SemiSparseProgramBuilder ssProgBuilder;
	auto ssProg = ssProgBuilder.runOnModule(module);
	auto ptrAnalysis = SemiSparsePointerAnalysis();
	ptrAnalysis.loadExternalPointerTable(opts.getPtrConfigFileName().data());

	// Set the context sensitivity level
	context::KLimitContext::setLimit(k);
	outs() << "Context sensitivity limit set to: " << context::KLimitContext::getLimit() << "\n\n";
	
	ptrAnalysis.runOnProgram(ssProg);

	// Instead of dumping all points-to information, print high-level metrics
	size_t totalPointers = 0;
	size_t totalMemoryObjects = 0;
	size_t totalPointsToEntries = 0;
	size_t maxPtsSetSize = 0;
	
	const auto& ptrManager = ptrAnalysis.getPointerManager();
	auto pointers = ptrManager.getAllPointers();
	totalPointers = pointers.size();
	
	// Set of all objects in points-to sets to count unique memory objects
	std::unordered_set<const MemoryObject*> uniqueObjects;
	
	for (auto ptr : pointers) {
		auto ptsSet = ptrAnalysis.getPtsSet(ptr);
		size_t setSize = ptsSet.size();
		totalPointsToEntries += setSize;
		maxPtsSetSize = std::max(maxPtsSetSize, setSize);
		
		// Add memory objects to unique set
		for (auto obj : ptsSet) {
			uniqueObjects.insert(obj);
		}
	}
	
	totalMemoryObjects = uniqueObjects.size();
	
	outs() << "=== Points-to Analysis Statistics (k=" << k << ") ===\n";
	outs() << "Total Pointers: " << totalPointers << "\n";
	outs() << "Total Memory Objects: " << totalMemoryObjects << "\n";
	outs() << "Total Points-to Entries: " << totalPointsToEntries << "\n";
	outs() << "Max Points-to Set Size: " << maxPtsSetSize << "\n";
	
	if (totalPointers > 0) {
		double avgPtsSetSize = static_cast<double>(totalPointsToEntries) / totalPointers;
		std::stringstream ss;
		ss << std::fixed << std::setprecision(2) << avgPtsSetSize;
		outs() << "Average Points-to Set Size: " << ss.str() << "\n";
	}
	
	outs() << "Context Sensitivity: k=" << k << "\n";
	
	// To dump the full points-to information, the user should use a specific flag
	if (opts.shouldDumpPts()) {
		outs() << "\n=== Detailed Points-to Information ===\n";
		dumpAll(module, ptrAnalysis);
	}
}