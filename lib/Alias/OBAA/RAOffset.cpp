//===----------------- RAOffset.cpp - Pass definition -----------*- C++ -*-===//
//
//             Offset Based Alias Analysis for The LLVM Compiler
//
//===----------------------------------------------------------------------===//
///
/// \file
/// \brief
///
//===----------------------------------------------------------------------===//

// project's includes
#include "Alias/OBAA/Offset.h"
#include "Alias/OBAA/RAOffset.h"
#include "Alias/OBAA/OffsetBasedAliasAnalysis.h"
#include "Alias/OBAA/RangeAnalysis.h"
// llvm's includes
#include "llvm/IR/InstrTypes.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/ADT/APInt.h"
#include "llvm/Pass.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Operator.h"
#include "llvm/IR/Type.h"
// libc includes
#include <map>
#include <vector>
#include <cassert>

InterProceduralRA<Cousot>* RAOffset::ra;
Primitives RAOffset::P;

using namespace llvm;

extern unsigned MAX_BIT_INT;
extern APInt Min;
extern APInt Max;
extern APInt Zero;

RAOffset::RAOffset() : r(Zero, Zero) { }

/// \brief Builds \p pointer's offset using \p base 
RAOffset::RAOffset(const Value* Pointer, const Value* Base) {
  if(const GetElementPtrInst* p = dyn_cast<GetElementPtrInst>(Pointer)) {
    processGEP(Base, p->idx_begin(), p->idx_end());
  } else if(const GEPOperator* p = dyn_cast<GEPOperator>(Pointer)) {
    processGEP(Base, p->idx_begin(), p->idx_end());
  } else {
    r = Range(Zero, Zero);
  }
}

/// \brief Destructor 
RAOffset::~RAOffset() { }

/// \brief Returns a copy of the represented offset
RAOffset* RAOffset::copy() {
  RAOffset* copy = new RAOffset();
  copy->r = r;
  return copy;
}

/// \brief Adds two offsets of the respective representation
RAOffset* RAOffset::add(OffsetRepresentation* Other) {
  RAOffset* result = new RAOffset();
  RAOffset * other = static_cast<RAOffset*>(Other);
  result->r = r.add(other->r);
  return result;
}

/// \brief Answers true if two offsets are disjoints
bool RAOffset::disjoint(OffsetRepresentation* Other) {
  RAOffset * other = static_cast<RAOffset*>(Other);
  return r.intersectWith(other->r).isEmpty();
}

/// \brief Narrows the offset of the respective representation
RAOffset* RAOffset::narrow(CmpInst::Predicate Cmp, OffsetRepresentation* Other){
  RAOffset* result = new RAOffset();
  RAOffset* other = static_cast<RAOffset*>(Other);
  
  if (Cmp == CmpInst::ICMP_EQ){
    //The narrowed version is equal to other
    result->r = Range(other->r.getLower(), other->r.getUpper());
  } else if (Cmp == CmpInst::ICMP_NE) {
    //The narrowed version is unequal to other
    if (r.getLower().slt(other->r.getLower())
    and r.getUpper().sgt(other->r.getUpper())) {
      //Whole part
      result->r = Range(r.getLower(), r.getUpper());
    } else if (r.getLower().slt(other->r.getLower())) {
      //Smaller part
      if (r.getUpper().sgt(other->r.getLower())) {
        result->r = Range(r.getLower(), other->r.getLower());
      }
      else {
        result->r = Range(r.getLower(), r.getUpper());
      }
    } else if (r.getUpper().sgt(other->r.getUpper())) {
      //Larger part
      if (r.getLower().slt(other->r.getUpper())) {
        result->r = Range(other->r.getUpper(), r.getUpper());
      }
      else {
        result->r = Range(r.getLower(), r.getUpper());
      }
    } else {
      result->r.setEmpty();
    }
  } else if (Cmp == CmpInst::ICMP_SGT) {
    if(r.getLower().slt(other->r.getUpper())) {
      result->r = Range(other->r.getUpper()+1, r.getUpper());
    } else {
      result->r = Range(r.getLower(), r.getUpper());
    }
  } else if (Cmp == CmpInst::ICMP_SLT) {
    if (r.getUpper().sgt(other->r.getLower())) {
      result->r = Range(r.getLower(), other->r.getLower()-1);
    } else {
      result->r = Range(r.getLower(), r.getUpper());
    }
  } else if (Cmp == CmpInst::ICMP_SLE) {
    if (r.getUpper().sgt(other->r.getLower())) {
      result->r = Range(r.getLower(), other->r.getLower());
    } else {
      result->r = Range(r.getLower(), r.getUpper());
    }
  } else if (Cmp == CmpInst::ICMP_SGE) {
    if(r.getLower().slt(other->r.getUpper())) {
      result->r = Range(other->r.getUpper(), r.getUpper());
    } else {
      result->r = Range(r.getLower(), r.getUpper());
    }
  }
  return result;
}

