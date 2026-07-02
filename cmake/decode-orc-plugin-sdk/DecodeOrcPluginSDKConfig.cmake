# DecodeOrcPluginSDKConfig.cmake
#
# CMake package configuration for the decode-orc Plugin SDK.
# Installed as lib/cmake/decode-orc-plugin-sdk/decode-orc-plugin-sdkConfig.cmake
# (the name find_package(decode-orc-plugin-sdk) resolves on case-sensitive
# filesystems); the repository copy keeps this descriptive filename.
#
# After find_package(decode-orc-plugin-sdk REQUIRED), the following are
# available:
#
#   orc::plugin-sdk   — INTERFACE imported target. Link your plugin against
#                       this target to get the SDK include directories
#                       (<orc/plugin/...>, <orc/stage/...>), spdlog/fmt, and
#                       the orc-core link-only dependency.
#
#   orc_add_stage_plugin()  — CMake helper macro (from DecodeOrcPluginSDKHelpers.cmake)
#                             for creating properly configured plugin targets.
#
# Example (external plugin CMakeLists.txt):
#
#   cmake_minimum_required(VERSION 3.20)
#   project(my-orc-plugin CXX)
#
#   find_package(decode-orc-plugin-sdk REQUIRED)
#
#   orc_add_stage_plugin(
#       my-stage-plugin
#       OUTPUT_NAME  my-stage-plugin
#       PLUGIN_VERSION "1.0.0"
#       SOURCES plugin.cpp my_stage.cpp
#   )

cmake_minimum_required(VERSION 3.20)

include(CMakeFindDependencyMacro)

# Dependencies referenced by the exported targets' link interfaces:
#   orc::plugin-sdk  → spdlog, fmt (SDK logging surface)
#   orc::orc-core    → spdlog, fmt, SQLite3, yaml-cpp, PNG, FFmpeg
#   orc::orc-common  → spdlog
# These must all resolve before the Targets file is included, otherwise the
# generated import file fails with "missing referenced targets".
find_dependency(spdlog REQUIRED)
find_dependency(fmt REQUIRED)
find_dependency(SQLite3 REQUIRED)
find_dependency(yaml-cpp REQUIRED)
find_dependency(PNG REQUIRED)

# Some yaml-cpp packages export only the legacy `yaml-cpp` target; the export
# references the namespaced `yaml-cpp::yaml-cpp` (same shim as the host build).
if(TARGET yaml-cpp AND NOT TARGET yaml-cpp::yaml-cpp)
    set_target_properties(yaml-cpp PROPERTIES IMPORTED_GLOBAL TRUE)
    add_library(yaml-cpp::yaml-cpp ALIAS yaml-cpp)
endif()

# FFmpeg has no upstream CMake config; recreate the PkgConfig::FFMPEG imported
# target the export references.
find_dependency(PkgConfig REQUIRED)
pkg_check_modules(FFMPEG REQUIRED IMPORTED_TARGET GLOBAL
    libavcodec
    libavformat
    libavutil
    libswscale
)

# Import orc::plugin-sdk and its link dependencies (orc::orc-core,
# orc::orc-common).
include("${CMAKE_CURRENT_LIST_DIR}/decode-orc-plugin-sdkTargets.cmake")

# Load the orc_add_stage_plugin() helper macro
include("${CMAKE_CURRENT_LIST_DIR}/DecodeOrcPluginSDKHelpers.cmake")
