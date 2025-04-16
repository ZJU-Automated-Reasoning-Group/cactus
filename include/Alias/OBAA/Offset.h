//===---------------------- Offset.h - Pass definition ----------*- C++ -*-===//
//
//             Offset Based Alias Analysis for The LLVM Compiler
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details
//
//===----------------------------------------------------------------------===//
///
/// \file
/// This file contains the declaration of the Offset class and
/// the offset representations base. An offset is part of a pointer 
/// representation and requires simple operations: Adding two offsets, 
/// checking if two offsets are disjoint, narrow an offset and widen it. 
/// 
///
//===----------------------------------------------------------------------===//

#ifndef __OFFSET_H__
#define __OFFSET_H__

// llvm's includes
#include "llvm/IR/InstrTypes.h"
#include "llvm/Support/raw_ostream.h"
// libc includes
#include <map>

namespace llvm {

// Forward declarations
class AnalysisUsage;
struct NarrowingOp;
struct WideningOp;
class Value;
class OffsetPointer;
class OffsetBasedAliasAnalysis;

/// \brief Abstract class for implementing offset representations.
/// Use it for testing new integer comparison analyses.
class OffsetRepresentation {
  
public:
  OffsetRepresentation() { };
  
  /// \brief Builds \p pointer's offset using \p base 
  OffsetRepresentation(const Value* Pointer, const Value* Base) { };
  
  /// \brief Destructor 
  virtual ~OffsetRepresentation() { };
  
  /// \brief Returns a copy of the represented offset
  virtual OffsetRepresentation* copy() =0;
  
  /// \brief Adds two offsets of the respective representation
  virtual OffsetRepresentation* add(OffsetRepresentation* Other) =0;
  
  /// \brief Answers true if two offsets are disjoints
  virtual bool disjoint(OffsetRepresentation* Other) =0;
  
  /// \brief Narrows the offset of the respective representation
  virtual OffsetRepresentation* narrow(CmpInst::Predicate Cmp, 
    OffsetRepresentation* Other) =0;
  
  /// \brief Widens the offset of the respective representation, Before and 
  ///   After are given so its possible to calculate direction of growth.
  virtual OffsetRepresentation* widen(OffsetRepresentation* Before,
    OffsetRepresentation* After) =0;
  
  /// \brief Prints the offset representation
  virtual void print() { }
  /// \brief Prints the offset to a file
  virtual void print(raw_fd_ostream& fs) { }
  
};

/// \brief Class that encapsulates the concept of a pointer's offset.
/// An offset is a set of offset representations, each with it's own 
/// capabilities and operations set.
/// Use it for testing new integer comparison analyses.
class Offset{

public:

  /// \brief Add custom offset representation initialization
  static void initialization(OffsetBasedAliasAnalysis* Analysis);

  /// \brief Add custom offset representation to reps for use in obaa.
  /// This constructor should return a neutral offset element
  Offset();
  
  /// \brief Creates the offset occording to \p a = \p b + offset
  Offset(const Value* A, const Value* B);
  
  /// \brief Add custom offset representation required analyses
  static void getAnalysisUsage(AnalysisUsage &AU);
  
  /// \brief Copy contructor 
  Offset(const Offset& Other);
  
  /// \brief Destructor 
  ~Offset();
  
  /// \brief Assignment operator that provides a deep copy
  Offset& operator=(const Offset& Other);
   
  /// \brief Adds two offsets
  Offset operator+(const Offset& Other) const;
  
  /// \brief Answers true if two offsets are disjoints
  bool operator!=(const Offset& Other) const;
  
  /// \brief Narrows the offset, Base is nacessary since you only use addresses
  /// with the same base for the narrowing process. 
  void narrow(const NarrowingOp& Narrowing_op, OffsetPointer* Base);
  
  /// \brief Widens the offset
  void widen(const WideningOp& Widening_op);
  
  /// \brief Prints the offset
  void print() const;
  /// \brief Prints the offset to a file
  void print(raw_fd_ostream& fs) const;
  
private:
  std::map<const int, OffsetRepresentation*> reps;
};

}

#endif
