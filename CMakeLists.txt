cmake_minimum_required(VERSION 3.4.3)
project(Cactus)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_FLAGS "-fPIC -std=gnu++14 -O0 -fno-rtti -Wno-deprecated")


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
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14 -fno-rtti -Wall -fPIC")
else()
	message(STATUS "The compiler ${CMAKE_CXX_COMPILER} has no C++14 support. Please use a different C++ compiler.")
endif()

configure_file(config/ptr.config ptr.config COPYONLY)
configure_file(config/modref.config modref.config COPYONLY)
configure_file(config/taint.config taint.config COPYONLY)


include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/include/SVF
    ${CMAKE_CURRENT_SOURCE_DIR}/lib/SVF/CUDD
    ${CMAKE_CURRENT_SOURCE_DIR}/lib/SVF/CUDD/CUDD
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/lib
)

# Specify library and binary output dir
set (EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR}/bin)

add_subdirectory(lib)
add_subdirectory(tools)
