add_subdirectory(CUDD)

# Add include directories for SVF
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/SVF
)

# Initialize empty SOURCES variable
set(SOURCES "")

# Add subdirectories for SVF components
add_subdirectory(Util)
add_subdirectory(MemoryModel)
add_subdirectory(MSSA)
add_subdirectory(SABER)
add_subdirectory(WPA)

add_library(Svf MODULE ${SOURCES})
add_library(LLVMSvf STATIC ${SOURCES})

# Link SVF libraries with CUDD libraries
target_link_libraries(Svf PRIVATE LLVMCudd)
target_link_libraries(LLVMSvf PRIVATE LLVMCudd)

# Link with LLVM libraries
#llvm_map_components_to_libnames(llvm_libs
    #BitWriter 
    #Core 
    #IPO 
    #IrReader 
    #InstCombine 
    #Instrumentation
    #Target 
    #Linker 
    #Analysis 
    #ScalarOpts 
    #Support
#)   

#target_link_libraries(Svf PRIVATE ${llvm_libs})
#target_link_libraries(LLVMSvf PRIVATE ${llvm_libs})

if(DEFINED IN_SOURCE_BUILD)
    add_dependencies(Svf intrinsics_gen)
    add_dependencies(LLVMSvf intrinsics_gen)
endif()
