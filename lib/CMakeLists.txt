include_directories (${tpa_SOURCE_DIR}/include)
add_subdirectory(Transforms)
add_subdirectory(Util)
add_subdirectory(Annotation)
add_subdirectory(PointerAnalysis)
add_subdirectory(Dynamic)
add_subdirectory(TaintAnalysis)
add_subdirectory(SVF)
add_subdirectory(Alias)

file (GLOB SOURCES
        FPAnalysis/*.cpp
        FPAnalysis/CHA/*.cpp
        )

add_library(SparrowLib STATIC ${SOURCES})

target_link_libraries(SparrowLib PRIVATE 
    #${llvm_libs} 
    LLVMSvf 
    Util 
    Alias
)

if(DEFINED IN_SOURCE_BUILD)
    add_dependencies(SparrowLib intrinsics_gen)
endif()

include(CTestUseLaunchers)