# CVBS Source Stage Plugin
#
# Loads composite CVBS (Composite Video Baseband Signal) sources for PAL and NTSC
#
# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2026 Simon Inns

file(GLOB_RECURSE STAGE_PLUGIN_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
)

# Discover libsoxr (sinc resampler for PAL fractional-line normalization)
# Prefer a package config target, then fall back to a locally imported library.
set(SOXR_LIBRARIES "")

find_package(unofficial-soxr CONFIG QUIET)
find_package(soxr CONFIG QUIET)

if(TARGET unofficial::soxr::soxr)
    list(APPEND SOXR_LIBRARIES unofficial::soxr::soxr)
elseif(TARGET soxr::soxr)
    list(APPEND SOXR_LIBRARIES soxr::soxr)
else()
    find_path(SOXR_INCLUDE_DIR soxr.h REQUIRED)
    find_library(SOXR_LIBRARY NAMES soxr libsoxr REQUIRED)

    add_library(soxr::soxr UNKNOWN IMPORTED)
    set_target_properties(soxr::soxr PROPERTIES
        IMPORTED_LOCATION "${SOXR_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${SOXR_INCLUDE_DIR}"
    )

    list(APPEND SOXR_LIBRARIES soxr::soxr)
endif()

orc_add_stage_plugin(
    orc-stage-plugin-cvbs-source
    OUTPUT_NAME orc-stage-plugin-cvbs-source
    PLUGIN_VERSION "${ORC_VERSION}"
    SOURCES ${STAGE_PLUGIN_SOURCES}
    LINK_LIBRARIES ${SOXR_LIBRARIES}
)

# Install plugin
install(TARGETS orc-stage-plugin-cvbs-source
    LIBRARY DESTINATION lib/orc/stages
)
