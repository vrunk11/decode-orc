# CVBS Source Stage Plugin
#
# Loads composite CVBS (Composite Video Baseband Signal) sources for PAL,
# NTSC, and PAL-M systems.
#
# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2026 Simon Inns

file(GLOB_RECURSE STAGE_PLUGIN_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
)

orc_add_stage_plugin(
    orc-stage-plugin-cvbs-source
    OUTPUT_NAME orc-stage-plugin-cvbs-source
    PLUGIN_VERSION "${ORC_VERSION}"
    SOURCES ${STAGE_PLUGIN_SOURCES}
    # SQLite is used by cvbs_source_stage.cpp; declared here because the
    # SDK no longer propagates host third-party dependencies to plugins.
    # orc-audio-resample is the shared SoXR-based ingest converter
    # (orc/plugins/stages/common/audio-resample), used to convert WAV
    # payloads into 48 kHz synchronous channel-pair audio.
    LINK_LIBRARIES orc-audio-resample SQLite::SQLite3
)

# Install plugin
install(TARGETS orc-stage-plugin-cvbs-source
    LIBRARY DESTINATION lib/orc/stages
)
