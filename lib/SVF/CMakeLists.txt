add_subdirectory(CUDD)

# Initialize empty SOURCES variable
set(SOURCES "")

# Add subdirectories for SVF components
add_subdirectory(Util)
add_subdirectory(MemoryModel)
add_subdirectory(MSSA)
add_subdirectory(SABER)
add_subdirectory(WPA)

# add_library(Svf MODULE ${SOURCES})
add_library(LLVMSvf STATIC ${SOURCES})

# Link SVF libraries with CUDD libraries
# target_link_libraries(Svf PRIVATE LLVMCudd)
target_link_libraries(LLVMSvf PRIVATE LLVMCudd) 