# Modified from:
#   https://github.com/pixelomer/libshimejifinder/blob/226d97a4de3b3b0ed849163c841b422a6b389a5c/CMakeLists.txt

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
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build static libraries" FORCE)
if(NOT TARGET pugixml)
    add_subdirectory(
        ${LIBSHIMEJIFINDER_SOURCE_DIR}/pugixml
        ${LIBSHIMEJIFINDER_BIN_DIR}/pugixml
    )
endif()
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
    set(SHIMEJIFINDER_BUILD_LIBARCHIVE ON)
endif()

if(NOT DEFINED SHIMEJIFINDER_USE_LIBUNARR)
    set(SHIMEJIFINDER_USE_LIBUNARR ON)
endif()

if(SHIMEJIFINDER_USE_LIBUNARR)
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build static libraries" FORCE)
    add_subdirectory(${LIBSHIMEJIFINDER_SOURCE_DIR}/unarr
                        ${LIBSHIMEJIFINDER_BIN_DIR}/unarr)
    # unarr unconditionally adds -flto for MINGW/UNIX builds, which produces
    # LTO-only object files in libunarr.a. When shimejifinder (non-LTO) links
    # against it, the linker cannot resolve the LTO IR symbols, causing
    # "undefined reference to ar_open_*" errors. Strip -flto here so that
    # libunarr.a contains regular machine-code symbols.
    get_target_property(_unarr_compile_opts unarr COMPILE_OPTIONS)
    if(_unarr_compile_opts)
        list(REMOVE_ITEM _unarr_compile_opts -flto)
        set_target_properties(unarr PROPERTIES COMPILE_OPTIONS "${_unarr_compile_opts}")
    endif()
endif()
if(SHIMEJIFINDER_USE_LIBARCHIVE)
    if(SHIMEJIFINDER_BUILD_LIBARCHIVE)
        set(BUILD_SHARED_LIBS OFF)
        add_subdirectory(${LIBSHIMEJIFINDER_SOURCE_DIR}/libarchive
                            ${LIBSHIMEJIFINDER_BIN_DIR}/libarchive)
    else()
        find_package(PkgConfig REQUIRED)
        pkg_check_modules(LIBARCHIVE REQUIRED libarchive)
    endif()
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
    ${LIBSHIMEJIFINDER_SOURCE_DIR}/shimejifinder/extractor.cc
    ${LIBSHIMEJIFINDER_SOURCE_DIR}/shimejifinder/fs_extractor.cc
    ${LIBSHIMEJIFINDER_SOURCE_DIR}/shimejifinder/memory_extractor.cc
    ${LIBSHIMEJIFINDER_SOURCE_DIR}/shimejifinder/utf8_convert/jni.cc
    ${LIBSHIMEJIFINDER_SOURCE_DIR}/shimejifinder/utf8_convert/icu.cc
    ${LIBSHIMEJIFINDER_SOURCE_DIR}/shimejifinder/utf8_convert/iconv.cc
    ${LIBSHIMEJIFINDER_SOURCE_DIR}/shimejifinder/utils.cc
    ${DEFAULT_ACTIONS_CC}
    ${DEFAULT_BEHAVIORS_CC}
)
target_include_directories(
    shimejifinder PRIVATE
    ${LIBSHIMEJIFINDER_SOURCE_DIR}
)

if(SHIMEJIFINDER_USE_CUSTOM_UTF8_CONVERT)
    # consumer of libshimejifinder must provide shimejifinder::is_valid_utf8
    # and shimejifinder::shift_jis_to_utf8
    target_compile_definitions(shimejifinder PUBLIC -DSHIMEJIFINDER_USE_CUSTOM_UTF8_CONVERT=1
        -DSHIMEJIFINDER_HAS_UTF8_CONVERT=1)
