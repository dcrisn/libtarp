cmake_minimum_required(VERSION 3.2)

project(
    "libtarp"
    VERSION 0.1
    DESCRIPTION "C/C++ library of data structures and miscelllaneous utilities"
    LANGUAGES C CXX
    )

set(LIBTARP_MAJOR_VERSION 0)
set(LIBTARP_MINOR_VERSION 0)
set(LIBTARP_PATCH_NUMBER 1)


option(DEBUG "Build debug binaries" OFF)
option(USE_SANITIZERS "Use program sanitizers" OFF)
option(RUN_TESTS "Generate test targets" OFF)

add_compile_options(
    -Wall
    -Wextra
    -Wuninitialized
    -Wshadow
    -pedantic
    -Wnull-dereference
    -fstack-protector-all
    -fstrict-aliasing
    -Wstrict-aliasing
    -fdiagnostics-color=always
    -I${HOME}/.local/usr/include
    )

add_definitions(
    -D_POSIX_C_SOURCE=200809L
    # add functions from endian.h. Note the header and function names are
    # slightly different on the BSDs */
    -D_DEFAULT_SOURCE  # Linux, GNU libc (but probably musl etc)
    )

if (DEBUG)
    message("This is a debug *DEBUG* build")
    add_compile_options(-ggdb3)
    add_definitions(-DDEBUG_BUILD)
else()
    message("This is a *PRODUCTION* build")
    add_compile_options(-O3)
endif()

if (USE_SANITIZERS)
    foreach(flag
            -fsanitize=address;
            -fsanitize=leak;
            -fsanitize=undefined; # includes null, alignment, bounds-strict etc
            -fsanitize=pointer-compare;
            -fsanitize=pointer-subtract;
            #-fsanitize=thread
            )
        message("sanitizer flag is ${flag}")

        # TODO: find way to check if san libs exist / compiler supports the flags
        SET(has_flag 1)

        if (has_flag)
            add_compile_options(${flag})
            add_link_options(${flag})
        else()
            message(NOTICE "Failed to add sanitizer flag '${flag}' (unacceptable)")
        endif()

    endforeach()
    message("compile flags is ${COMPILE_OPTIONS}")
endif()

message("compile flags is ${COMPILE_OPTIONS}")

# set language standard
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 11)

# throw an error if standard not implemented instead of trying to compile
# with older standard.
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# actually stick to c11/c++11 proper: don't use gnu11 -- i.e. do not use
# compiler-specific extensions
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_CXX_EXTENSIONS OFF)

# make Makefiles spit out verbose output by default
set(CMAKE_VERBOSE_MAKEFILE ON)

set(public_headers "include")
file(GLOB_RECURSE library_c_sources "src/*.c")
file(GLOB_RECURSE library_cxx_sources "src/*.cxx")

#######################
# Target definitions #
######################
#
add_library(tarp SHARED
    ${library_c_sources}
    ${library_cxx_sources}
    )

set_target_properties(tarp PROPERTIES
    VERSION ${LIBTARP_MAJOR_VERSION}.${LIBTARP_MINOR_VERSION}.${LIBTARP_PATCH_NUMBER}
    SOVERSION ${LIBTARP_MAJOR_VERSION}
)

target_include_directories(tarp PUBLIC
    ${public_headers}
    )

add_executable(md5sum
    src/hash/md5/md5sum-main.c
    src/hash/md5/md5sum.c
    )

target_include_directories(md5sum PUBLIC
    ${public_headers}
    )

############################
## Link-time dependencies #
###########################
#
target_link_libraries(tarp
    m)

target_link_libraries(md5sum
    tarp)

########################################
## Build artifact installation rules  #
#######################################
#
INSTALL(DIRECTORY ${public_headers} DESTINATION usr)

set_target_properties(tarp md5sum
    PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

INSTALL(TARGETS tarp md5sum
    LIBRARY DESTINATION usr/lib
    ARCHIVE DESTINATION usr/lib
    RUNTIME DESTINATION usr/bin
    )


# tests
if (RUN_TESTS)
    add_subdirectory(tests)
endif()
