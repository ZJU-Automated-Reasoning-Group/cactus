#include "PointerAnalysis/Engine/ContextSensitivity.h"

namespace tpa
{
    // Initialize the static policy member
    ContextSensitivityPolicy::Policy ContextSensitivityPolicy::activePolicy = ContextSensitivityPolicy::Policy::NoContext;
} 