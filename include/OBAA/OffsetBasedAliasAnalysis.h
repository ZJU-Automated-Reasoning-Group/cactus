//===--- OffsetBasedAliasAnalysis.h - Pass definition ----------*- C++ -*-===//
//
//             Offset Based Alias Analysis for The LLVM Compiler
//
//===----------------------------------------------------------------------===//
///
/// \file
/// \brief This file contains the declaration of the Offset Based Alias Analy-
/// sis pass. TODO: Better description
///
//===----------------------------------------------------------------------===//
#ifndef __OFFSET_BASED_ALIAS_ANALYSIS_H__
#define __OFFSET_BASED_ALIAS_ANALYSIS_H__

// LLVM's includes
#include "llvm/Pass.h"
#include "llvm/Analysis/AliasAnalysis.h"
// libc's includes
#include <map>
#include <set>
#include <deque>

namespace llvm
{

/// Forward declarations
class OffsetPointer;

class OffsetBasedAliasAnalysis : public ModulePass, public AliasAnalysis {
public:
  
  /// LLVM framework methods and atributes
  static char ID; // Class identification, replacement for typeinfo
  OffsetBasedAliasAnalysis() : ModulePass(ID){}
  virtual void getAnalysisUsage(AnalysisUsage &AU) const override;
  bool runOnModule(Module &M) override;
  
  /// Alias Analysis framework methods
  AliasResult alias(const llvm::AliasAnalysis::Location &LocA,
    const llvm::AliasAnalysis::Location &LocB) override;
  bool pointsToConstantMemory(const llvm::AliasAnalysis::Location &Loc, bool OrLocal) override;
  /// getAdjustedAnalysisPointer - This method is used when a pass implements
  /// an analysis interface through multiple inheritance.  If needed, it
  /// should override this to adjust the this pointer as needed for the
  /// specified pass info.
  virtual void *getAdjustedAnalysisPointer(AnalysisID PI) override {
    if (PI == &AliasAnalysis::ID)
      return (AliasAnalysis*)this;
    return this;
  }
  
  /// \brief Function that returns the offset pointer corresponding
  ///  to the value given
  OffsetPointer* getOffsetPointer(const Value*);
  
private:
  /// \brief map that contains all the pointers represented
  std::map<const Value*, OffsetPointer* > offset_pointers;
  std::set<const Value*> all_pointers;
  std::set<const StoreInst*> relevant_stores;
  /// \brief map that stores whether a function returns a local alloc or not
  std::map<const Function*, bool> allocFunctions;
  /// \brief Counts how many dot graphs were printed
  unsigned int dotNum;
  /// \brief Gather all pointers from the module
  void gatherPointers(Module &M);
  /// \brief Builds the dependence graph using an intra procedural frame
  void buildIntraProceduralDepGraph();
  /// \brief Obtains narrowing information from the module
  void getNarrowingInfo();
  /// \brief DFS based on color 
  void DFS_visit_t(OffsetPointer* u, std::deque<OffsetPointer*>* dqp);
  /// \brief DFS visit for calculating scc
  void DFS_visit_scc(OffsetPointer* u, int scc, int &n);
  /// \brief transpost DFS based on color and scc
  void DFS_visit_t_scc(OffsetPointer* u, std::deque<OffsetPointer*>* dqp);
  /// \brief Finds the strongly connected components from the graph
  std::map<int,std::pair<OffsetPointer*, int> > findSCCs();
  /// \brief Resolves the strongly connected components from the graph
  void resolveSCCs(std::map<int,std::pair<OffsetPointer*, int> > sccs);
  /// \brief Resolves the whole graph
  void resolveWholeGraph();
  /// \brief Applies the windening operators present in the graph
  void applyWidening();
  /// \brief Applies the narrowing operators present in the graph
  void applyNarrowing();
  /// \brief Analyzes the function F to verify if it returns a local alloc
  void analyzeFunction(const Function* F);
  /// \brief Updates the call insts to allocs if the called function returns
  ///  a local Alloc
  void updateCalls();
  /// \brief Adds addresses to arguments and calls to make the dependence graph
  ///  interprocedural
  void addInterProceduralEdges(); 
  /// \brief Function that prints the dependence graph in DOT format
  void printDOT(Module &M, std::string Stage);
};

}

#endif
