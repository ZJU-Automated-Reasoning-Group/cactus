set(UTIL_SOURCES
    Util/AnalysisUtil.cpp
    Util/CPPUtil.cpp
    Util/DataFlowUtil.cpp
    Util/PTACallGraph.cpp
    Util/RaceAnnotator.cpp
    Util/ThreadCallGraph.cpp
    Util/BreakConstantExpr.cpp
    Util/Conditions.cpp
    Util/ExtAPI.cpp
    Util/PathCondAllocator.cpp
    Util/PTAStat.cpp
    Util/ThreadAPI.cpp
)

# Add Util sources to the parent directory's SOURCES variable
set(SOURCES ${SOURCES} ${UTIL_SOURCES} PARENT_SCOPE) 