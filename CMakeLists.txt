# must come first, before anything else -- sets the cmake version
# with the features and syntax understood
cmake_minimum_required(VERSION 3.2)

# name of this project
project(
    "libtarp"
    VERSION 0.1
    DESCRIPTION "C library of data structures and miscelllaneous utilities"
    LANGUAGES C
    )

# this must come before any targets you want the flags to apply to
add_compile_options(
    -Wall 
    -Wextra 
    -Wuninitialized  # already covered by Wextra, but worth being explicit
    -Wshadow
    -pedantic
    -O3
    -g
    -Wnull-dereference
    -fsanitize=null
    -fsanitize=address
    -fsanitize=leak
    -fsanitize=bounds-strict
    #-fsanitize=thread
    -fsanitize=undefined
    -fsanitize=return
    -fsanitize=alignment
    -fstack-protector-all
    -fstrict-aliasing
    -Wstrict-aliasing
    )

# you can -D macros either as part of the compile options above or
# better -- use the following:
add_definitions(
    -D_POSIX_C_SOURCE=200809L
    )

# setting the language standard can be done like this
set(CMAKE_C_STANDARD 11)

# more generally, you can set individual properties using:
#set_property(TARGET tgt PROPERTY C_STANDARD 11)
# or set an arbitrary number of properties all at once like this:
# 
# set_target_properties(myTarget
#     PROPERTIES
#         CXX_STANDARD 11
#         CXX_STANDARD_REQUIRED YES
#         CXX_EXTENSIONS NO
#)

# throw an error if standard not implemented instead of trying to compile
# with older standard.
set(CMAKE_C_STANDARD_REQUIRED ON)

# actually stick to c11, don't use gnu11 -- i.e. do not use
# compiler-specific extensions
set(CMAKE_C_EXTENSIONS OFF)

set(namespace "tarp")
set(common_headers "common/include")
file(GLOB common_sources "common/src/*.c")

set(staq_headers "mods/staq/include")
file(GLOB staq_sources "mods/staq/src/*.c")

# cohort test runner
set(cohort_dir "cohort")
file(GLOB cohort_sources "cohort/*.c")

add_library(tarp SHARED 
    ${common_sources}
    ${staq_sources}
    )

target_include_directories(tarp PUBLIC
    ${common_headers}
    ${staq_headers}
    )

###########
# tests   #
###########
# tests for common headers and sources
add_executable(common_tests
    ${cohort_sources}
    common/tests/tests.c
    common/tests/tarputils_tests.c
    )
target_include_directories(common_tests PUBLIC 
    ${common_headers}
    ${cohort_dir}
    )

# asan and ubsan are needed for the executable to be runnable
# since we passed all the sanitization compiler flags.
# note libasan MUST come FIRST
target_link_libraries(common_tests asan ubsan tarp)

# staq tests
add_executable(staq_tests
    ${cohort_sources}
    ${staq_sources}
    mods/staq/tests/tests.c
    mods/staq/tests/staq_tests.c
    )
target_include_directories(staq_tests PUBLIC 
    ${cohort_dir}
    ${common_headers}
    ${staq_headers}
    )

# asan and ubsan are needed for the executable to be runnable
# since we passed all the sanitization compiler flags.
# note libasan MUST come FIRST
target_link_libraries(staq_tests m asan ubsan tarp)

add_custom_target(test
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMENT "Running libtarp tests ..."
  COMMAND common_tests
  COMMAND staq_tests
)




set(CMAKE_VERBOSE_MAKEFILE ON)
# users should set it
# set(CMAKE_INSTALL_PREFIX /home/vcsaturninus/common/playground/staging)

#add_executable(my_program program.cpp)
##target_include_directories(my_program
## PRIVATE
##  first_dir/
##  second_dir/)
#target_include_directories(my_lib PUBLIC some_directory)

#add_executable(my_program my_program_source.cpp)
#target_link_libraries(my_program PRIVATE my_lib)

install(TARGETS tarp LIBRARY DESTINATION usr/lib)

install(DIRECTORY ${common_headers} DESTINATION usr)


#add_dependencies(test1 test2_run)
