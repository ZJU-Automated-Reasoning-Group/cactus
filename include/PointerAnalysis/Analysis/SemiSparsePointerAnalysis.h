#pragma once

#include "PointerAnalysis/Analysis/PointerAnalysis.h"
#include "PointerAnalysis/Analysis/PointerAnalysisQueries.h"
#include "PointerAnalysis/Support/Env.h"
#include "PointerAnalysis/Support/Memo.h"
#include <memory>

namespace tpa
{

class SemiSparseProgram;

class SemiSparsePointerAnalysis: public PointerAnalysis<SemiSparsePointerAnalysis>
{
private:
	Env env;
	Memo memo;
public:
	SemiSparsePointerAnalysis() = default;

	void runOnProgram(const SemiSparseProgram&);

	PtsSet getPtsSetImpl(const Pointer*) const;
	
	// Get a non-const reference to the pointer manager
	PointerManager& getMutablePointerManager() { return ptrManager; }
	
	// Get a query interface for this pointer analysis
	std::unique_ptr<PointerAnalysisQueries> createQueryInterface() const
	{
		return std::make_unique<PointerAnalysisQueriesImpl<SemiSparsePointerAnalysis>>(*this);
	}
};

}
