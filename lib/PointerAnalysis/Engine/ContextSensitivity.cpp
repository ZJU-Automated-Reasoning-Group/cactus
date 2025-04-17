#include "PointerAnalysis/Engine/ContextSensitivity.h"

namespace tpa
{
    /**
     * @brief Initialize the static policy member for context sensitivity.
     * 
     * The ContextSensitivityPolicy class manages the active context sensitivity policy
     * for the pointer analysis. Context sensitivity controls how calling contexts are 
     * distinguished during analysis, affecting both precision and performance.
     * 
     * Policies:
     * - NoContext: Context-insensitive analysis (least precise, fastest)
     * - SelectiveKCFA: Selective k-CFA with configurable k values per call site
     * - Introspective: Advanced context sensitivity guided by pre-analysis metrics
     * 
     * The default policy is NoContext (context-insensitive) for baseline performance.
     * This can be changed at runtime based on analysis requirements.
     */
    ContextSensitivityPolicy::Policy ContextSensitivityPolicy::activePolicy = ContextSensitivityPolicy::Policy::NoContext;
} 