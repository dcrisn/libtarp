cmake_minimum_required(VERSION 3.25)

SET(TARP_PACKAGE_FILES "${CMAKE_SOURCE_DIR}/cmake")

project(
    "libtarp"
    VERSION 0.1
    DESCRIPTION "C/C++ library of data structures and miscelllaneous utilities"
    LANGUAGES C CXX
    )

set(LIBTARP_MAJOR_VERSION 0)
set(LIBTARP_MINOR_VERSION 0)
set(LIBTARP_PATCH_NUMBER 1)
set(LIBTARP_VERSION_STRING
    "${LIBTARP_MAJOR_VERSION}.${LIBTARP_MINOR_VERSION}.${LIBTARP_PATCH_NUMBER}")

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(DEBUG "Build debug binaries" OFF)
option(USE_SANITIZERS "Use program sanitizers" OFF)
option(BUILD_TESTS "Generate test targets" OFF)
option(BUILD_EXAMPLES "Build example binaries")

# compilation options for both c and c++
add_compile_options(
    -Wall
    -Wextra
    -Wuninitialized
    -Wshadow
    -Wpedantic
    #-Werror
    -Wnull-dereference
    -fwrapv
    -fstack-protector-all
    -fstrict-aliasing
    -Wstrict-aliasing
    -fdiagnostics-color=always
    -Wno-format-truncation
    -Wcast-align
    -Wpointer-arith
    )

# compilation options for c++ only.
add_compile_options(
  $<$<COMPILE_LANGUAGE:CXX>:-Wold-style-cast>
)

add_definitions(
    -D_POSIX_C_SOURCE=200809L
    # add functions from endian.h. Note the header and function names are
    # slightly different on the BSDs */
    -D_DEFAULT_SOURCE  # Linux, GNU libc (but probably musl etc)
    -D_BSD_SOURCE
    #-DAVL_NODES_WITH_HEIGHT_FIELD
    )

if (DEBUG)
    message("This is a *DEBUG* build")
    add_compile_options(-ggdb3)
    add_definitions(-DDEBUG_BUILD)
    SET(CMAKE_BUILD_TYPE Debug)
else()
    message("This is a *PRODUCTION* build")
    SET(CMAKE_BUILD_TYPE Release)
    add_compile_options(-O3)
    #add_definitions(-DNDEBUG)  # strip out asserts
endif()

if (USE_SANITIZERS)
    message("Trying to build with sanitizer support")
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
endif()

if (BUILD_TESTS)
    message("Build will include tests")
endif()

if (BUILD_EXAMPLES)
    message("Build will include examples")
endif()

# set language standard
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

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

#######################
# Target definitions #
######################
#

SET(libtarp_public_headers include)

SET(libtarp_private_headers
    ${libtarp_public_headers}
    ${PROJECT_SOURCE_DIR}/include/
    )

add_library(libtarp_objects OBJECT
    src/misc/bits.c
    src/misc/bits.cxx
    src/misc/bitarray.c
    src/misc/cohort.c
    src/misc/common.c
    src/misc/error.c
    src/misc/event.c
    src/misc/floats.c
    src/misc/hash.c
    src/misc/ioutils.c
    src/misc/ioutils.cxx
    src/misc/linux-event.c
    src/misc/log.c
    src/misc/math.c
    src/misc/process.c
    src/misc/timeutils.c
    src/misc/timeutils.cxx
    src/misc/threading.cxx
    src/misc/sched.cxx
    src/misc/string_utils.cxx
    src/misc/process.cxx
    src/misc/posix_signal_helpers.cxx
    src/misc/cancellation_token.cxx
    src/misc/idman.cxx
    src/misc/filters.cxx
    src/misc/buffer.cxx
    src/misc/event.cxx
    src/hash/md5/md5sum.c
    src/hash/sha/sha256.cxx
    src/hash/checksum.cxx
    src/hash/inet_checksum.cxx
    src/hash/adler32.cxx
    src/hash/crc/crc.cxx
    src/mods/avltree/avl.c
    src/mods/bst/bst.c
    src/mods/dllist/dllist.c
    src/mods/hasht/hasht.c
    src/mods/heap/explicit.c
    src/mods/heap/implicit.c
    src/mods/staq/staq.c
    src/mods/vector/vector.c
    src/parsers/iniparse.c
)

set_target_properties(libtarp_objects PROPERTIES
    POSITION_INDEPENDENT_CODE ON
)

# no distinction between interface and internal headers at the moment
target_include_directories(libtarp_objects PUBLIC
    $<INSTALL_INTERFACE:include/>
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include/>
)

