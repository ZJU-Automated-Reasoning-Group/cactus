set(OBAA_SOURCES
    OBAA/Address.cpp
    OBAA/SSIfy.cpp
    OBAA/vSSA.cpp
    OBAA/RangeAnalysis.cpp
    OBAA/Narrowing.cpp
    OBAA/RAOffset.cpp
    OBAA/OffsetPointer.cpp
    OBAA/OffsetBasedAliasAnalysis.cpp
    OBAA/Offset.cpp
)

set(SRAA_SOURCES
    SRAA/StrictRelationAliasAnalysis.cpp
    SRAA/RangeAnalysis.cpp
    SRAA/vSSA.cpp
)

add_subdirectory(Canary)

add_library(Alias STATIC ${OBAA_SOURCES} ${SRAA_SOURCES})

target_include_directories(Alias PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(Alias PRIVATE 
    Util 
    Canary
    LLVMCore 
    LLVMAnalysis 
    LLVMSupport
    LLVMTransformUtils
)

if(DEFINED IN_SOURCE_BUILD)
    add_dependencies(Alias intrinsics_gen)
endif()
