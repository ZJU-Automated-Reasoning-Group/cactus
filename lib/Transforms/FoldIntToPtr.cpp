#include "Transforms/FoldIntToPtr.h"

#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/PatternMatch.h>

using namespace llvm;
using namespace llvm::PatternMatch;

namespace transform
{

static bool foldInstruction(IntToPtrInst* inst)
{
	auto op = inst->getOperand(0)->stripPointerCasts();

	// Pointer copy: Y = inttoptr (ptrtoint X)
	Value* src = nullptr;
	if (match(op, m_PtrToInt(m_Value(src))))
	{
		// Make sure the types match before replacing
		if (src->getType() != inst->getType()) {
			src = new BitCastInst(src, inst->getType(), "ptr.cast", inst);
		}
		inst->replaceAllUsesWith(src);
		inst->eraseFromParent();
		return true;
	}

	// Pointer arithmetic
	Value* offsetValue = nullptr;
	if (match(op, m_Add(m_PtrToInt(m_Value(src)), m_Value(offsetValue))))
	{
		// Cast source pointer to the destination type if needed
		if (src->getType() != inst->getType())
			src = new BitCastInst(src, inst->getType(), "src.cast", inst);
		
		// For compatibility in LLVM 3.6.2, simplify to cast+add instead of GEP:
		// 1. Cast pointer to integer
		Type* intPtrTy = Type::getInt64Ty(inst->getContext());
		auto ptrAsInt = new PtrToIntInst(src, intPtrTy, "ptr.int", inst);
		
		// 2. Ensure offset value is the same integer type
		if (offsetValue->getType() != intPtrTy) {
			offsetValue = new SExtInst(offsetValue, intPtrTy, "offset.cast", inst);
		}
		
		// 3. Add the offset
		auto addInst = BinaryOperator::CreateAdd(ptrAsInt, offsetValue, "ptr.add", inst);
		
		// 4. Cast back to pointer
		auto result = new IntToPtrInst(addInst, inst->getType(), "ptr.result", inst);
		
		inst->replaceAllUsesWith(result);
		inst->eraseFromParent();
		return true;
	}

	return false;
}

bool FoldIntToPtrPass::runOnBasicBlock(BasicBlock& bb)
{
	bool modified = false;
	for (auto itr = bb.begin(); itr != bb.end(); )
	{
		auto inst = itr++;
		if (auto itpInst = dyn_cast<IntToPtrInst>(inst))
			modified |= foldInstruction(itpInst);
	}
	return modified;
}

char FoldIntToPtrPass::ID = 0;
static RegisterPass<FoldIntToPtrPass> X("fold-inttoptr", "Turn ptrtoint+arithmetic+inttoptr into gep", false, false);

}
