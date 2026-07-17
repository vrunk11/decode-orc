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

# Exported as orc::plugin-sdk from the installed package (the in-tree target
# name keeps the orc- prefix; EXPORT_NAME controls only the imported name).
set_target_properties(orc-plugin-sdk PROPERTIES EXPORT_NAME plugin-sdk)

target_include_directories(orc-plugin-sdk INTERFACE
    # SDK headers, tiered: <orc/abi/...> (frozen binary contract),
    # <orc/stage/...> (stage contract, grouped by domain), and
    # <orc/support/...> (compiled-into-plugin utilities).
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include/decode-orc-plugin-sdk>
)

# Deprecated pre-tier include-path shims (flat <orc/plugin/...> and
# <orc/stage/...> paths that forward to the tiered layout) are retained for one
# release so existing third-party plugin source keeps building. Turning this
# OFF makes any plugin TU that still includes a pre-tier path fail to compile
# (the shim headers #error on ORC_SDK_NO_DEPRECATED_INCLUDE_SHIMS). Bundled
# plugins must build with it OFF — CI exercises that configuration. The define
# is a usage requirement of orc-plugin-sdk, so it reaches every plugin/test
# target that links the SDK but not the host (which does not link the SDK).
option(ORC_SDK_DEPRECATED_INCLUDE_SHIMS
    "Keep deprecated pre-tier SDK include-path shims for third-party source compatibility"
    ON)
if(NOT ORC_SDK_DEPRECATED_INCLUDE_SHIMS)
    target_compile_definitions(orc-plugin-sdk INTERFACE
        ORC_SDK_NO_DEPRECATED_INCLUDE_SHIMS)
endif()

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

# Exported targets for out-of-tree plugin builds. The export set collects
# orc-plugin-sdk (this file), orc-core (orc/core/CMakeLists.txt), and
# orc-common (orc/common/CMakeLists.txt); the generated Targets file defines
# orc::plugin-sdk plus the orc::orc-core / orc::orc-common link targets it
# references.
install(TARGETS orc-plugin-sdk EXPORT decode-orc-plugin-sdkTargets)
install(EXPORT decode-orc-plugin-sdkTargets
    FILE decode-orc-plugin-sdkTargets.cmake
    NAMESPACE orc::
    DESTINATION lib/cmake/decode-orc-plugin-sdk
)

# CMake package config files (see cmake/decode-orc-plugin-sdk/). The Config
# file is installed under the exact name find_package(decode-orc-plugin-sdk)
# looks for on case-sensitive filesystems: <package-name>Config.cmake.
install(FILES
    ${CMAKE_SOURCE_DIR}/cmake/decode-orc-plugin-sdk/DecodeOrcPluginSDKConfig.cmake
    DESTINATION lib/cmake/decode-orc-plugin-sdk
    RENAME decode-orc-plugin-sdkConfig.cmake
)
install(FILES
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

# The generated SDK header allowlist consumed by check_plugin_private_includes.sh.
# Installed next to the script so the standalone (out-of-tree) scan resolves it
# via its own directory without a decode-orc checkout.
install(FILES
    ${CMAKE_SOURCE_DIR}/cmake/sdk_header_allowlist.txt
    DESTINATION lib/cmake/decode-orc-plugin-sdk
)
