file(GLOB_RECURSE STAGE_PLUGIN_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
)

orc_add_stage_plugin(
    orc-stage-plugin-ld-sink
    OUTPUT_NAME orc-stage-plugin-ld-sink
    PLUGIN_VERSION "${ORC_VERSION}"
    SOURCES ${STAGE_PLUGIN_SOURCES}
    # SQLite is used by tbc_metadata_writer.cpp; declared here because the
    # SDK no longer propagates host third-party dependencies to plugins.
    LINK_LIBRARIES SQLite::SQLite3
)

# tbc_metadata_types.h lives in tbc_source — both stages share the same
# ld-decode field data structures.
target_include_directories(orc-stage-plugin-ld-sink PRIVATE
    ${CMAKE_SOURCE_DIR}/orc/plugins/stages/tbc_source
)
