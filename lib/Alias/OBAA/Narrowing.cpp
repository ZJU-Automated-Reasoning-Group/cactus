//===----------------- Narrowing.cpp - Pass definition ----------*- C++ -*-===//
//
//             Offset Based Alias Analysis for The LLVM Compiler
//
//===----------------------------------------------------------------------===//
///
/// \file
/// \brief
///
//===----------------------------------------------------------------------===//

#include "Alias/OBAA/Narrowing.h"

using namespace llvm;

/// \brief Simple function that returns a contextualized version of the 
/// operator in address expansion
const NarrowingOp NarrowingOp::contextualize(const Offset& C) const {
  return NarrowingOp(cmp_op, cmp_v, (context + C));
}
