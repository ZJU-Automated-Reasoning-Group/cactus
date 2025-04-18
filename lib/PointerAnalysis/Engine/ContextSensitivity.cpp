// Initialization only
#include "PointerAnalysis/Engine/ContextSensitivity.h"

namespace tpa
{
    // Initialize the default policy to use UniformKLimit
    ContextSensitivityPolicy::Policy ContextSensitivityPolicy::activePolicy = ContextSensitivityPolicy::Policy::UniformKLimit;
} 