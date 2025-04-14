//===------------------- Address.cpp - Pass definition ----------*- C++ -*-===//
//
//             Offset Based Alias Analysis for The LLVM Compiler
//
//===----------------------------------------------------------------------===//
///
/// \file
/// \brief
///
//===----------------------------------------------------------------------===//

// local includes
#include "OBAA/Address.h"
#include "OBAA/OffsetPointer.h"
#include "OBAA/Narrowing.h"
// llvm includes
#include "llvm/IR/Argument.h"
#include "llvm/IR/GlobalVariable.h"
// STL includes
#include <map>
#include <set>
#include <deque>

using namespace llvm;

/// \brief Main constructor
Address::Address(OffsetPointer* const A, OffsetPointer* const B, 
const Offset& O) : base(B), addressee(A), offset(O) {
  widened = false;
  
  if(isa<const Argument>(*(B->getPointer()))) argument = true;
  else argument = false;
  
  if(isa<const GlobalVariable>(*(B->getPointer()))) global = true;
  else global = false;
  
  //Inserts the address edge in the graph
  addressee->addresses.insert(this);
  base->bases.insert(this);
  
}

/// \brief Copy constructor
Address::Address(const Address& A) : 
base(A.base), addressee(A.addressee), offset(A.offset) {
  // Copy narrow operations manually since they have const values
  for (auto& pair : A.narrowing_ops) {
    narrowing_ops.insert(std::make_pair(pair.first, pair.second));
  }
  
  // Copy widening operations manually
  for (auto& pair : A.widening_ops) {
    widening_ops.insert(std::make_pair(pair.first, pair.second));
  }
  
  widened = A.widened;
  argument = A.argument;
  global = A.global;
  
  //Inserts the address edge in the graph
  addressee->addresses.insert(this);
  base->bases.insert(this);
}

/// \brief Destructor that removes the address from the graph
Address::~Address() {
  addressee->addresses.erase(this);
  base->bases.erase(this);
}

/// \brief Returns the base of this address
OffsetPointer* Address::getBase() const { return base; }

/// \brief Returns the addressee of this address
OffsetPointer* Address::getAddressee() const { return addressee; }

/// \brief Returns the offset of this address
const Offset Address::getOffset() const { return offset; }

/// \brief Returns whether this address was widened
bool Address::wasWidened() const { return widened; }

/// \brief Returns whether there was an argument in the path between base and
/// addressee 
bool Address::hasArgFlag() const { return argument; }

/// \brief Returns whether there was a global var in the path between base and
/// addressee 
bool Address::hasGlobalFlag() const { return global; }

/// \brief Function tha prints the object's informtation
void Address::print() const {
  errs() << "[";
  if(base->getPointer()->getValueName() == NULL)
    errs() << *(base->getPointer());
  else
    errs() << base->getPointer()->getName();
  errs() << "+";
  offset.print();  
  errs() << "]";
}

/// \brief Function that expands an address, this is the most important 
/// feature of this class. Expands an address to addresses using it's base 
/// pointer's base pointers as the new addresses' base pointers. Fn and Ad
/// are there to propagate a created widening operator if the base to be 
/// expanded was already expanded
void Address::Expand(std::deque<Address *>& Ad, std::set<Address *>& Fn) {
  if(expanded.find(base) == expanded.end()) {
    //if Base hasn't been expanded already
    std::set<Address*> aux = base->addresses;
    for(auto i : aux) {
      Address* new_address = new Address(addressee, i->base, offset+i->offset);
      
      if(argument) new_address->argument = true;
      else if(i->argument) new_address->argument = true;
      
      if(global) new_address->global = true;
      else if(i->global) new_address->global = true;
      
      new_address->expanded.clear();
      for (auto& pair : expanded) {
        new_address->expanded.insert(std::make_pair(pair.first, pair.second));
      }
      new_address->expanded.insert(
        std::pair<OffsetPointer*, Offset>(base, offset));
        
      new_address->narrowing_ops.clear();
      for (auto& pair : narrowing_ops) {
        new_address->narrowing_ops.insert(std::make_pair(pair.first, pair.second));
      }
      for(auto j : i->narrowing_ops)
      {
        if(new_address->narrowing_ops.find(j.first) == 
        new_address->narrowing_ops.end()) {
          const NarrowingOp new_no = j.second.contextualize(offset);
          std::pair<const Value*, const NarrowingOp> new_entry(j.first, new_no);
          new_address->narrowing_ops.insert(new_entry);
        }
      }

      new_address->widening_ops.clear();
      for (auto& pair : widening_ops) {
        new_address->widening_ops.insert(std::make_pair(pair.first, pair.second));
      }
      for(auto j : i->widening_ops) {
        new_address->widening_ops.insert(std::make_pair(j.first, j.second));
      }
      
      Ad.push_front(new_address);
    }
  } else {
    //if it has, there must be widening
    WideningOp new_wo(expanded.at(base), offset);
    
    for(auto i : Ad) {
      //add narrowing operators
      for(auto j : narrowing_ops) {
        if(i->narrowing_ops.find(j.first) == i->narrowing_ops.end()) {
          const NarrowingOp new_no = j.second;
          std::pair<const Value*, const NarrowingOp> new_entry(j.first, new_no);
          i->narrowing_ops.insert(new_entry);
        }
      }
      //add expanded
      for(auto j : expanded) {
        i->expanded.insert(j);
      }
      
      const Value* p = base->getPointer();
      if(i->widening_ops.find(p) == i->widening_ops.end()) {
        std::pair<const Value*, const WideningOp> new_entry(p, new_wo);
        i->widening_ops.insert(new_entry);
      }
    }
    
    for(auto i : Fn) {
      //add narrowing operators
      for(auto j : narrowing_ops) {
        if(i->narrowing_ops.find(j.first) == i->narrowing_ops.end()) {
          const NarrowingOp new_no = j.second;
          std::pair<const Value*, const NarrowingOp> new_entry(j.first, new_no);
          i->narrowing_ops.insert(new_entry);
        }
      }
      //add expanded
      for(auto j : expanded) {
        i->expanded.insert(j);
      }
      
      const Value* p = base->getPointer();
      if(i->widening_ops.find(p) == i->widening_ops.end()) {
        std::pair<const Value*, const WideningOp> new_entry(p, new_wo);
        i->widening_ops.insert(new_entry);
      }
    }
  }
  delete this;
}

//adds a NarrowingOp to the address
bool Address::associateNarrowingOp(const Value* V, const NarrowingOp& No) {
  if(narrowing_ops.find(V) == narrowing_ops.end()) {
    std::pair<const Value*, const NarrowingOp> new_entry(V, No);
    narrowing_ops.insert(new_entry);
    return true;
  }
  return false;
}

//adds a NarrowingOp to the address
bool Address::associateWideningOp(const Value* V, const WideningOp& Wo) {
  if(widening_ops.find(V) == widening_ops.end()) {
    std::pair<const Value*, const WideningOp> new_entry(V, Wo);
    widening_ops.insert(new_entry);
    return true;
  }
  return false;
}



