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


# configure the executable test target that has name tgname.
MACRO(CONFIGURE_TARGET tgname)
    # naming the target with a test. prefix to take advantage of tab
    # autocompletion when looking for test targets to run.
    SET(test_runtg test.${tgname})

    target_include_directories(${tgname} PRIVATE
            ${PROJECT_SOURCE_DIR}/src/
    )

    target_link_libraries(${tgname} libtarp)

    set_target_properties(${tgname} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${output_dir}"
    )

    # add a target that runs the test binary; note this typically does not
    # run so the user must call it explicitly; having a separate target for
    # each subdirectory given is convenient if the test takes a long time to
    # run.
    # The dependency on the test binary ensures the binary is always built
    # first.
    add_custom_target(${test_runtg}
        DEPENDS ${tgname}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "Running ${output_dir}/${tgname} ..."
        COMMAND ${tgname}
    )

    add_dependencies(${test-runner} ${test_runtg})
ENDMACRO()


# For tests to be run the following structure is expected.
# 1) each logically separate set of tests should be in its own subdirectory,
#    nested directly under <ROOT PROJECT DIR>/tests/.
# 2) this subdirectory must contain .c or .cxx source files, and one of these
#    files must contain a main() function.
#
# For each subdirectory a separate executable binary is built (since each
# subdirectory contains a file with a main()) and then executed to run the tests.
# The binary is named tgname.test tgname is whatever name was given to
# add_executable. Typically it will be the subdirectory name of the respective
# test e.g. avl => avl.test.
# Each binary/set of tests can be run by calling a test.tgname
# target e.g. test.avl. All these targets can be invoked all at the same time
# by running the 'tests' target.
add_executable(avl
    avl/avl_tests.c
    avl/tests.c
)
CONFIGURE_TARGET(avl)

add_executable(bitarray
    bitarray/tests.c
)
CONFIGURE_TARGET(bitarray)

add_executable(cexec
    cexec/cexec.c
)
CONFIGURE_TARGET(cexec)

add_executable(cxxexec
    cxxexec/cxxexec.cxx
)
CONFIGURE_TARGET(cxxexec)

add_executable(dllist
    dllist/dllist_tests.c
    dllist/tests.c
)
CONFIGURE_TARGET(dllist)

add_executable(evchan
    evchan/evchan_test.cxx
    evchan/event_aggregator_test.cxx
    evchan/event_broadcaster_test.cxx
    evchan/event_rstream.cxx
    evchan/event_wstream.cxx
    evchan/trunk_test.cxx
    evchan/main.cxx
)
CONFIGURE_TARGET(evchan)

add_executable(bits
    bits/bits.cxx
)
CONFIGURE_TARGET(bits)

add_executable(hash.fletcher
    hash/fletcher.cxx
)
CONFIGURE_TARGET(hash.fletcher)

add_executable(hash.crc
    hash/crc.cxx
)
CONFIGURE_TARGET(hash.crc)

add_executable(hasht
    hasht/hasht_tests.c
    hasht/tests.c
)
CONFIGURE_TARGET(hasht)

add_executable(heap
    heap/heap_tests.c
    heap/tests.c
)
CONFIGURE_TARGET(heap)

add_executable(iniparse
    iniparse/tests.c
)
CONFIGURE_TARGET(iniparse)

add_executable(misc
    misc/tarputils_tests.c
    misc/tests.c
)
CONFIGURE_TARGET(misc)

add_executable(pipeline
    pipeline/pipeline.cxx
)
CONFIGURE_TARGET(pipeline)

add_executable(staq
    staq/staq_tests.c
    staq/tests.c
)
CONFIGURE_TARGET(staq)

add_executable(vector
    vector/tests.c
)
CONFIGURE_TARGET(vector)

add_executable(signals
    signals/tests.cxx
)
CONFIGURE_TARGET(signals)


