# orc-common: Shared type definitions accessible to all layers
#
# This library contains shared types and utility functions that are
# used across core, public API, presenters, GUI, and CLI.

add_library(orc-common
    parameter_util.cpp
    node_id.cpp
    logging.cpp
    crash_handler.cpp
)

# PUBLIC interface - these directories are visible to all consumers
target_include_directories(orc-common PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include/orc/common>
    # Stage contract headers (<orc/stage/...>): shared vocabulary types
    # (common_types.h, node_type.h, parameter_types.h, ...) are re-homed in
    # the plugin SDK contract tree and consumed by every layer, so the SDK
    # include directory is published from here (headers only; no link
    # dependency on the SDK or core).
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/orc/sdk/include>
    $<INSTALL_INTERFACE:include/decode-orc-plugin-sdk>
)

# Find and link spdlog (which includes fmt)
# Note: spdlog bundles fmt, so we get both libraries
find_package(spdlog REQUIRED)
target_link_libraries(orc-common PUBLIC spdlog::spdlog)

if(WIN32)
    target_link_libraries(orc-common PRIVATE dbghelp)
endif()

# Install headers
install(DIRECTORY include/
    DESTINATION include/orc/common
    FILES_MATCHING PATTERN "*.h"
)

# orc-common is a host runtime library, installed for the app bundle. It is no
# longer part of the decode-orc-plugin-sdk export set: plugins link the SDK
# alone (orc::plugin-sdk + orc::orc-sdk-support), not the host.
install(TARGETS orc-common
    ARCHIVE DESTINATION lib COMPONENT Runtime
    LIBRARY DESTINATION lib COMPONENT Runtime
    RUNTIME DESTINATION bin COMPONENT Runtime
)
