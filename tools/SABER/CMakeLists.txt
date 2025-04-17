if(DEFINED IN_SOURCE_BUILD)
    set(LLVM_LINK_COMPONENTS 
        BitWriter Core IPO IrReader InstCombine Instrumentation
        Target Linker Analysis ScalarOpts Support
    )
    set(llvm_libs ${LLVM_LINK_COMPONENTS})
else()
    llvm_map_components_to_libnames(llvm_libs
        BitWriter Core IPO IrReader InstCombine Instrumentation
        Target Linker Analysis ScalarOpts Support
    )
endif()

add_executable(saber saber.cpp)
target_link_libraries(saber LLVMSvf LLVMCudd ${llvm_libs})
set_target_properties(saber PROPERTIES
                     RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
