cmake_minimum_required(VERSION 3.2)

SET(output_dir "${CMAKE_BINARY_DIR}/examples")

file(GLOB examples
    ${CMAKE_CURRENT_SOURCE_DIR}/${dir}/*.c
    ${CMAKE_CURRENT_SOURCE_DIR}/${dir}/*.cxx
    )


# build a binary in build/examples with the same name as
# the corresponding source file (minus the extension)
foreach(source_file ${examples})
    cmake_path(GET source_file STEM filestem)

    SET(example "${filestem}")

    add_executable(${example} ${source_file})
    target_link_libraries(${example} tarp)

    set_target_properties(${example} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${output_dir}"
        )

endforeach()