add_library(libtarp SHARED)

add_library(tarp::misc ALIAS libtarp)

set_target_properties(libtarp PROPERTIES
    VERSION ${LIBTARP_VERSION_STRING}
    SOVERSION ${LIBTARP_MAJOR_VERSION}
    LIBRARY_OUTPUT_NAME tarp
)

# the target for client projects will be tarp::misc.
set_target_properties(libtarp PROPERTIES
    EXPORT_NAME misc
)

target_include_directories(libtarp PUBLIC
    $<INSTALL_INTERFACE:include/>
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include/>
)

add_executable(md5hash
    src/hash/md5/md5sum-main.c
)

add_executable(checksum
    src/hash/checksum-main.cxx
)
target_compile_options(checksum PUBLIC
    -Wno-unused-parameter
)

target_link_libraries(libtarp PRIVATE
    libtarp_objects
    m)

target_link_libraries(md5hash
    libtarp_objects)

target_link_libraries(checksum
    libtarp_objects)

########################################
## Build artifact installation rules  #
#######################################
#
INSTALL(DIRECTORY ${libtarp_public_headers}/ DESTINATION include/)

set_target_properties(libtarp md5hash checksum
    PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

#################################################
# Artifact installation and target export
#################################################
SET(TARP_CMAKE_EXPORT_FILE "libtarp-targets")

INSTALL(TARGETS libtarp md5hash checksum
    EXPORT ${TARP_CMAKE_EXPORT_FILE}
    LIBRARY DESTINATION lib/
    ARCHIVE DESTINATION lib/
    RUNTIME DESTINATION bin/
    )

# Create the exports file; other projects, when package_find-ing
# this project, can then import this exports file and get all the
# exported targets.
export(TARGETS libtarp NAMESPACE tarp::
    FILE ${PROJECT_BINARY_DIR}/${TARP_CMAKE_EXPORT_FILE}.cmake
    )

# Some standard paths according to gnu guidelines
include(GNUInstallDirs)

# Where to put the tarp cmake files that can be used
# by other cmake projects;
# NOTE: CMAKE_INSTALL_LIBDIR is from the GNUInstallDirs import.
SET(TARP_CMAKE_INSTALLATION_DIR ${CMAKE_INSTALL_LIBDIR}/cmake/tarp/)
SET(TARP_PKGCONFIG_INSTALLATION_DIR ${CMAKE_INSTALL_LIBDIR}/pkgconfig/)

SET(version_config_file ${PROJECT_BINARY_DIR}/libtarp-config-version.cmake)
SET(project_config_file ${PROJECT_BINARY_DIR}/libtarp-config.cmake)
SET(pkgconfig_pc_file ${PROJECT_BINARY_DIR}/libtarp.pc)

# CMake helpers for generating a package config file (used below).
include(CMakePackageConfigHelpers)

# Generate the package-version cmake config file into the build directory
write_basic_package_version_file(
    ${version_config_file}
    VERSION ${LIBTARP_VERSION_STRING}
    COMPATIBILITY SameMajorVersion)

# generate the config.cmake file into the build directory.
# this file is used by cmake's find_package.
configure_package_config_file(
    ${TARP_PACKAGE_FILES}/libtarp-config.cmake.in
    ${project_config_file}
    INSTALL_DESTINATION ${TARP_CMAKE_INSTALLATION_DIR})

# generate the .pc file used by the standard Linux pkg-config utility,
# and by the cmake check_modules wrapper/interface to it.
configure_file(
    "${TARP_PACKAGE_FILES}/libtarp.pc.in"
    "${pkgconfig_pc_file}"
    @ONLY
    )

# Install the actual cmake exports file that previous commands
# have gradually populated.
INSTALL(
    EXPORT
       ${TARP_CMAKE_EXPORT_FILE}
    DESTINATION
        ${TARP_CMAKE_INSTALLATION_DIR}
    NAMESPACE
       tarp::
    )

# Install config.cmake and config-version.cmake files.
install(
    FILES
       ${project_config_file}
       ${version_config_file}
    DESTINATION
       ${TARP_CMAKE_INSTALLATION_DIR}
    )

# install .pc pkg-config file.
install(
    FILES
       "${pkgconfig_pc_file}"
    DESTINATION
    "${TARP_PKGCONFIG_INSTALLATION_DIR}"
    )

include(cmake/dependencies.cmake)

# tests
if (BUILD_TESTS)
    add_subdirectory(tests)
endif()

if (BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()


