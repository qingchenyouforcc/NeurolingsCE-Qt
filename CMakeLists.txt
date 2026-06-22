cmake_minimum_required(VERSION 3.21)

include(cmake/Version.cmake)
load_version(${CMAKE_SOURCE_DIR}/VERSION.txt)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/VERSION.txt")

project(NeurolingsCE VERSION ${PROJECT_VERSION} LANGUAGES C CXX)

enable_testing()

if(MSVC)
  # This project expects a 64-bit Qt when using Qt's msvc2022_64 kit.
  # If you configure with Ninja from a Win32 (x86) environment, Qt6Config will be rejected.
  if(CMAKE_SIZEOF_VOID_P EQUAL 4)
    message(FATAL_ERROR "Detected 32-bit MSVC toolchain. Please configure/build with the x64 toolchain (Platform=x64 in Visual Studio) to match your Qt kit (e.g. msvc2022_64).")
  endif()
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

if(NOT DEFINED CMAKE_PREFIX_PATH AND DEFINED ENV{QTDIR})
  # Allow Qt to be provided via environment (common with Qt installs / dev shells).
  set(CMAKE_PREFIX_PATH "$ENV{QTDIR}" CACHE PATH "Qt install prefix" FORCE)
endif()

option(SHIJIMA_USE_QTMULTIMEDIA "Enable Qt Multimedia support" ON)

find_package(Qt6 QUIET COMPONENTS Core Gui Widgets Concurrent Network LinguistTools)
if(NOT Qt6_FOUND)
  message(FATAL_ERROR
    "Qt6 not found. Set one of the following and reconfigure:\n"
    "  - Qt6_DIR=D:/Qt/6.8.2/mingw_64/lib/cmake/Qt6          (MinGW)\n"
    "  - Qt6_DIR=D:/Qt/6.8.2/msvc2022_64/lib/cmake/Qt6       (MSVC)\n"
    "  - CMAKE_PREFIX_PATH=D:/Qt/6.8.2/mingw_64              (MinGW)\n"
    "  - CMAKE_PREFIX_PATH=D:/Qt/6.8.2/msvc2022_64           (MSVC)\n"
    "  - or set environment variable QTDIR to your Qt prefix\n"
    "Note: When using MinGW, use the compiler bundled with Qt\n"
    "  (e.g. D:/Qt/Tools/mingw1310_64/bin/g++.exe) to avoid GCC runtime mismatches.\n"
  )
endif()
message(STATUS "Using Qt6 from: ${Qt6_DIR}")
message(STATUS "Qt6 version: ${Qt6_VERSION}")

if(NOT TARGET Qt6::WidgetsPrivate)
  # Modern Qt installs (Homebrew, official online installer, MSYS2, ...) ship a
  # standalone Qt6WidgetsPrivate config. Pull it in via find_package before
  # falling back to the manual interface target — the fallback path can pick
  # the wrong include dir on multi-framework macOS layouts.
  find_package(Qt6 QUIET OPTIONAL_COMPONENTS WidgetsPrivate)
endif()

if(NOT TARGET Qt6::WidgetsPrivate)  # hack for ElaWidgetTools/ElaWidgetTools/CmakeLists.txt
  message(STATUS "Qt6WidgetsPrivate not provided by Qt SDK, creating fallback target")
  add_library(Qt6::WidgetsPrivate INTERFACE IMPORTED)
  target_link_libraries(Qt6::WidgetsPrivate
    INTERFACE
    Qt6::Widgets
  )
  target_include_directories(Qt6::WidgetsPrivate
    INTERFACE
    ${Qt6Widgets_INCLUDE_DIRS}
    ${Qt6Widgets_INCLUDE_DIRS}/QtWidgets/private
  )
endif()

