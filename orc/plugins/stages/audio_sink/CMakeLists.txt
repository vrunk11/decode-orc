file(GLOB_RECURSE STAGE_PLUGIN_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
)

orc_add_stage_plugin(
    orc-stage-plugin-audio-sink
    OUTPUT_NAME orc-stage-plugin-audio-sink
    PLUGIN_VERSION "${ORC_VERSION}"
    SOURCES ${STAGE_PLUGIN_SOURCES}
    # orc-audio-resample is the shared SoXR-based free-running↔locked
    # resampler (orc/plugins/stages/common/audio-resample), used for the
    # free_running_44100 export mode on NTSC/PAL-M sources.
    LINK_LIBRARIES orc-audio-resample
)
