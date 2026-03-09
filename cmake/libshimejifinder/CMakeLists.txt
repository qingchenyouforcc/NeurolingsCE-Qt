# Modified from:
#   https://github.com/pixelomer/libshimejifinder/blob/5b2ffb98f06939dae4aa075fabda2cf527abbaaf/CMakeLists.txt

if(NOT DEFINED LIBSHIMEJIFINDER_SOURCE_DIR)
  message(FATAL_ERROR "LIBSHIMEJIFINDER_SOURCE_DIR must be set to the path of libshimejifinder/")
endif()

if(NOT DEFINED LIBSHIMEJIFINDER_BIN_DIR)
  message(FATAL_ERROR "LIBSHIMEJIFINDER_BIN_DIR must be set to the path of the build directory for libshimejifinder/")
endif()

if(NOT DEFINED BIN2CPP_CMAKE_SCRIPT)
  message(FATAL_ERROR "BIN2CPP_CMAKE_SCRIPT must be set to the path of bin2cpp.cmake")
endif()

cmake_minimum_required(VERSION 3.14)
project(shimejifinder)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
set(BUILD_SHARED_LIBS OFF)
set(ENABLE_UNZIP OFF)
set(ENABLE_TEST OFF)
set(ENABLE_CAT OFF)
set(ENABLE_TAR OFF)
set(ENABLE_CPIO OFF)

# Generate C++ files from XML using add_custom_command (not add_custom_target)
# This allows CMake to track OUTPUT files and only regenerate when inputs change
set(DEFAULT_ACTIONS_CC ${LIBSHIMEJIFINDER_SOURCE_DIR}/shimejifinder/default_actions.cc)
set(DEFAULT_BEHAVIORS_CC ${LIBSHIMEJIFINDER_SOURCE_DIR}/shimejifinder/default_behaviors.cc)

add_custom_command(
    OUTPUT ${DEFAULT_ACTIONS_CC}
    COMMAND ${CMAKE_COMMAND}
        -DINPUT=${LIBSHIMEJIFINDER_SOURCE_DIR}/default_actions.xml
        -DOUTPUT=${DEFAULT_ACTIONS_CC}
        -P ${BIN2CPP_CMAKE_SCRIPT}
    WORKING_DIRECTORY ${LIBSHIMEJIFINDER_SOURCE_DIR}
    DEPENDS ${BIN2CPP_CMAKE_SCRIPT}
            ${LIBSHIMEJIFINDER_SOURCE_DIR}/default_actions.xml
    COMMENT "Generating default_actions.cc from default_actions.xml"
)

add_custom_command(
    OUTPUT ${DEFAULT_BEHAVIORS_CC}
    COMMAND ${CMAKE_COMMAND}
        -DINPUT=${LIBSHIMEJIFINDER_SOURCE_DIR}/default_behaviors.xml
        -DOUTPUT=${DEFAULT_BEHAVIORS_CC}
        -P ${BIN2CPP_CMAKE_SCRIPT}
    WORKING_DIRECTORY ${LIBSHIMEJIFINDER_SOURCE_DIR}
    DEPENDS ${BIN2CPP_CMAKE_SCRIPT}
            ${LIBSHIMEJIFINDER_SOURCE_DIR}/default_behaviors.xml
    COMMENT "Generating default_behaviors.cc from default_behaviors.xml"
)

if(NOT DEFINED CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()

if(NOT DEFINED SHIMEJIFINDER_BUILD_LIBARCHIVE)
    set(SHIMEJIFINDER_BUILD_LIBARCHIVE YES)
endif()

if(NOT DEFINED SHIMEJIFINDER_USE_LIBUNARR)
    set(SHIMEJIFINDER_USE_LIBUNARR YES)
endif()

if (SHIMEJIFINDER_USE_LIBUNARR)
    add_subdirectory(${LIBSHIMEJIFINDER_SOURCE_DIR}/unarr
                        ${LIBSHIMEJIFINDER_BIN_DIR}/unarr)
endif()
if(SHIMEJIFINDER_BUILD_LIBARCHIVE)
    add_subdirectory(${LIBSHIMEJIFINDER_SOURCE_DIR}/libarchive
                        ${LIBSHIMEJIFINDER_BIN_DIR}/libarchive)
else()
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(LIBARCHIVE REQUIRED libarchive)
endif()

add_library(
    shimejifinder STATIC
    ${LIBSHIMEJIFINDER_SOURCE_DIR}/shimejifinder/libarchive/archive.cc
    ${LIBSHIMEJIFINDER_SOURCE_DIR}/shimejifinder/libunarr/unarr_FILE.c
    ${LIBSHIMEJIFINDER_SOURCE_DIR}/shimejifinder/libunarr/archive.cc
    ${LIBSHIMEJIFINDER_SOURCE_DIR}/shimejifinder/analyze.cc
    ${LIBSHIMEJIFINDER_SOURCE_DIR}/shimejifinder/archive_folder.cc
    ${LIBSHIMEJIFINDER_SOURCE_DIR}/shimejifinder/archive.cc
    ${LIBSHIMEJIFINDER_SOURCE_DIR}/shimejifinder/archive_entry.cc
    ${LIBSHIMEJIFINDER_SOURCE_DIR}/shimejifinder/extract_target.cc
    ${LIBSHIMEJIFINDER_SOURCE_DIR}/shimejifinder/utils.cc
    ${DEFAULT_ACTIONS_CC}
    ${DEFAULT_BEHAVIORS_CC}
)
target_include_directories(
    shimejifinder PRIVATE
    ${LIBSHIMEJIFINDER_SOURCE_DIR}
)

if(NOT MSVC)
    if(CMAKE_BUILD_TYPE EQUAL "Release")
        target_compile_options(shimejifinder PRIVATE -O3 -Wall -Wextra -Werror -Wpedantic)
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
    else()
        target_compile_options(shimejifinder PRIVATE -O0 -g -Wall -Wextra -Werror -Wpedantic)
    endif()
else()
    set(_mode_t "unsigned short")
    target_compile_definitions(shimejifinder PRIVATE
        mode_t=${_mode_t}
    )
endif()

# No need for explicit dependency on default_xmls_target anymore
# CMake tracks the OUTPUT files from add_custom_command automatically
if(SHIMEJIFINDER_USE_LIBUNARR)
    target_link_libraries(shimejifinder unarr)
    add_dependencies(shimejifinder unarr)
else()
    add_definitions(-DSHIMEJIFINDER_NO_LIBUNARR=1)
endif()
if(SHIMEJIFINDER_BUILD_LIBARCHIVE)
    target_link_libraries(shimejifinder archive_static)
    add_dependencies(shimejifinder archive_static)
else()
    target_link_libraries(shimejifinder ${LIBARCHIVE_LIBRARIES})
    target_include_directories(shimejifinder PUBLIC ${LIBARCHIVE_INCLUDE_DIRS})
endif()