function(_shijima_print_qt_target_locations tgt)
  if(NOT TARGET ${tgt})
    return()
  endif()

  foreach(cfg DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
    get_target_property(_loc ${tgt} IMPORTED_LOCATION_${cfg})
    if(_loc)
      message(STATUS "${tgt} ${cfg}: ${_loc}")
    endif()
  endforeach()
endfunction()

_shijima_print_qt_target_locations(Qt6::Core)
_shijima_print_qt_target_locations(Qt6::Concurrent)

# Conda-provided Qt packages are typically release-only on Windows (no *d.lib).
# In that case, Debug builds must use the release CRT (/MD) to avoid CRT conflicts.
set(_shijima_msvc_runtime "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
if(MSVC)
  string(TOLOWER "${Qt6_DIR}" _qt6_dir_lower)
  if(_qt6_dir_lower MATCHES "miniconda|anaconda|conda")
    set(_shijima_msvc_runtime "MultiThreadedDLL")
    message(WARNING "Detected conda Qt6; forcing /MD runtime for all configs to avoid Debug CRT mismatches.")
  endif()
endif()
if(SHIJIMA_USE_QTMULTIMEDIA)
  find_package(Qt6 COMPONENTS Multimedia QUIET)
  if(NOT TARGET Qt6::Multimedia)
    message(WARNING "Qt6 Multimedia not found; building without sound support (SHIJIMA_USE_QTMULTIMEDIA=OFF).")
    set(SHIJIMA_USE_QTMULTIMEDIA OFF CACHE BOOL "Enable Qt Multimedia support" FORCE)
  endif()
endif()

if(UNIX AND NOT APPLE)
  # Linux platform layer uses Qt DBus for GNOME/KDE window tracking.
  find_package(Qt6 COMPONENTS DBus REQUIRED)
  _shijima_print_qt_target_locations(Qt6::DBus)
endif()

# Integrated shijima engine sources live under src/app/core/shijima-engine.
file(GLOB_RECURSE NEUROLINGS_SHIJIMA_ENGINE_SOURCES CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/src/app/core/shijima-engine/shijima/*.cc"
)

add_library(NeurolingsShijimaEngine STATIC
  ${NEUROLINGS_SHIJIMA_ENGINE_SOURCES}
)
target_include_directories(NeurolingsShijimaEngine PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/src/app/core/shijima-engine
)
target_compile_definitions(NeurolingsShijimaEngine PUBLIC SHIJIMA_DUK_STATIC_BUILD=1)
target_compile_definitions(NeurolingsShijimaEngine PUBLIC SHIJIMA_LOGGING_ENABLED=1)

if(MSVC)
  set_property(TARGET NeurolingsShijimaEngine PROPERTY MSVC_RUNTIME_LIBRARY "${_shijima_msvc_runtime}")
  # Handle UTF-8 source files in the integrated shijima engine.
  target_compile_options(NeurolingsShijimaEngine PRIVATE /utf-8)
endif()

# ElaWidgetTools (Fluent Design UI components)
# We add_subdirectory the inner library directly (ElaWidgetTools/ElaWidgetTools/)
# to skip the top-level CMake which has a Qt version gate and hardcoded QT_SDK_DIR.
# ElaWidgetTools expects QT_VERSION_MAJOR to be set (normally done by find_package(QT NAMES ...)).
if(NOT DEFINED QT_VERSION_MAJOR)
  set(QT_VERSION_MAJOR ${Qt6_VERSION_MAJOR})
endif()
# Build as static lib to avoid DLL deployment complexity
set(ELAWIDGETTOOLS_BUILD_STATIC_LIB ON CACHE BOOL "Build ElaWidgetTools as static library" FORCE)
add_subdirectory(ElaWidgetTools/ElaWidgetTools)
if(MSVC AND TARGET ElaWidgetTools)
  set_property(TARGET ElaWidgetTools PROPERTY MSVC_RUNTIME_LIBRARY "${_shijima_msvc_runtime}")
endif()
target_compile_options(ElaWidgetTools PRIVATE
  $<$<CXX_COMPILER_ID:GNU,Clang>:-w>
  $<$<CXX_COMPILER_ID:MSVC>:/W0>
)

# libshimejifinder's upstream CMakeLists.txt relies on a bash script (bin2cpp.sh)
# which cannot run on Windows natively. This project instead routes through
# cmake/libshimejifinder/CMakeLists.txt, which replaces the script with a pure
# CMake implementation (cmake/bin2cpp.cmake) and targets archive_static rather
# than the shared-only 'archive' target, making it fully cross-platform.
# NOTE: When building on Windows with MinGW, use the compiler bundled with Qt
# (e.g. D:/Qt/Tools/mingw1310_64) to avoid GCC runtime version mismatches.
set(LIBSHIMEJIFINDER_SOURCE_DIR ${CMAKE_SOURCE_DIR}/libshimejifinder)
set(LIBSHIMEJIFINDER_BIN_DIR ${CMAKE_CURRENT_BINARY_DIR}/libshimejifinder)
set(BIN2CPP_CMAKE_SCRIPT ${CMAKE_SOURCE_DIR}/cmake/bin2cpp.cmake)
set(SHIMEJIFINDER_USE_LIBARCHIVE OFF)
set(SHIMEJIFINDER_USE_LIBUNARR ON)
add_subdirectory(${CMAKE_SOURCE_DIR}/cmake/libshimejifinder)

option(SHIJIMA_WITH_DEFAULT_MASCOT "Embed DefaultMascot assets" ON)
option(SHIJIMA_WITH_LICENSES_TEXT "Embed licenses text" ON)

set(_shijima_generated_dir "${CMAKE_CURRENT_BINARY_DIR}")
set(_shijima_licenses_header "${_shijima_generated_dir}/licenses_generated.hpp")

add_executable(NeurolingsCE)
add_executable(NeurolingsCECli)

if(MSVC)
  set_property(TARGET NeurolingsCE PROPERTY MSVC_RUNTIME_LIBRARY "${_shijima_msvc_runtime}")
  set_property(TARGET NeurolingsCECli PROPERTY MSVC_RUNTIME_LIBRARY "${_shijima_msvc_runtime}")
endif()

target_sources(NeurolingsCE PRIVATE
  src/app/main.cc
  src/app/core/AppLog.cc
  src/app/core/assets/Asset.cc
  src/app/core/assets/MascotData.cc
  src/app/core/assets/MascotPackage.cc
  src/app/core/assets/SafePath.cc
  src/app/core/assets/AssetLoader.cc
  src/app/core/commands/MascotApi.cc
  src/app/core/commands/MascotCommandService.cc
  src/app/core/localipc/ShijimaLocalApiClient.cc
  src/app/core/localipc/ShijimaLocalApi.cc
  src/app/core/update/GitHubUpdateManager.cc
  src/app/ui/menus/ShijimaContextMenu.cc
  src/app/ui/menus/ContextMenuActions.cc
  src/app/core/audio/SoundEffectManager.cc
  src/app/core/http/ShijimaHttpApi.cc
  src/app/runtime/ManagerEnvironmentSync.cc
  src/app/runtime/ManagerEnvironmentController.cc
  src/app/runtime/ManagerImportWorkflow.cc
  src/app/runtime/ManagerLifecycle.cc
  src/app/runtime/ManagerMascotRuntime.cc
  src/app/runtime/MascotSessionStore.cc
  src/app/runtime/MascotTemplateStore.cc
  src/app/ui/mascot/MascotWidgetInteraction.cc
  src/app/ui/mascot/MascotWidgetLifecycle.cc
  src/app/ui/mascot/MascotWidgetRendering.cc
  src/app/ui/ManagerWindowSetup.cc
  src/app/ui/ManagerUiActions.cc
  src/app/ui/dialogs/common/ForcedProgressDialog.cc
  src/app/ui/dialogs/inspector/ShimejiInspectorDialog.cc
  src/app/ui/dialogs/inspector/ShimejiInspectorRows.cc
  src/app/ui/dialogs/licenses/ShijimaLicensesDialog.cc
  src/app/ui/ManagerTrayIcon.cc
  src/app/ui/interface/ManagerAboutSection.cc
  src/app/ui/interface/ManagerCreatePage.cc
  src/app/ui/interface/ManagerHomePage.cc
  src/app/ui/interface/ManagerNavigation.cc
  src/app/ui/interface/ManagerSettingsPage.cc
  src/app/cli.cc
  src/app/cli/CommandLineParser.cc
  src/app/cli/CommandExecutor.cc
  src/app/cli/OutputFormatter.cc
  src/app/ui/widgets/SpeechBubbleWidget.cc
  src/app/ui/widgets/SpeechBubbleTextCatalog.cc
  src/resources/resources.qrc

  # Headers with Q_OBJECT (needed for AUTOMOC)
  include/shijima-qt/ShijimaManager.hpp
  include/shijima-qt/ui/mascot/ShijimaWidget.hpp
  include/shijima-qt/ui/menus/ShijimaContextMenu.hpp
  include/shijima-qt/ui/dialogs/licenses/ShijimaLicensesDialog.hpp
  include/shijima-qt/ui/dialogs/inspector/ShimejiInspectorDialog.hpp
  src/app/core/update/GitHubUpdateManager.hpp

  include/shijima-qt/ui/widgets/SpeechBubbleWidget.hpp
  src/platform/Platform/Platform.hpp
  src/platform/Platform/ActiveWindow.hpp
  src/platform/Platform/ActiveWindowObserver.hpp
)

target_sources(NeurolingsCECli PRIVATE
  src/app/cli_main.cc
  src/app/cli.cc
  src/app/cli/CommandLineParser.cc
  src/app/cli/CommandExecutor.cc
  src/app/cli/OutputFormatter.cc
  src/app/core/AppLog.cc
  src/app/core/assets/MascotPackage.cc
  src/app/core/assets/SafePath.cc
  src/app/core/commands/MascotApi.cc
  src/app/core/localipc/ShijimaLocalApiClient.cc
)

if(SHIJIMA_WITH_DEFAULT_MASCOT)
  set(_default_mascot_files
    "${CMAKE_CURRENT_SOURCE_DIR}/src/assets/DefaultMascot/info.json"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/assets/DefaultMascot/behaviors.xml"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/assets/DefaultMascot/actions.xml"
  )
  foreach(i RANGE 1 46)
    list(APPEND _default_mascot_files "${CMAKE_CURRENT_SOURCE_DIR}/src/assets/DefaultMascot/img/shime${i}.png")
  endforeach()

  add_custom_command(
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/DefaultMascot.cc"
    COMMAND ${CMAKE_COMMAND}
      -DOUT:FILEPATH=${CMAKE_CURRENT_BINARY_DIR}/DefaultMascot.cc
      -DSHIJIMA_DEFAULT_MASCOT_DIR:PATH=${CMAKE_CURRENT_SOURCE_DIR}/src/assets/DefaultMascot
      -DSHIJIMA_DEFAULT_MASCOT_IMAGE_COUNT:STRING=46
      -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/BundleDefaultMascot.cmake"
    DEPENDS
      "${CMAKE_CURRENT_SOURCE_DIR}/cmake/BundleDefaultMascot.cmake"
      ${_default_mascot_files}
    VERBATIM
    COMMENT "Generating DefaultMascot.cc from bundled mascot assets"
  )

  set_source_files_properties("${CMAKE_CURRENT_BINARY_DIR}/DefaultMascot.cc" PROPERTIES GENERATED TRUE)
  target_sources(NeurolingsCE PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/DefaultMascot.cc")
  target_include_directories(NeurolingsCE PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
else()
  message(FATAL_ERROR "SHIJIMA_WITH_DEFAULT_MASCOT=OFF is not supported yet (DefaultMascot.cc is required).")
endif()

if(SHIJIMA_WITH_LICENSES_TEXT)
  add_custom_command(
    OUTPUT "${_shijima_licenses_header}"
    COMMAND ${CMAKE_COMMAND}
      -DOUT:FILEPATH=${_shijima_licenses_header}
      -DSHIJIMA_SOURCE_DIR:PATH=${CMAKE_CURRENT_SOURCE_DIR}
      -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/GenerateLicenses.cmake"
    DEPENDS
      ${CMAKE_CURRENT_SOURCE_DIR}/cmake/GenerateLicenses.cmake
      ${CMAKE_CURRENT_SOURCE_DIR}/licenses/Shijima-Qt.LICENSE.txt
      ${CMAKE_CURRENT_SOURCE_DIR}/licenses/duktape.LICENSE.txt
      ${CMAKE_CURRENT_SOURCE_DIR}/licenses/duktape.AUTHORS.rst
      ${CMAKE_CURRENT_SOURCE_DIR}/licenses/libarchive.LICENSE.txt
      ${CMAKE_CURRENT_SOURCE_DIR}/licenses/libshijima.LICENSE.txt
      ${CMAKE_CURRENT_SOURCE_DIR}/licenses/libshimejifinder.LICENSE.txt
      ${CMAKE_CURRENT_SOURCE_DIR}/licenses/unarr.LICENSE.txt
      ${CMAKE_CURRENT_SOURCE_DIR}/licenses/unarr.AUTHORS.txt
      ${CMAKE_CURRENT_SOURCE_DIR}/licenses/Qt.LICENSE.txt
      ${CMAKE_CURRENT_SOURCE_DIR}/licenses/rapidxml.LICENSE.txt
    VERBATIM
    COMMENT "Generating licenses_generated.hpp from license files"
  )

  set_source_files_properties("${_shijima_licenses_header}" PROPERTIES GENERATED TRUE)
  target_include_directories(NeurolingsCE PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

  # Create a custom target to ensure licenses are generated before autogen runs
  add_custom_target(shijima_licenses_generated DEPENDS "${_shijima_licenses_header}")
  add_dependencies(NeurolingsCE shijima_licenses_generated)
else()
  message(FATAL_ERROR "SHIJIMA_WITH_LICENSES_TEXT=OFF is not supported yet (licenses_generated.hpp is required).")
endif()

if(WIN32)
  target_sources(NeurolingsCE PRIVATE
    src/resources/resources.rc
    src/platform/Platform/Windows/Platform.cc
    src/platform/Platform/Windows/ActiveWindowObserver.cc
    src/platform/Platform/Windows/PrivateActiveWindowObserver.cc
    src/platform/Platform/Windows/PrivateActiveWindowObserver.hpp
  )
elseif(APPLE)
  target_sources(NeurolingsCE PRIVATE
    src/platform/Platform/macOS/Platform.mm
    src/platform/Platform/macOS/ActiveWindowObserver.cc
    src/platform/Platform/macOS/PrivateActiveWindowObserver.mm
    src/platform/Platform/macOS/PrivateActiveWindowObserver.hpp
  )

  # macOS .app bundle target (cmake --build build --target macapp)
  set(MACAPP_TARGET        NeurolingsCE)
  set(MACAPP_BUNDLE_NAME   "${APP_BUNDLE_NAME}")
  set(MACAPP_BUNDLE_EXEC   "${APP_EXECUTABLE}")
  set(MACAPP_SKELETON_DIR  "${CMAKE_CURRENT_SOURCE_DIR}/src/packaging/NeurolingsCE.app")
  set(MACAPP_OUTPUT_DIR    "${CMAKE_CURRENT_BINARY_DIR}/macos")
  set(MACAPP_CLI_TARGET    NeurolingsCECli)
  include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/MacApp.cmake)
elseif(UNIX)
  set(LINUX_TARGET NeurolingsCE)
  set(LINUX_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR})
  set(LINUX_BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR})
  set(BIN2CPP_CMAKE_SCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/cmake/bin2cpp.cmake)
  include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/LinuxPlatform.cmake)

  # AppImage packaging target (cmake appimage)
  set(APPIMAGE_TARGET      NeurolingsCE)
  set(APPIMAGE_SOURCE_DIR  ${CMAKE_CURRENT_SOURCE_DIR})
  set(APPIMAGE_BINARY_DIR  ${CMAKE_CURRENT_BINARY_DIR})
  if(NOT CMAKE_BUILD_TYPE STREQUAL "Release")
    set(APPIMAGE_NO_STRIP "1")
  endif()
  include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/AppImage.cmake)
endif()

target_include_directories(NeurolingsCE PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_SOURCE_DIR}/src/platform
  ${CMAKE_CURRENT_SOURCE_DIR}/libshimejifinder
  ${CMAKE_CURRENT_SOURCE_DIR}/cpp-httplib
)

target_include_directories(NeurolingsCECli PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_SOURCE_DIR}/libshimejifinder
)

target_compile_definitions(NeurolingsCE PRIVATE
  NEUROLINGSCE_VERSION="${PROJECT_VERSION}"
  APP_NAME="NeurolingsCE"
  APP_DISPLAY_NAME="NeurolingsCE[Shijima-Qt Edition]"
  $<$<BOOL:${SHIJIMA_USE_QTMULTIMEDIA}>:SHIJIMA_USE_QTMULTIMEDIA=1>
  $<$<NOT:$<BOOL:${SHIJIMA_USE_QTMULTIMEDIA}>>:SHIJIMA_USE_QTMULTIMEDIA=0>
)
target_compile_definitions(NeurolingsCECli PRIVATE
  NEUROLINGSCE_VERSION="${PROJECT_VERSION}"
  APP_NAME="NeurolingsCE"
  APP_DISPLAY_NAME="NeurolingsCE[Shijima-Qt Edition]"
)
target_link_libraries(NeurolingsCE PRIVATE
  Qt6::Core
  Qt6::Gui
  Qt6::Widgets
  Qt6::Concurrent
  Qt6::Network
  NeurolingsShijimaEngine
  ElaWidgetTools
)
target_link_libraries(NeurolingsCECli PRIVATE
  Qt6::Core
  Qt6::Network
  NeurolingsShijimaEngine
  shimejifinder
)


# i18n translations
if(TARGET Qt6::lrelease)
  # Only compile .ts -> .qm and embed; do NOT run lupdate (which would
  # overwrite our hand-maintained .ts with unfinished entries).
  qt_add_lrelease(
    TS_FILES translations/shijima-qt_zh_CN.ts
    QM_FILES_OUTPUT_VARIABLE QM_FILES
  )
  qt_add_resources(NeurolingsCE "translations"
    PREFIX "/i18n"
    BASE "${CMAKE_CURRENT_BINARY_DIR}"
    FILES ${QM_FILES}
  )
  message(STATUS "Qt LinguistTools found; translations will be compiled and embedded.")
else()
  message(WARNING "Qt6 LinguistTools not found; translations will NOT be compiled. Install qt6-tools or equivalent.")
endif()

if(MSVC)
  # Make running from Visual Studio work without manually editing PATH.
  # This points PATH at the Qt bin directory (where Qt6Guid.dll lives in Debug).
  set_property(TARGET NeurolingsCE PROPERTY VS_DEBUGGER_ENVIRONMENT
    "PATH=$<TARGET_FILE_DIR:Qt6::Core>\;%PATH%")
  set_property(TARGET NeurolingsCECli PROPERTY VS_DEBUGGER_ENVIRONMENT
    "PATH=$<TARGET_FILE_DIR:Qt6::Core>\;%PATH%")
endif()

target_compile_definitions(NeurolingsCE PRIVATE ELAWIDGETTOOLS_LIBRARY=0)

target_link_libraries(NeurolingsCE PRIVATE shimejifinder)

set_target_properties(NeurolingsCECli PROPERTIES OUTPUT_NAME "NeurolingsCE-cli")

if(SHIJIMA_USE_QTMULTIMEDIA)
  target_link_libraries(NeurolingsCE PRIVATE Qt6::Multimedia)
endif()

add_executable(NeurolingsCETests
  src/app/tests/AppCoreTests.cc
  src/app/core/AppLog.cc
  src/app/core/assets/MascotPackage.cc
  src/app/core/assets/SafePath.cc
  src/app/core/commands/MascotApi.cc
)
target_include_directories(NeurolingsCETests PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_SOURCE_DIR}/src/app
  ${CMAKE_CURRENT_SOURCE_DIR}/libshimejifinder
)
target_compile_definitions(NeurolingsCETests PRIVATE
  NEUROLINGSCE_VERSION="${PROJECT_VERSION}"
  APP_NAME="NeurolingsCE"
  APP_DISPLAY_NAME="NeurolingsCE[Shijima-Qt Edition]"
)
target_link_libraries(NeurolingsCETests PRIVATE
  Qt6::Core
  Qt6::Gui
  NeurolingsShijimaEngine
  shimejifinder
)
if(MSVC)
  set_property(TARGET NeurolingsCETests PROPERTY MSVC_RUNTIME_LIBRARY "${_shijima_msvc_runtime}")
endif()
if(WIN32)
  target_compile_definitions(NeurolingsCETests PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
endif()
add_test(NAME NeurolingsCETests COMMAND NeurolingsCETests)

if(WIN32)
  target_compile_definitions(NeurolingsCE PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
  target_compile_definitions(NeurolingsCECli PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
  set_target_properties(NeurolingsCE PROPERTIES WIN32_EXECUTABLE TRUE)
endif()

# On Windows, use windeployqt to deploy the Qt runtime in release builds.
if(WIN32)
  get_target_property(_qt_core_location Qt6::Core IMPORTED_LOCATION_$<UPPER_CASE:$<CONFIG>>)
  if(NOT _qt_core_location)
    get_target_property(_qt_core_location Qt6::Core IMPORTED_LOCATION_RELEASE)
  endif()
  if(NOT _qt_core_location)
    get_target_property(_qt_core_location Qt6::Core IMPORTED_LOCATION)
  endif()

  if(_qt_core_location)
    get_filename_component(_qt_bin_dir "${_qt_core_location}" DIRECTORY)
    find_program(QT_WINDEPLOYQT_EXECUTABLE windeployqt HINTS "${_qt_bin_dir}")
  else()
    find_program(QT_WINDEPLOYQT_EXECUTABLE windeployqt)
  endif()

  if(QT_WINDEPLOYQT_EXECUTABLE)
    add_custom_command(TARGET NeurolingsCE POST_BUILD
      COMMAND "${QT_WINDEPLOYQT_EXECUTABLE}"
              --no-translations
              --no-system-d3d-compiler
              "$<TARGET_FILE:NeurolingsCE>"
      WORKING_DIRECTORY "$<TARGET_FILE_DIR:NeurolingsCE>"
      VERBATIM
    )
    add_custom_command(TARGET NeurolingsCECli POST_BUILD
      COMMAND "${QT_WINDEPLOYQT_EXECUTABLE}"
              --no-translations
              --no-system-d3d-compiler
              "$<TARGET_FILE:NeurolingsCECli>"
      WORKING_DIRECTORY "$<TARGET_FILE_DIR:NeurolingsCECli>"
      VERBATIM
    )
    message(STATUS "windeployqt found; adding post-build command to deploy Qt runtime.")
  else()
    message(WARNING "windeployqt not found; NeurolingsCE.exe may fail to start outside of a Qt environment (missing Qt6Guid.dll).")
  endif()
endif()

# On Release builds, strip the binary to reduce size.
if(NOT MSVC)
  if(CMAKE_BUILD_TYPE STREQUAL "Release")
    message(STATUS "Release build: adding post-build command to strip binary")
    add_custom_command(
        TARGET NeurolingsCE POST_BUILD
        COMMAND ${CMAKE_STRIP} $<TARGET_FILE:NeurolingsCE>
        COMMENT "Stripping binary"
    )
  else()
    message(STATUS "Non-release build: skipping binary stripping")
  endif()
endif()

function(define_assets_target TARGET_NAME TARGET)
  set(_input_file "${CMAKE_CURRENT_SOURCE_DIR}/src/${TARGET}.in")
  set(_target_file "${CMAKE_CURRENT_SOURCE_DIR}/src/${TARGET}")
  configure_file(${_input_file} ${_target_file} @ONLY)
  add_custom_target(${TARGET_NAME} ALL DEPENDS ${_target_file})
  add_dependencies(NeurolingsCE ${TARGET_NAME})
endfunction()

# Generate metainfo.xml from template with version substitution
define_assets_target(METAINFO_XML "packaging/io.github.qingchenyouforcc.NeurolingsCE.metainfo.xml")

# Generate resources.rc from template (Windows)
define_assets_target(RESOURCES_RC "resources/resources.rc")

# Generate Info.plist from template (macOS)
define_assets_target(INFO_PLIST "packaging/NeurolingsCE.app/Contents/Info.plist")

# Generate GNOME extension metadata.json from template (Linux)
define_assets_target(GNOME_METADATA "platform/Platform/Linux/gnome_script/metadata.json")
