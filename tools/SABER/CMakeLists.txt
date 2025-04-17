add_executable(saber saber.cpp)
target_link_libraries(saber LLVMSvf)
set_target_properties(saber PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
