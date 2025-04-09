if(DEFINED IN_SOURCE_BUILD)
 set(LLVM_LINK_COMPONENTS BitWriter Core IPO IrReader InstCombine Instrumentation Target Linker Analysis ScalarOpts Support soarLib Cudd)
 add_llvm_tool(sparrow sparrow.cpp)
else()
 llvm_map_components_to_libnames(llvm_libs bitwriter core ipo irreader instcombine instrumentation target linker analysis scalaropts support transformutils)
 add_executable(sparrow sparrow.cpp)

 target_link_libraries(sparrow SparrowLib ${llvm_libs} ) #if using SVF
 #target_link_libraries(sparrow sailLib ${llvm_libs} )
endif()