/// \brief Widens the offset of the respective representation, Before and 
///   After are given so its possible to calculate direction of growth.
RAOffset* RAOffset::widen(OffsetRepresentation* Before,
OffsetRepresentation* After) {
  RAOffset * before = static_cast<RAOffset*>(Before);
  RAOffset * after = static_cast<RAOffset*>(After);
  RAOffset* result = this->copy();

  if(after->r.getLower().slt(before->r.getLower()))
    result->r.setLower(Min);
  if(after->r.getUpper().sgt(before->r.getUpper()))
    result->r.setUpper(Max);

  return result;
}

/// \brief Prints the offset representation
void RAOffset::print() { r.print(errs()); }
/// \brief Prints the offset to a file
void RAOffset::print(raw_fd_ostream& fs) { r.print(fs); }


void RAOffset::initialization(OffsetBasedAliasAnalysis* Analysis) {
  RAOffset::ra = &(Analysis->getAnalysis<InterProceduralRA<Cousot> >());
}

/// This function processes the indexes of a GEP operation and returns
/// the actual bitwise range of its offset;
void RAOffset::processGEP(const Value* Base, const Use* idx_begin,
const Use* idx_end){
  //Number of primitive elements
  Type* base_ptr_type = Base->getType();
  int base_ptr_num_primitive = 
    RAOffset::P.getNumPrimitives(base_ptr_type->getPointerElementType());

  //parse first index
  Value* indx = idx_begin->get();

  if(ConstantInt* cint = dyn_cast<ConstantInt>(indx)) {
    int constant = cint->getSExtValue();
    //updating lower and higher ranges
    r.setLower(APInt(MAX_BIT_INT, base_ptr_num_primitive * constant));
    r.setUpper(APInt(MAX_BIT_INT, base_ptr_num_primitive * constant));
  } else {
    Range a = ra->getRange(indx);
    //updating lower and higher ranges
    r.setLower(APInt(MAX_BIT_INT, base_ptr_num_primitive) * a.getLower());
    r.setUpper(APInt(MAX_BIT_INT, base_ptr_num_primitive) * a.getUpper());
  }

  //parse sequential indexes
  int index = 0;
  for(int i = 1; (idx_begin + i) != idx_end; i++) {
    //Calculating Primitive Layout
    base_ptr_type = RAOffset::P.getTypeInside(base_ptr_type, index);
    std::vector<int> base_ptr_primitive_layout = 
      RAOffset::P.getPrimitiveLayout(base_ptr_type);

    Value* indx = (idx_begin + i)->get();
    if(ConstantInt* cint = dyn_cast<ConstantInt>(indx)) {
      int constant = cint->getSExtValue();

      APInt addons(MAX_BIT_INT,
        RAOffset::P.getSumBehind(base_ptr_primitive_layout, constant));
      Range addon(addons, addons);
      r = r.add(addon);

      index = constant;
    } else {
      Range a = RAOffset::ra->getRange(indx);

      r = r.add(
        Range(
          APInt(MAX_BIT_INT, RAOffset::P.getSumBehind(base_ptr_primitive_layout,
            a.getLower().getSExtValue())),
          APInt(MAX_BIT_INT, RAOffset::P.getSumBehind(base_ptr_primitive_layout,
            a.getUpper().getSExtValue()))
        )
      );
      
      index = 0;
    }
  }
}


