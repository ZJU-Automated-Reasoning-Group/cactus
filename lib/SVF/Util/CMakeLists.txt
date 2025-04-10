include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/SVF
)

# Need to specify paths relative to the parent directory
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