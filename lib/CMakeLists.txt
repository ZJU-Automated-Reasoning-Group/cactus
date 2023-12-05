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

target_link_libraries(SparrowLib PRIVATE ${llvm_libs})

if(DEFINED IN_SOURCE_BUILD)
    add_dependencies(SparrowLib intrinsics_gen)
endif()

