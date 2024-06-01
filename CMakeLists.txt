cmake_minimum_required(VERSION 3.2)

SET(TARP_PACKAGE_FILES "${CMAKE_SOURCE_DIR}/package")

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
option(RUN_TESTS "Generate test targets" OFF)
option(BUILD_EXAMPLES "Build example binaries")

# compilation options for both c and c++
add_compile_options(
    -Wall
    -Wextra
    -Wuninitialized
    -Wshadow
    -Wpedantic
    -Werror
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

if (RUN_TESTS)
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
file(GLOB_RECURSE library_c_sources "src/*.c")
file(GLOB_RECURSE library_cxx_sources "src/*.cxx")

SET(libtarp_public_headers include)
SET(libtarp_private_headers
    ${libtarp_public_headers}
    src/
    )

add_library(tarp SHARED
    ${library_c_sources}
    ${library_cxx_sources}
    )

add_library(tarp::tarp ALIAS tarp)

set_target_properties(tarp PROPERTIES
    VERSION ${LIBTARP_VERSION_STRING}
    SOVERSION ${LIBTARP_MAJOR_VERSION}
)

# no distinction between interface and internal headers at the moment
target_include_directories(tarp PUBLIC
    $<INSTALL_INTERFACE:${libtarp_public_headers}>
    $<BUILD_INTERFACE:${libtarp_private_headers}>
    )

add_executable(md5hash
    src/hash/md5/md5sum-main.c
    src/hash/md5/md5sum.c
    )

target_include_directories(md5hash PUBLIC
    ${public_headers}
    )

############################
## Link-time dependencies #
###########################
#
target_link_libraries(tarp
    m)

target_link_libraries(md5hash
    tarp)

########################################
## Build artifact installation rules  #
#######################################
#
INSTALL(DIRECTORY ${libtarp_public_headers}/ DESTINATION include/)

set_target_properties(tarp md5hash
    PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

#################################################
# Artifact installation and target export
#################################################
SET(TARP_CMAKE_EXPORT_FILE "libtarp-targets")

INSTALL(TARGETS tarp md5hash
    EXPORT ${TARP_CMAKE_EXPORT_FILE}
    LIBRARY DESTINATION lib/
    ARCHIVE DESTINATION lib/
    RUNTIME DESTINATION bin/
    )

# Create the exports file; other projects, when package_find-ing
# this project, can then import this exports file and get all the
# exported targets.
export(TARGETS tarp NAMESPACE tarp::
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

# tests
if (RUN_TESTS)
    add_subdirectory(tests)
endif()

if (BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()


