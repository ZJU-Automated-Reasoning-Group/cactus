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

	// Reset and set the context sensitivity limit BEFORE building the program
	// to ensure it takes effect during analysis
	context::KLimitContext::setLimit(k);
	outs() << "Context sensitivity limit explicitly set to: " << context::KLimitContext::getLimit() << "\n\n";
	
	// Enable debug output if requested
	bool debugContext = opts.isContextDebugEnabled();
	if (debugContext) {
		outs() << "DEBUG: Context debugging enabled\n";
		outs() << "DEBUG: Initial KLimitContext setting: " << context::KLimitContext::getLimit() << "\n";
	}

	// Build the program representation
	SemiSparseProgramBuilder ssProgBuilder;
	auto ssProg = ssProgBuilder.runOnModule(module);
	
	// Create pointer analysis instance
	auto ptrAnalysis = SemiSparsePointerAnalysis();
	
	// Enable context preservation for global values if k > 0
	// This needs to be done before loading external pointer table
	if (k > 0) {
		outs() << "Enabling context preservation for global values\n";
		// Use the non-const accessor to get the PointerManager
		ptrAnalysis.getMutablePointerManager().setPreserveGlobalValueContexts(true);
		
		if (debugContext) {
			outs() << "DEBUG: Global value context preservation enabled: " 
			       << (ptrAnalysis.getPointerManager().getPreserveGlobalValueContexts() ? "true" : "false") << "\n";
		}
	}
	
	// Load external pointer configuration after setting up the pointer manager
	ptrAnalysis.loadExternalPointerTable(opts.getPtrConfigFileName().data());
	
	if (debugContext) {
		outs() << "DEBUG: Before analysis, KLimitContext setting is: " << context::KLimitContext::getLimit() << "\n";
	}
	
	// Run the analysis with the configured context sensitivity
	ptrAnalysis.runOnProgram(ssProg);
	
	if (debugContext) {
		outs() << "DEBUG: After analysis, KLimitContext setting is: " << context::KLimitContext::getLimit() << "\n";
	}

	// Instead of dumping all points-to information, print high-level metrics
	size_t totalPointers = 0;
	size_t totalMemoryObjects = 0;
	size_t totalPointsToEntries = 0;
	size_t maxPtsSetSize = 0;
	
	const auto& ptrManager = ptrAnalysis.getPointerManager();
	auto pointers = ptrManager.getAllPointers();
	totalPointers = pointers.size();
	
	// Debug pointer contexts if requested
	if (debugContext) {
		outs() << "DEBUG: Examining pointer contexts in the pointer manager...\n";
		std::map<size_t, size_t> contextCounts;
		std::map<const context::Context*, size_t> uniqueContexts;
		
		for (const auto* ptr : pointers) {
			const auto* ctx = ptr->getContext();
			contextCounts[ctx->size()]++;
			uniqueContexts[ctx]++;
		}
		
		outs() << "DEBUG: Found " << uniqueContexts.size() << " unique contexts in total\n";
		for (const auto& ctxPair : contextCounts) {
			size_t depth = ctxPair.first;
			size_t count = ctxPair.second;
			outs() << "DEBUG: Found " << count << " pointers with context depth " << depth << "\n";
		}
		
		// Show a sample of the unique contexts for verification
		outs() << "DEBUG: Sample of unique contexts (max 5):\n";
		size_t shownCount = 0;
		for (const auto& ctxPair : uniqueContexts) {
			if (shownCount >= 5) break;
			const auto* ctx = ctxPair.first;
			size_t pointerCount = ctxPair.second;
			outs() << "  Context #" << shownCount << ": depth=" << ctx->size() 
			       << ", used by " << pointerCount << " pointers\n";
			shownCount++;
		}
	}
	
	// Set of all objects in points-to sets to count unique memory objects
	std::unordered_set<const MemoryObject*> uniqueObjects;
	
	// Collect and analyze points-to information
	// Keep track of context statistics for points-to sets
	std::map<size_t, size_t> ptsContextDepths; // Context depth -> number of pointers
	
	for (auto ptr : pointers) {
		auto ptsSet = ptrAnalysis.getPtsSet(ptr);
		size_t setSize = ptsSet.size();
		totalPointsToEntries += setSize;
		maxPtsSetSize = std::max(maxPtsSetSize, setSize);
		
		// Track context depths
		const auto* ctx = ptr->getContext();
		ptsContextDepths[ctx->size()]++;
		
		// Add memory objects to unique set
		for (auto obj : ptsSet) {
			uniqueObjects.insert(obj);
		}
	}
	
	totalMemoryObjects = uniqueObjects.size();
	
	// Display statistics
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
	outs() << "Final KLimitContext setting: " << context::KLimitContext::getLimit() << "\n";
	
	// Print context distribution from points-to set information
	outs() << "Context depth distribution (from points-to analysis):\n";
	for (const auto& ctxPair : ptsContextDepths) {
		size_t depth = ctxPair.first;
		size_t count = ctxPair.second;
		outs() << "  Depth " << depth << ": " << count << " pointers\n";
	}
	
	// Print context statistics if debug is enabled
	if (debugContext) {
		outs() << "\n=== Context Sensitivity Debug Information ===\n";
		outs() << "Final k value: " << context::KLimitContext::getLimit() << "\n";
		
		// Track contexts of different types
		std::map<const context::Context*, size_t> uniqueContexts;
		std::map<size_t, size_t> contextDepths;
		
		// Count pointers by context depth
		for (auto ptr : pointers) {
			const context::Context* ctx = ptr->getContext();
			uniqueContexts[ctx]++;
			contextDepths[ctx->size()]++;
		}
		
		outs() << "Number of unique contexts: " << uniqueContexts.size() << "\n";
		outs() << "Context depth distribution (from pointer manager):\n";
		for (auto& pair : contextDepths) {
			outs() << "  Depth " << pair.first << ": " << pair.second << " pointers\n";
		}
		
		// Count unique contexts by depth
		std::map<size_t, size_t> uniqueContextsByDepth;
		for (const auto& pair : uniqueContexts) {
			const context::Context* ctx = pair.first;
			uniqueContextsByDepth[ctx->size()]++;
		}
		
		outs() << "Unique contexts by depth:\n";
		for (const auto& pair : uniqueContextsByDepth) {
			outs() << "  Depth " << pair.first << ": " << pair.second << " unique contexts\n";
		}
		
		// Debug output - show a sample of pointers with deeper contexts if they exist
		if (contextDepths.size() > 1) {
			outs() << "\nSample pointers with context depth > 0:\n";
			size_t shown = 0;
			for (auto ptr : pointers) {
				if (ptr->getContext()->size() > 0 && shown < 5) {
					outs() << "  " << *ptr->getContext() << "::" 
					       << *ptr->getValue() << "\n";
					shown++;
				}
			}
		}
		
		// Validate context preservation
		if (debugContext) {
			outs() << "\n=== Context Preservation Validation ===\n";
			
			// Check if contexts of depth > 0, which should exist in a context-sensitive analysis
			bool hasNonGlobalContexts = false;
			size_t numNonGlobalContexts = 0;
			
			for (const auto& pair : uniqueContextsByDepth) {
				if (pair.first > 0) {
					hasNonGlobalContexts = true;
					numNonGlobalContexts += pair.second;
				}
			}
			
			if (hasNonGlobalContexts) {
				outs() << "VALID: Found " << numNonGlobalContexts << " contexts with depth > 0\n";
			} else {
				outs() << "WARNING: All contexts are global contexts (depth=0). "
				       << "This suggests context sensitivity is not working correctly.\n"
					   << "Check that KLimitContext is being properly used during analysis.\n";
			}
			
			// Check for global variables with preserved contexts
			outs() << "\n--- Global Value Context Preservation Check ---\n";
			bool foundGlobalWithNonGlobalContext = false;
			size_t globalVarsWithContextCount = 0;
			
			// Track global variables with their contexts
			std::map<const llvm::GlobalValue*, std::set<const context::Context*>> globalContexts;
			
			for (const auto* ptr : pointers) {
				const auto* val = ptr->getValue();
				const auto* ctx = ptr->getContext();
				
				if (llvm::isa<llvm::GlobalValue>(val) && ctx->size() > 0) {
					foundGlobalWithNonGlobalContext = true;
					globalVarsWithContextCount++;
					globalContexts[llvm::cast<llvm::GlobalValue>(val)].insert(ctx);
				}
			}
			
			if (foundGlobalWithNonGlobalContext) {
				outs() << "VALID: Found " << globalVarsWithContextCount 
				       << " global variable pointers with non-global contexts\n";
				       
				// Show some examples (max 3)
				outs() << "Examples:\n";
				size_t shown = 0;
				for (const auto& pair : globalContexts) {
					if (shown >= 3) break;
					const auto* gv = pair.first;
					const auto& contexts = pair.second;
					
					outs() << "  Global: " << gv->getName() << ", contexts: " << contexts.size() << "\n";
					shown++;
				}
			} else {
				outs() << "WARNING: No global variables found with non-global contexts.\n"
				       << "         This may indicate that context preservation for globals is not working.\n";
			}
			
			// Print a more detailed report
			outs() << "\nDetailed context depth report:\n";
			for (const auto& pair : uniqueContextsByDepth) {
				float percentage = 0.0;
				if (uniqueContexts.size() > 0) {
					percentage = (static_cast<float>(pair.second) / uniqueContexts.size()) * 100.0;
				}
				
				std::stringstream ss;
				ss << std::fixed << std::setprecision(2) << percentage;
				
				outs() << "  Depth " << pair.first << ": " << pair.second 
				       << " contexts (" << ss.str() << "% of total)\n";
			}
		}
	}
	
	// To dump the full points-to information, the user should use a specific flag
	if (opts.shouldDumpPts()) {
		outs() << "\n=== Detailed Points-to Information ===\n";
		dumpAll(module, ptrAnalysis);
	}
}