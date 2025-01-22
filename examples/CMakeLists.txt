SET(output_dir "${CMAKE_BINARY_DIR}/examples")

add_executable(c_event_example
    c_event_example.c
)
target_link_libraries(c_event_example tarp)
set_target_properties(c_event_example
    PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${output_dir}
)

add_executable(cxx_event_example
    cxx_event_example.cxx
)
target_link_libraries(cxx_event_example tarp)
set_target_properties(cxx_event_example
    PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${output_dir}
)

add_executable(heap_dll_example
    heap_dll_example.c
)
target_link_libraries(heap_dll_example tarp)
set_target_properties(heap_dll_example
    PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${output_dir}
)

add_executable(iniparse_example
    iniparse.c
)
target_link_libraries(iniparse_example tarp)
set_target_properties(iniparse_example
    PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${output_dir}
)
