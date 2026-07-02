# orc-plugin-sdk: Public stable plugin SDK for decode-orc stage plugins.
#
# This INTERFACE library provides:
#   - The public SDK include directory only: <orc/plugin/...> (ABI/services)
#     and <orc/stage/...> (stage contract headers). Private host include
#     trees (orc/core, orc/common, orc/view-types) are deliberately NOT
#     propagated: a plugin TU that includes a private host header must fail
#     to compile, matching the allowlist enforced by
#     cmake/check_plugin_private_includes.sh.
#   - The orc-core link target (link-only) so plugins can resolve DAGStage,
#     ArtifactPtr, and the other contract symbols at link time.
#   - spdlog/fmt usage requirements for the SDK logging surface
#     (<orc/stage/logging.h>, <orc/plugin/orc_plugin_services_helpers.h>).
#
# In-tree usage (plugins built inside this repository):
#   target_link_libraries(my-plugin PRIVATE orc-plugin-sdk)
#
# Out-of-tree usage (third-party plugins):
#   find_package(decode-orc-plugin-sdk REQUIRED)
#   target_link_libraries(my-plugin PRIVATE orc::plugin-sdk)
#   orc_add_stage_plugin(...)   # helper macro from DecodeOrcPluginSDKHelpers.cmake

add_library(orc-plugin-sdk INTERFACE)

target_include_directories(orc-plugin-sdk INTERFACE
    # SDK headers: <orc/plugin/...> and <orc/stage/...>.
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include/decode-orc-plugin-sdk>
)

# Plugins link orc-core for symbols only. $<LINK_ONLY:...> strips orc-core's
# usage requirements (include directories, compile definitions), so private
# host headers are not reachable from plugin TUs; third-party dependencies a
# plugin needs beyond the SDK surface must be declared by the plugin's own
# CMakeLists.txt.
target_link_libraries(orc-plugin-sdk INTERFACE $<LINK_ONLY:orc-core>)

# The SDK headers themselves use spdlog (<orc/stage/logging.h>) and fmt
# (<orc/plugin/orc_plugin_services_helpers.h>), so their usage requirements
# are part of the SDK contract.
target_link_libraries(orc-plugin-sdk INTERFACE spdlog::spdlog fmt::fmt)

# =============================================================================
# Install rules
# =============================================================================

# SDK public headers
install(DIRECTORY include/
    DESTINATION include/decode-orc-plugin-sdk
    FILES_MATCHING PATTERN "*.h"
)

# CMake package config files (see cmake/decode-orc-plugin-sdk/)
# Note: full CMake package export (find_package support) requires orc-core to
# also be installed and exported. The Config.cmake and Helpers.cmake are
# installed here; the Targets.cmake is generated when orc-core is packaged.
# Full out-of-tree package support is completed in Phase 7.
install(FILES
    ${CMAKE_SOURCE_DIR}/cmake/decode-orc-plugin-sdk/DecodeOrcPluginSDKConfig.cmake
    ${CMAKE_SOURCE_DIR}/cmake/decode-orc-plugin-sdk/DecodeOrcPluginSDKHelpers.cmake
    DESTINATION lib/cmake/decode-orc-plugin-sdk
)

# SDK enforcement gate scripts — installed alongside the CMake package so that
# third-party plugin authors can run the same private-include and private-link
# scans locally without needing access to the host source tree.
install(FILES
    ${CMAKE_SOURCE_DIR}/cmake/check_plugin_private_includes.sh
    ${CMAKE_SOURCE_DIR}/cmake/check_plugin_private_links.sh
    DESTINATION lib/cmake/decode-orc-plugin-sdk
    PERMISSIONS OWNER_READ OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
)
