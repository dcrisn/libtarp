include(FetchContent)

find_package( PkgConfig REQUIRED )

set(FETCHCONTENT_QUIET OFF CACHE BOOL "" FORCE)

SET(EXTERNAL_SOURCES_DIR "${CMAKE_SOURCE_DIR}/external")

############
# doctest  #
############
# c++-friendly testing framework
SET(DOCTEST_SRC_ROOT_DIR "${EXTERNAL_SOURCES_DIR}/doctest/")
SET(DOCTEST_GIT_TAG "v2.4.11")

if (BUILD_TESTS)
    FetchContent_Declare(
       doctest
       EXCLUDE_FROM_ALL
       GIT_REPOSITORY  https://github.com/doctest/doctest/
       GIT_TAG         ${DOCTEST_GIT_TAG}
       SOURCE_DIR      ${DOCTEST_SRC_ROOT_DIR}
       SYSTEM
     )
    FetchContent_MakeAvailable(doctest)
endif()

