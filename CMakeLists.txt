cmake_minimum_required(VERSION 3.4.3)
project(Cactus)
set(CMAKE_CXX_STANDARD 14)


#find LLVM package
if (NOT LLVM_BUILD_PATH)
    message(FATAL_ERROR "please specified the build folder of LLVM by -DLLVM_BUILD_PATH=")
else()
    message(STATUS "LLVM_BUILD_PATH set to : ${LLVM_BUILD_PATH}")
endif()

find_package(LLVM 3.6.2
        REQUIRED
        CONFIG
        PATHS ${LLVM_BUILD_PATH}
        )

if(NOT LLVM_FOUND)
    message(FATAL_ERROR "can not find LLVMConfig.cmake under paths: ${LLVM_BUILD_PATH}")
else()
    message(STATUS "Found LLVM : ${LLVM_PACKAGE_VERSION}")
    # Print the abstract path for 'LLVMConfig.cmake'
    message(STATUS "LLVMConfig.cmake is: ${LLVM_CONFIG}")
    message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")
    message(STATUS "LLVM header files locate in : ${LLVM_INCLUDE_DIRS}")
    message(STATUS "LLVM definitions is : ${LLVM_DEFINITIONS}")

    set(LLVM_REQUIRES_RTTI TRUE)
    set(LLVM_REQUIRES_EH TRUE)

    include_directories(${LLVM_INCLUDE_DIRS})
    link_directories(${LLVM_LIBRARY_DIRS})
    add_definitions(${LLVM_DEFINITIONS})

    # Enable ExternC for C linkage
    add_definitions(-D__STDC_LIMIT_MACROS)
    add_definitions(-D__STDC_CONSTANT_MACROS)

    message(STATUS "definitions in LLVM ${LLVM_DEFINITIONS}")
endif()

# Check for Python3 installation
find_package(PythonInterp 3 REQUIRED)
message(STATUS "Found Python ${PYTHON_VERSION_STRING}")
message(STATUS "Using python interpreter in: ${PYTHON_EXECUTABLE}")

# Check for C++14 support and set the compilation flag
include(CheckCXXCompilerFlag)
CHECK_CXX_COMPILER_FLAG("-std=c++14" COMPILER_SUPPORTS_CXX14)
if(COMPILER_SUPPORTS_CXX14)
	set(CMAKE_CXX_FLAGS "-fPIC -O3 -fno-rtti -Wall -Wno-deprecated")
else()
	message(STATUS "The compiler ${CMAKE_CXX_COMPILER} has no C++14 support. Please use a different C++ compiler.")
endif()

# Copy configuration files to build directory
file(COPY 
     ${CMAKE_CURRENT_SOURCE_DIR}/config/ptr.config
     ${CMAKE_CURRENT_SOURCE_DIR}/config/modref.config
     ${CMAKE_CURRENT_SOURCE_DIR}/config/taint.config
     DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

# Set up include paths more elegantly
set(CACTUS_INCLUDE_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/include/SVF
    ${CMAKE_CURRENT_SOURCE_DIR}/lib
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}/include
)

set(CUDD_INCLUDE_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}/lib/SVF/CUDD
    ${CMAKE_CURRENT_SOURCE_DIR}/lib/SVF/CUDD/CUDD
)

include_directories(
    ${CACTUS_INCLUDE_DIRS}
    ${CUDD_INCLUDE_DIRS}
)

# Specify library and binary output dir
set (EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR}/bin)

add_subdirectory(lib)
add_subdirectory(tools)
