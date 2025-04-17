if(DEFINED IN_SOURCE_BUILD)
    set(LLVM_LINK_COMPONENTS Analysis)
    add_executable(wpa wpa.cpp)
    target_link_libraries(wpa LLVMSvf LLVMCudd ${LLVM_LINK_COMPONENTS})
else()
    llvm_map_components_to_libnames(llvm_libs Analysis)
    add_executable(wpa wpa.cpp)
    target_link_libraries(wpa LLVMSvf LLVMCudd ${llvm_libs})
endif()

set_target_properties(wpa PROPERTIES
                     RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)


