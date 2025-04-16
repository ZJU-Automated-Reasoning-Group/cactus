//===------------------- Narrowing.h - Pass definition ----------*- C++ -*-===//
//
//             Offset Based Alias Analysis for The LLVM Compiler
//
//===----------------------------------------------------------------------===//
///
/// \file
/// This is a simple interface dedicated to hold information about narrowing
/// and widening operations for obaa. Since these operations are executed
/// last in obaa, these helper structures are necessary for storage.
///
//===----------------------------------------------------------------------===//
#ifndef __NARROWING_H__
#define __NARROWING_H__

// local includes
#include "Alias/OBAA/Offset.h"
// llvm's includes
#include "llvm/IR/InstrTypes.h"
// libc includes
#include <set>

namespace llvm {

// Forward declarations
class Value;
class PHINode;
class OffsetPointer;

/// \brief struct that holds comparisons in the program. It is useful 
/// to create in obaa the narrowing operators.
struct NarrowingData {
  const CmpInst::Predicate cmp_op;
  const Value *cmp_v1;
  const Value *cmp_v2;
  std::set<const PHINode *> sigmas;
  
  NarrowingData(const CmpInst::Predicate Op, const Value* V1, const Value* V2) 
    : cmp_op(Op), cmp_v1(V1), cmp_v2(V2) { }
};

/// \brief struct that hold information about a narrowing operation to be 
/// performed by obaa
struct NarrowingOp {
  const CmpInst::Predicate cmp_op;
  OffsetPointer* const cmp_v;
  const Offset context;

  /// \brief constructor that doesn't epecify any context, for using early
  NarrowingOp(const CmpInst::Predicate Op, OffsetPointer* const V) 
    : cmp_op(Op), cmp_v(V) { }
  /// \brief Constructor that specify context, for using on address expansion
  NarrowingOp(const CmpInst::Predicate Op, OffsetPointer* const V, 
    const Offset C) : cmp_op(Op), cmp_v(V), context(C) { }
  /// \brief Simple function that returns a contextualized version of the 
  /// operator in address expansion
  const NarrowingOp contextualize(const Offset& C) const;
};

/// \brief struct that hold information about a widening operation to be 
/// performed by obaa
struct WideningOp {
  const Offset before;
  const Offset after;
  
  WideningOp(const Offset B, const Offset A) : before(B), after(A) { }
};
}

#endif