elseif(SHIMEJIFINDER_USE_ICU)
    find_package(ICU COMPONENTS uc REQUIRED)
    target_link_libraries(shimejifinder ICU::uc)
    target_compile_definitions(shimejifinder PUBLIC -DSHIMEJIFINDER_USE_ICU=1
        -DSHIMEJIFINDER_HAS_UTF8_CONVERT=1)
elseif(SHIMEJIFINDER_USE_ICONV)
    find_package(Iconv)
    target_link_libraries(shimejifinder Iconv::Iconv)
    target_compile_definitions(shimejifinder PUBLIC -DSHIMEJIFINDER_USE_ICONV=1
        -DSHIMEJIFINDER_HAS_UTF8_CONVERT=1)
elseif(SHIMEJIFINDER_USE_JNI)
    find_package(JNI REQUIRED)
    target_link_libraries(shimejifinder JNI::JNI JNI::JVM)
    target_include_directories(shimejifinder PUBLIC ${JNI_INCLUDE_DIRS})
    target_compile_definitions(shimejifinder PUBLIC -DSHIMEJIFINDER_USE_JNI=1
        -DSHIMEJIFINDER_HAS_UTF8_CONVERT=1)
elseif(ANDROID AND (SHIMEJIFINDER_USE_JNI OR NOT DEFINED SHIMEJIFINDER_USE_JNI))
    target_link_libraries(shimejifinder android)
    target_compile_definitions(shimejifinder PUBLIC -DSHIMEJIFINDER_USE_JNI=1
        -DSHIMEJIFINDER_HAS_UTF8_CONVERT=1)
else()
    target_compile_definitions(shimejifinder PUBLIC
        -DSHIMEJIFINDER_HAS_UTF8_CONVERT=0)
endif()


if(NOT MSVC)
    if(CMAKE_BUILD_TYPE EQUAL "Release")
        target_compile_options(shimejifinder PRIVATE -O3 -Wall -Wextra -Werror -Wpedantic)
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
    else()
        target_compile_options(shimejifinder PRIVATE -O0 -g -Wall -Wextra -Werror -Wpedantic)
    endif()
else()
    set(_mode_t "unsigned short")
    target_compile_definitions(shimejifinder PRIVATE mode_t=${_mode_t})
    target_compile_options(shimejifinder PRIVATE /utf-8)
endif()

add_dependencies(shimejifinder pugixml)
target_link_libraries(shimejifinder PRIVATE pugixml)

if(NOT SHIMEJIFINDER_USE_LIBARCHIVE AND NOT SHIMEJIFINDER_USE_LIBUNARR)
    message(FATAL_ERROR "At least one of SHIMEJIFINDER_USE_LIBARCHIVE or SHIMEJIFINDER_USE_LIBUNARR must be enabled")
endif()

if(SHIMEJIFINDER_USE_LIBUNARR)
    add_dependencies(shimejifinder unarr)
    target_link_libraries(shimejifinder PRIVATE unarr)
    target_compile_definitions(shimejifinder PUBLIC SHIMEJIFINDER_NO_LIBUNARR=0)
else()
    target_compile_definitions(shimejifinder PUBLIC SHIMEJIFINDER_NO_LIBUNARR=1)
endif()

if(SHIMEJIFINDER_USE_LIBARCHIVE)
    if(SHIMEJIFINDER_BUILD_LIBARCHIVE)
        add_dependencies(shimejifinder archive_static)
        target_link_libraries(shimejifinder PUBLIC archive_static)
        if(WIN32)
            target_compile_definitions(shimejifinder PUBLIC LIBARCHIVE_STATIC)
        endif()
    else()
        target_link_libraries(shimejifinder PUBLIC ${LIBARCHIVE_LIBRARIES})
        target_include_directories(shimejifinder PUBLIC ${LIBARCHIVE_INCLUDE_DIRS})
    endif()
    target_compile_definitions(shimejifinder PUBLIC SHIMEJIFINDER_NO_LIBARCHIVE=0)
else()
    target_compile_definitions(shimejifinder PUBLIC SHIMEJIFINDER_NO_LIBARCHIVE=1)
endif()
