#pragma once

#include "Context/Context.h"
#include "Util/Hashing.h"

#include <llvm/IR/Function.h>
#include <llvm/IR/GlobalVariable.h>

namespace tpa
{

enum class AllocSiteTag: std::uint8_t
{
	Null,
	Universal,
	Global,
	Function,
	Stack,
	Heap
};

class AllocSite
{
private:
	const context::Context* ctx;
	const llvm::Value* value;

	AllocSiteTag tag;

	AllocSite(AllocSiteTag t, const llvm::Value* v): ctx(context::Context::getGlobalContext()), value(v), tag(t) {}
	AllocSite(AllocSiteTag t, const context::Context* c, const llvm::Value* v): ctx(c), value(v), tag(t) {}
public:
	AllocSiteTag getAllocType() const { return tag; }

	const context::Context* getAllocContext() const { return ctx; }

	const llvm::GlobalVariable* getGlobalValue() const
	{
		assert(tag == AllocSiteTag::Global);
		return llvm::cast<llvm::GlobalVariable>(value);
	}

	const llvm::Value* getLocalValue() const
	{
		assert(tag == AllocSiteTag::Stack || tag == AllocSiteTag::Heap);
		return value;
	}

	const llvm::Function* getFunction() const
	{
		assert(tag == AllocSiteTag::Function);
		return llvm::cast<llvm::Function>(value);
	}

	bool operator==(const AllocSite& rhs) const
	{
		if (tag != rhs.tag)
			return false;

		switch (tag)
		{
			case AllocSiteTag::Universal:
			case AllocSiteTag::Null:
				return true;
			case AllocSiteTag::Global:
			case AllocSiteTag::Function:
				return value == rhs.value;
			case AllocSiteTag::Stack:
			case AllocSiteTag::Heap:
				return ctx == rhs.ctx && value == rhs.value;
		}
	}
	bool operator!=(const AllocSite& rhs) const
	{
		return !(*this == rhs);
	}

	static AllocSite getNullAllocSite()
	{
		return AllocSite(AllocSiteTag::Null, nullptr);
	}
	static AllocSite getUniversalAllocSite()
	{
		return AllocSite(AllocSiteTag::Universal, nullptr);
	}
	static AllocSite getGlobalAllocSite(const llvm::GlobalVariable* g)
	{
		assert(g != nullptr);
		return AllocSite(AllocSiteTag::Global, g);
	}
	static AllocSite getFunctionAllocSite(const llvm::Function* f)
	{
		assert(f != nullptr);
		return AllocSite(AllocSiteTag::Function, f);
	}
	static AllocSite getStackAllocSite(const context::Context* ctx, const llvm::Value* ptr)
	{
		assert(ptr != nullptr);
		return AllocSite(AllocSiteTag::Stack, ctx, ptr);
	}
	static AllocSite getHeapAllocSite(const context::Context* ctx, const llvm::Value* ptr)
	{
		assert(ptr != nullptr);
		return AllocSite(AllocSiteTag::Heap, ctx, ptr);
	}

	friend struct std::hash<AllocSite>;
};

}

namespace std
{

template<> struct hash<tpa::AllocSiteTag>
{
	size_t operator()(const tpa::AllocSiteTag& tag) const
	{
		return util::hashEnumClass(tag);
	}
};

template<> struct hash<tpa::AllocSite>
{
	size_t operator()(const tpa::AllocSite& site) const
	{
		return util::hashTriple(site.ctx, site.value, site.tag);
	}
};

}
