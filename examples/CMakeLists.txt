SET(output_dir "${CMAKE_BINARY_DIR}/examples")

# configure the executable test target that has name tgname.
MACRO(CONFIGURE_TARGET tgname)
    target_link_libraries(${tgname} tarp)
    set_target_properties(${tgname}
        PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${output_dir}
    )
ENDMACRO()

add_executable(c_event_example
    c_event_example.c
)
CONFIGURE_TARGET(c_event_example)

add_executable(cxx_event_example
    cxx_event_example.cxx
)
CONFIGURE_TARGET(cxx_event_example)

add_executable(heap_dll_example
    heap_dll_example.c
)
CONFIGURE_TARGET(heap_dll_example)

add_executable(iniparse_example
    iniparse.c
)
CONFIGURE_TARGET(iniparse_example)

add_executable(uds-fdpass-server
    ./unix-domain-socket-fd-passing/server.c
)
CONFIGURE_TARGET(uds-fdpass-server)

add_executable(uds-fdpass-client
    ./unix-domain-socket-fd-passing/client.c
)
CONFIGURE_TARGET(uds-fdpass-client)

