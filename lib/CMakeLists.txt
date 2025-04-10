include_directories (${tpa_SOURCE_DIR}/include)
add_subdirectory(Transforms)
add_subdirectory(Util)
add_subdirectory(Annotation)
add_subdirectory(PointerAnalysis)
add_subdirectory(Dynamic)
add_subdirectory(TaintAnalysis)
add_subdirectory(SVF)

file (GLOB SOURCES
        FPAnalysis/Steensgaard/DyckCG/*.cpp
        FPAnalysis/Steensgaard/DyckAA/*.cpp
        FPAnalysis/Steensgaard/DyckGraph/*.cpp
        FPAnalysis/*.cpp
        SparrowUtils/*.cpp
        FPAnalysis/CHA/*.cpp
        )

add_library(SparrowLib STATIC ${SOURCES})

llvm_map_components_to_libnames(llvm_libs bitwriter core ipo irreader instcombine instrumentation target linker analysis scalaropts support )

target_link_libraries(SparrowLib PRIVATE ${llvm_libs} LLVMSvf)

if(DEFINED IN_SOURCE_BUILD)
    add_dependencies(SparrowLib intrinsics_gen)
endif()

