include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/SVF
)

# Need to specify paths relative to the parent directory
set(MEMORYMODEL_SOURCES
    MemoryModel/ConsG.cpp
    MemoryModel/LocationSet.cpp
    MemoryModel/LocMemModel.cpp
    MemoryModel/MemModel.cpp
    MemoryModel/PAGBuilder.cpp
    MemoryModel/PAG.cpp
    MemoryModel/CHA.cpp
    MemoryModel/PointerAnalysis.cpp
)

# Add MemoryModel sources to the parent directory's SOURCES variable
set(SOURCES ${SOURCES} ${MEMORYMODEL_SOURCES} PARENT_SCOPE) 