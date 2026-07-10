file(GLOB STAGE_PLUGIN_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
)

orc_add_stage_plugin(
    orc-stage-plugin-efm-audio-decode
    OUTPUT_NAME orc-stage-plugin-efm-audio-decode
    PLUGIN_VERSION "${ORC_VERSION}"
    SOURCES ${STAGE_PLUGIN_SOURCES}
    LINK_LIBRARIES orc-efm-decode
)