/// Primitives class implementation
//Returns the sum of previous elements of vector
int Primitives::getSumBehind(std::vector<int> v, unsigned int i) {
  int s = 0;
  if(i > v.size())
    i = v.size();
  for(int j = i-1; j >= 0; j--)
    s += v[j];
  return s;
}

//Returns the type of the ith element inside type
Type* Primitives::getTypeInside(Type* type, int i) {
  if(type->isPointerTy())
    return type->getPointerElementType();
  else if(type->isArrayTy())
    return type->getArrayElementType();
  else if(type->isStructTy())
    return type->getStructElementType(i);
  else if(type->isVectorTy())
    return type->getVectorElementType();
  else
    return NULL; 
}

//Returns the number of primitive elements of type
int Primitives::getNumPrimitives(Type* type) {
  //Verifies if this number of primitives was calculated already
  for(unsigned int i = 0; i < NumPrimitives.size(); i++)
    if(NumPrimitives[i]->type == type)
      return NumPrimitives[i]->num;
  
  //if not
  int np;
  if(type->isArrayTy()) {
    int num = type->getArrayNumElements();
    Type* arrtype = type->getArrayElementType();
    int arrtypenum = getNumPrimitives(arrtype); 
    np = num * arrtypenum;
  } else if(type->isStructTy()) {
    int num = type->getStructNumElements();
    np = 0;
    for(int i = 0; i < num; i++) {
      Type* structelemtype = type->getStructElementType(i);
      np += getNumPrimitives(structelemtype);
    }
  } else if(type->isVectorTy()) {
    int num = type->getVectorNumElements();
    Type* arrtype = type->getVectorElementType();
    int arrtypenum = getNumPrimitives(arrtype); 
    np = num * arrtypenum;
  } else {
    np = type->getPrimitiveSizeInBits();
    ///The type is not any one of the above or a primitive type
    // TODO: validate
    if (np == 0) np = 1;
    //assert(np > 0 && "Unrecognized type");
  }
  
  NumPrimitives.insert(NumPrimitives.end(), new NumPrimitive(type, np));
  return np;
}

//Returns a vector with the primitive layout of type
std::vector<int> Primitives::getPrimitiveLayout(Type* type) {
  //Verifies if this layout was calculated already
  for(unsigned int i = 0; i < PrimitiveLayouts.size(); i++)
    if(PrimitiveLayouts[i]->type == type)
      return PrimitiveLayouts[i]->layout;
  
  //if not
    
  if(type->isArrayTy()) {
    int num = type->getArrayNumElements();
    std::vector<int> pm (num);
    Type* arrtype = type->getArrayElementType();
    int arrtypenum = getNumPrimitives(arrtype); 
    for(int i = 0; i < num; i++)
      pm[i] = arrtypenum;
    PrimitiveLayouts.insert(PrimitiveLayouts.end(), 
      new PrimitiveLayout(type, pm));
    return pm;
  } else if(type->isStructTy()) {
    int num = type->getStructNumElements();
    std::vector<int> pm (num);
    for(int i = 0; i < num; i++) {
      Type* structelemtype = type->getStructElementType(i);
      pm[i] = getNumPrimitives(structelemtype);
    }
    PrimitiveLayouts.insert(PrimitiveLayouts.end(), 
      new PrimitiveLayout(type, pm));
    return pm;
  } else if(type->isVectorTy()) {
    int num = type->getVectorNumElements();
    std::vector<int> pm (num); 
    Type* arrtype = type->getVectorElementType();
    int arrtypenum = getNumPrimitives(arrtype); 
    for(int i = 0; i < num; i++)
      pm[i] = arrtypenum;
    PrimitiveLayouts.insert(PrimitiveLayouts.end(), 
      new PrimitiveLayout(type, pm));
    return pm;
  } else {
    std::vector<int> pm (1);
    pm[0] = 1;
    PrimitiveLayouts.insert(PrimitiveLayouts.end(), 
      new PrimitiveLayout(type, pm));
    return pm;
  }
}
