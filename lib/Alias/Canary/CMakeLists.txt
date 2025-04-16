set(DYCKAA_SOURCES
    DyckAA/EdgeLabel.cpp
    DyckAA/DyckAliasAnalysis.cpp
    DyckAA/AAAnalyzer.cpp
)

set(DYCKGRAPH_SOURCES
    DyckGraph/DyckVertex.cpp
    DyckGraph/DyckGraph.cpp
)

set(DYCKCG_SOURCES
    DyckCG/DyckCallGraphNode.cpp
    DyckCG/DyckCallGraph.cpp
)

add_library(Canary STATIC 
    ${DYCKAA_SOURCES} 
    ${DYCKGRAPH_SOURCES} 
    ${DYCKCG_SOURCES}
)

target_include_directories(Canary PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(Canary PRIVATE 
    Util 
    LLVMCore 
    LLVMAnalysis 
    LLVMSupport
)

if(DEFINED IN_SOURCE_BUILD)
    add_dependencies(Canary intrinsics_gen)
endif() 