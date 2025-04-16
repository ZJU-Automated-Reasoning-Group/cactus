//===------------- OffsetPointer.cpp - Pass definition ----------*- C++ -*-===//
//
//             Offset Based Alias Analysis for The LLVM Compiler
//
//===----------------------------------------------------------------------===//
///
/// \file
/// \brief
/// In addIntraProceduralAddresses, neutral offset constructor are being used 
/// for some types of pointers, this might be an issue for some possible offset
/// representations (to be honest I have no idea)
///
//===----------------------------------------------------------------------===//

// local includes
#include "Alias/OBAA/OffsetPointer.h"
#include "Alias/OBAA/Address.h"
#include "Alias/OBAA/OffsetBasedAliasAnalysis.h"
#include "Alias/OBAA/Offset.h"
// llvm includes
#include "llvm/IR/Value.h"
#include "llvm/IR/Type.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Argument.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Operator.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Instructions.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/Debug.h"
// STL includes
#include <cassert>

using namespace llvm;

/// \brief Simple constructor that recieves a simple Value* with pointer type
OffsetPointer::OffsetPointer(const Value* V) : pointer(V) { 
  //V must be a pointer
  assert(V->getType()->isPointerTy() && "Tried to build non pointer.");
  
  pointer_type = PointerTypes::Unk;  
}

/// \brief Constructor that recieves a simple Value* and a Type
OffsetPointer::OffsetPointer(const Value* V, PointerTypes Pt) : pointer(V) { 
  //V must be a pointer
  assert(V->getType()->isPointerTy() && "Tried to build non pointer.");
  
  pointer_type = Pt;  
}

/// \brief Returns the LLVM's Value to which the object represents
const Value* OffsetPointer::getPointer() const { return pointer; }

/// \brief Returns which kind of pointer in the graph this object
/// has
enum OffsetPointer::PointerTypes OffsetPointer::getPointerType() const { 
  return pointer_type; 
  
}

/// \brief Returns the initial iterator of the address set
std::set<Address *>::iterator OffsetPointer::addr_begin() const { 
  return addresses.begin();
}

/// \brief Returns the end iterator of the address set
std::set<Address *>::iterator OffsetPointer::addr_end() const { 
  return addresses.end();
}

/// \brief Returns whether the address set is empty
bool OffsetPointer::addr_empty() const { return addresses.empty(); }

/// \brief Returns the initial iterator of the bases set
std::set<Address *>::iterator OffsetPointer::bases_begin() const { 
  return bases.begin();
}

/// \brief Returns the end iterator of the bases set
std::set<Address *>::iterator OffsetPointer::bases_end() const { 
  return bases.end();
}

/// \brief Sets which kind of pointer in the graph this object has
void OffsetPointer::setPointerType(OffsetPointer::PointerTypes Pt) { 
  pointer_type = Pt; 
}

/// \brief Function tha prints the object's information
void OffsetPointer::print() const {
  errs() << "Pointer: " << *pointer << "\n";
  errs() << "Type: ";
  if(pointer_type == OffsetPointer::Unk)
    errs() << "Unknown\n";
  else if(pointer_type == OffsetPointer::Alloc)
    errs() << "Allocation\n";
  else if(pointer_type == OffsetPointer::Phi)
    errs() << "Phi\n";
  else if(pointer_type == OffsetPointer::Cont)
    errs() << "Continuous\n";
  else if(pointer_type == OffsetPointer::Null)
    errs() << "Null\n";
  
  errs() << "{";
  bool notFirst = false;
  for(auto i : addresses)
  {
    if(notFirst) errs() << "; ";
    i->print();
  }
  errs() << "}\n";
}

