//===-------------------- RAOffset.h - Pass definition ----------*- C++ -*-===//
//
//             Offset Based Alias Analysis for The LLVM Compiler
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details
//
//===----------------------------------------------------------------------===//
///
/// \file
/// This file contains the declaration of the RAOffset class. It is an offset
/// representation that uses a numerical Range Analysis 
/// 
///
//===----------------------------------------------------------------------===//

#ifndef __RAOFFSET_H__
#define __RAOFFSET_H__

// project's includes
#include "OBAA/Offset.h"
#include "OBAA/RangeAnalysis.h"
// llvm's includes
#include "llvm/IR/InstrTypes.h"
#include "llvm/Support/raw_ostream.h"
// libc includes
#include <map>
#include <vector>

namespace llvm {

// Forward declarations
class AnalysisUsage;
struct NarrowingOp;
struct WideningOp;
class Value;
class Type;
class OffsetPointer;
class OffsetBasedAliasAnalysis;


/// Representation of types as sequences of primitive values (now bits!)
class Primitives {
  public:
  //Holds a Primitive Layout for a determined Type
  struct PrimitiveLayout {
    Type * type;
    std::vector<int> layout;
    PrimitiveLayout(Type* ty, std::vector<int> lay) {
      type = ty;
      layout = lay;
    }
  };
  struct NumPrimitive {
    Type * type;
    int num;
    NumPrimitive(Type* ty, int n) {
      type = ty;
      num = n;
    }
  };
  std::vector<PrimitiveLayout*> PrimitiveLayouts;
  std::vector<NumPrimitive*> NumPrimitives;
  std::vector<int> getPrimitiveLayout(Type* type);
  int getNumPrimitives(Type* type);
  llvm::Type* getTypeInside(Type* type, int i);
  int getSumBehind(std::vector<int> v, unsigned int i);
};

/// \brief Offset representation that uses range analysis
class RAOffset : public OffsetRepresentation {
  
public:
  RAOffset();
  
  /// \brief Builds \p pointer's offset using \p base 
  RAOffset(const Value* Pointer, const Value* Base);
  
  /// \brief Destructor 
  ~RAOffset() override;
  
  /// \brief Returns a copy of the represented offset
  RAOffset* copy() override;
  
  /// \brief Adds two offsets of the respective representation
  RAOffset* add(OffsetRepresentation* Other) override;
  
  /// \brief Answers true if two offsets are disjoints
  bool disjoint(OffsetRepresentation* Other) override;
  
  /// \brief Narrows the offset of the respective representation
  RAOffset* narrow(CmpInst::Predicate Cmp, OffsetRepresentation* Other) override;
  
  /// \brief Widens the offset of the respective representation, Before and 
  ///   After are given so its possible to calculate direction of growth.
  RAOffset* widen(OffsetRepresentation* Before, OffsetRepresentation* After) override;
  
  /// \brief Prints the offset representation
  void print() override;
  /// \brief Prints the offset to a file
  void print(raw_fd_ostream& fs) override;

  static void initialization(OffsetBasedAliasAnalysis* Analysis);

private:
  static InterProceduralRA<Cousot>* ra;
  Range r;
  static Primitives P;
  /// This function processes the indexes of a GEP operation and returns
  /// the actual bitwise range of its offset;
  void processGEP(const Value* Base, const Use* idx_begin, const Use* idx_end);
};

}

#endif
