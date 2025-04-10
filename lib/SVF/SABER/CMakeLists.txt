include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/SVF
)

# Need to specify paths relative to the parent directory
set(SABER_SOURCES
    SABER/DoubleFreeChecker.cpp
    SABER/FileChecker.cpp
    SABER/LeakChecker.cpp
    SABER/ProgSlice.cpp
    SABER/SaberAnnotator.cpp
    SABER/SaberCheckerAPI.cpp
    SABER/SaberSVFGBuilder.cpp
    SABER/SrcSnkDDA.cpp
    SABER/UseAfterFreeChecker.cpp
    SABER/CFGReachabilityAnalysis.cpp
)

# Add SABER sources to the parent directory's SOURCES variable
set(SOURCES ${SOURCES} ${SABER_SOURCES} PARENT_SCOPE) 