/// \brief Function that finds the pointer's possible addresses, in an intra-
/// procedural situation. TODO: Add debug code for unks
void OffsetPointer::addIntraProceduralAddresses(OffsetBasedAliasAnalysis*
Analysis){
  // Global variables in LLVM are pointers by definition with their own alloc
  if(isa<const GlobalVariable>(*pointer)) { pointer_type = Global; }
  else if(const Argument* p = dyn_cast<Argument>(pointer)) { 
    if(p->getName().equals("argv")) { pointer_type = Alloc; }
    else { pointer_type = Arg; }
  }
  else if(isa<const AllocaInst>(*pointer)) { 
    pointer_type = Alloc;
  }
  else if(const CallInst* p = dyn_cast<CallInst>(pointer)) { 
    Function* CF = p->getCalledFunction();
    if(CF) {
      if(strcmp( CF->getName().data(), "malloc") == 0) { 
        pointer_type = Alloc; 
      }
      else if(strcmp( CF->getName().data(), "calloc") == 0) { 
        pointer_type = Alloc; 
      }
      else if(strcmp( CF->getName().data(), "realloc") == 0)
      {
        /// realloc is of the same name as it's first argument
        pointer_type = Cont;
        const Value* base_ptr_value = p->getOperand(0);
        OffsetPointer* base_ptr = Analysis->getOffsetPointer(base_ptr_value);
        new Address(this, base_ptr, Offset());
      }
      else {
        pointer_type = Call;
      }
    }
  }
  else if(const BitCastInst* p = dyn_cast<BitCastInst>(pointer)) { 
    pointer_type = Cont;
    const Value* base_ptr_value = p->getOperand(0);
    OffsetPointer* base_ptr = Analysis->getOffsetPointer(base_ptr_value);
    new Address(this, base_ptr, Offset());
  }
  else if(isa<const LoadInst>(*pointer)) { 
    pointer_type = Unk;
  }
  else if(const PHINode* p = dyn_cast<PHINode>(pointer)) { 
    pointer_type = Phi;
    unsigned int num = p->getNumIncomingValues();
    for (unsigned int i = 0; i < num; i++)
    {
      const Value* base_ptr_value = p->getIncomingValue(i);
      OffsetPointer* base_ptr = Analysis->getOffsetPointer(base_ptr_value);
      new Address(this, base_ptr, Offset());
    }
  }
  else if(const GetElementPtrInst* p = dyn_cast<GetElementPtrInst>(pointer)) { 
    pointer_type = Cont;
    const Value* base_ptr_value = p->getPointerOperand();
    OffsetPointer* base_ptr = Analysis->getOffsetPointer(base_ptr_value);
    new Address(this, base_ptr, Offset(this->getPointer(), base_ptr_value));
  }
  else if(const GEPOperator* p = dyn_cast<GEPOperator>(pointer)) { 
    pointer_type = Cont;
    const Value* base_ptr_value = p->getPointerOperand();
    OffsetPointer* base_ptr = Analysis->getOffsetPointer(base_ptr_value);
    new Address(this, base_ptr, Offset(this->getPointer(), base_ptr_value));
  }
  else if(isa<const ConstantPointerNull>(*pointer)) { 
    pointer_type = Null;
  }
  else if(const ConstantExpr* p = dyn_cast<ConstantExpr>(pointer)) { 
    const char* operation = p->getOpcodeName();
    if(strcmp(operation, "bitcast") == 0) {
      pointer_type = Cont;
      const Value* base_ptr_value = p->getOperand(0);
      OffsetPointer* base_ptr = Analysis->getOffsetPointer(base_ptr_value);
      new Address(this, base_ptr, Offset());
    }
    else {
      pointer_type = Unk;
    }
  }
  else if(isa<const Function>(*pointer)) { 
    pointer_type = Alloc;
  }
  else { 
    pointer_type = Unk; 
  }
}

void OffsetPointer::addInterProceduralAddresses(OffsetBasedAliasAnalysis*
Analysis){
  if(const Argument* p = dyn_cast<Argument>(pointer)) {
    const Function* F = p->getParent();
    //Go through all the uses of the argument's function, the calls are
    // the addresses bases
    for(auto ui = F->user_begin(), ue = F->user_end(); ui != ue; ui++) {
      const User* u = *ui;
      if(const CallInst* caller = dyn_cast<CallInst>(u)) {
        int anum = caller->getNumArgOperands();
        int ano = p->getArgNo();
        if(ano <= anum) {
          // create address
          OffsetPointer* base = Analysis->getOffsetPointer
            (caller->getArgOperand(ano));
          if(base != NULL) 
            new Address(this, base, Offset());
        } else {
          /// TODO: support standard values in cases where the argument
          /// has a standard value and does not appear in function call
          DEBUG_WITH_TYPE("errors",
            errs() << "!: ERROR (Not enough arguments):\n");
          DEBUG_WITH_TYPE("errors", errs() << *p << " " << ano << "\n");
          DEBUG_WITH_TYPE("errors", errs() << *u << "\n");
          pointer_type = Unk;
          addresses.clear();
          return;
        }
      }
    }
  } else if (const CallInst* p = dyn_cast<CallInst>(pointer)){
    Function* CF = p->getCalledFunction();
    if(CF) {
      for (auto i = inst_begin(CF), e = inst_end(CF); i != e; i++)
        if(isa<const ReturnInst>(*i)) {
            /// create address
            const Value* ret_ptr = ((ReturnInst*)&(*i))->getReturnValue();
            OffsetPointer* base = Analysis->getOffsetPointer(ret_ptr);

            //if the returned value is not a pointer, there is something wrong
            assert(base != NULL);

            new Address(this, base, Offset());
        }
    }
  }
}

/// \brief Function that finds the pointer's path to the root of it's local
/// tree.
/// TODO: Optimize function to take advantage of topological ordering.
void OffsetPointer::getPathToRoot() {
  OffsetPointer* current = this;
  int index = 0;
  Offset offset;
  while(true) {
    path_to_root[current] = std::pair<int, Offset>(index, offset);
    if(current->addresses.size() == 1) {
      Address* addr = *(current->addresses.begin());
      current = addr->getBase();
      if(path_to_root.count(current)) {
        //This means that the local tree is actually a lonely loop
      	// so the local tree's root will be the pointer with the highest address
      	OffsetPointer* root = NULL;
      	for(auto i : path_to_root){
      		if(root < i.first) root = i.first;
      	}
      	local_root = root;
      	break;
      }
      index++;
      offset = offset + addr->getOffset();
    } else {
      local_root = current;
      break;
    }
  }
}








