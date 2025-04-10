include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/SVF
)

# Need to specify paths relative to the parent directory
set(MSSA_SOURCES
    MSSA/MemPartition.cpp
    MSSA/MemRegion.cpp
    MSSA/MemSSA.cpp
    MSSA/SVFGBuilder.cpp
    MSSA/SVFG.cpp
    MSSA/SVFGOPT.cpp
    MSSA/SVFGStat.cpp
)

# Add MSSA sources to the parent directory's SOURCES variable
set(SOURCES ${SOURCES} ${MSSA_SOURCES} PARENT_SCOPE) 