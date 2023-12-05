cmake_minimum_required(VERSION 3.4.3)
project(Sparrow)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_FLAGS "-fPIC -std=gnu++11 -O0 -fno-rtti -Wno-deprecated")


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
    message(STATUS "definitions in LLVM ${LLVM_DEFINITIONS}")
endif()


include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include
                    ${CMAKE_CURRENT_BINARY_DIR}/include
        )

add_subdirectory(lib)
add_subdirectory(tools)
