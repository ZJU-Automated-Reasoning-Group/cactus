#pragma once

#include "PointerAnalysis/FrontEnd/Type/ArrayLayoutMap.h"

namespace tpa
{

class TypeSet;

class ArrayLayoutAnalysis
{
public:
	ArrayLayoutAnalysis() = default;

	ArrayLayoutMap runOnTypes(const TypeSet&);
};

}
