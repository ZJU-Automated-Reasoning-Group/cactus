//===--------------- OffsetPointer.h - Pass definition ----------*- C++ -*-===//
//
//             Offset Based Alias Analysis for The LLVM Compiler
//
//===----------------------------------------------------------------------===//
///
/// \file
/// This is the pointer representation. A pointer is a set of possible addresses
/// that it can point to on program execution. This representation also holds
/// other information, like it's type and its local tree.
///
//===----------------------------------------------------------------------===//
#ifndef __OFFSET_POINTER_H__
#define __OFFSET_POINTER_H__

// Project's includes
#include "Alias/OBAA/Offset.h"
// llvm's includes
// libc includes
#include <set>

namespace llvm {

// Forward declarations
class OffsetBasedAliasAnalysis;
class Address;
class Value;

/// \brief Pointer representation in which each pointer is a set of
/// possible addresses
class OffsetPointer {
  
  // Address is a friend class because on each instance creation, offset
  // pointers bases and addresses are updated.
  friend class Address;
  friend class Offset;
  friend class OffsetBasedAliasAnalysis;

public:
  enum PointerTypes { 
    Unk = 0, Alloc = 1, Phi = 2, Cont = 3,
    Arg = 4, Call = 5, Global = 6, Null = 7 
  };

  // Contructors and destructors
  OffsetPointer(const Value* V);
  OffsetPointer(const Value* V, PointerTypes Pt);

  // Functions that provide the object's information
  const Value* getPointer() const;
  enum PointerTypes getPointerType() const;
  std::set<Address *>::iterator addr_begin() const;
  std::set<Address *>::iterator addr_end() const;
  bool addr_empty() const;
  std::set<Address *>::iterator bases_begin() const;
  std::set<Address *>::iterator bases_end() const;

  // Functions that set the object's information
  void setPointerType(PointerTypes Pt);

  /// \brief Function tha prints the object's information
  void print() const;

  /// \brief Function that finds the pointer's possible addresses,
  ///  this is the most important feature of this class.
  void addIntraProceduralAddresses(OffsetBasedAliasAnalysis* Analysis);
  void addInterProceduralAddresses(OffsetBasedAliasAnalysis* Analysis);
  
  void getPathToRoot();

private:
  const Value* const pointer;
  std::set<Address* > addresses;
  std::set<Address* > bases;
  PointerTypes pointer_type;
  // members that help topological ordering and scc finding
  int color;
  int scc;
  // function and structures for the local analysis
  OffsetPointer *local_root;
  std::map<OffsetPointer *, std::pair<int, Offset>> path_to_root;
  
};
}

#endif
