cmake_minimum_required(VERSION 3.2)

SET(output_dir "${CMAKE_BINARY_DIR}/tests")

MACRO(SUBDIRLIST result curdir)
  FILE(GLOB children RELATIVE ${curdir} ${curdir}/*)
  SET(dirlist "")
  FOREACH(child ${children})
    IF(IS_DIRECTORY ${curdir}/${child})
      LIST(APPEND dirlist ${child})
    ENDIF()
  ENDFOREACH()
  SET(${result} ${dirlist})
ENDMACRO()

# create a symbolic link to the test scripts
# inside the test output directory.
SET(test_scripts_dir "${CMAKE_SOURCE_DIR}/tests/scripts/")
file(CREATE_LINK "${test_scripts_dir}" "${output_dir}/scripts" SYMBOLIC)

# Dummy target that causes all the specific test targets to run due to its
# dependency on them. This makes available a 'tests' target the user can call
# explicitly.
SET(test-runner "tests")
add_custom_target(${test-runner}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )

SUBDIRLIST(test_dirs ${CMAKE_CURRENT_SOURCE_DIR})

# For tests to be run the following structure is expected.
# 1) each logically separate set of tests should be in its own subdirectory,
#    nested directly under <ROOT PROJECT DIR>/tests/.
# 2) this subdirectory must contain .c or .cxx source files, and one of these
#    files must contain a main() function.
#
# For each subdirectory a separate executable binary is built (since each
# subdirectory contains a file with a main()) and then executed to run the tests.
# The binary is named <subdirectory name>.test.
# Each binary/set of tests can be run by calling a test.<subdirectory name>
# target. All these targets can be invoked at the same time by running the
# 'tests' target.
foreach(dir ${test_dirs})
    file(GLOB sources
        ${CMAKE_CURRENT_SOURCE_DIR}/${dir}/*.c
        ${CMAKE_CURRENT_SOURCE_DIR}/${dir}/*.cxx
        )

    if (sources)
        SET(test_bin "${dir}.test")
        # naming the target with a test. prefix to take advantage of tab
        # autocompletion when looking for test targets to run.
        SET(test_runtg "test.${dir}")

        add_executable(${test_bin} ${sources})

        target_include_directories(${test_bin} PRIVATE
            ${PROJECT_SOURCE_DIR}/src/)

        target_link_libraries(${test_bin} tarp)

        set_target_properties(${test_bin} PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY "${output_dir}"
            )

        # add a target that runs the test binary; note this typically does not
        # run so the user must call it explicitly; having a separate target for
        # each subdirectory given is convenient if the test take a long time to
        # run.
        # The dependency on the test binary ensures the binary is always built
        # first.
        add_custom_target(${test_runtg}
            DEPENDS ${test_bin}
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
            COMMENT "Running ${output_dir}/${test_bin} ..."
            COMMAND ${test_bin}
            )

        add_dependencies(${test-runner} ${test_runtg})
    endif()
endforeach()

