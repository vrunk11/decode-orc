file(GLOB_RECURSE STAGE_PLUGIN_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
)

# SoXR is required for NTSC/PAL_M audio resampling (audio_resampler.cpp).
# Try pkg-config first (more reliable across platforms), then CONFIG mode.
find_package(PkgConfig QUIET)
set(_SOXR_LINK_TARGET "")
if(PkgConfig_FOUND)
    pkg_check_modules(SOXR IMPORTED_TARGET soxr)
    if(SOXR_FOUND)
        set(_SOXR_LINK_TARGET PkgConfig::SOXR)
    endif()
endif()
if(NOT _SOXR_LINK_TARGET)
    find_package(soxr CONFIG QUIET)
    if(soxr_FOUND OR TARGET soxr)
        set(_SOXR_LINK_TARGET soxr)
    endif()
endif()
if(NOT _SOXR_LINK_TARGET)
    # Fallback: try SoXR with capitalised name (some distro packages).
    find_library(SOXR_LIBRARY NAMES soxr)
    find_path(SOXR_INCLUDE_DIR NAMES soxr.h)
    if(SOXR_LIBRARY AND SOXR_INCLUDE_DIR)
        add_library(soxr_fallback INTERFACE)
        target_link_libraries(soxr_fallback INTERFACE "${SOXR_LIBRARY}")
        target_include_directories(soxr_fallback INTERFACE "${SOXR_INCLUDE_DIR}")
        set(_SOXR_LINK_TARGET soxr_fallback)
    endif()
endif()
if(NOT _SOXR_LINK_TARGET)
    message(FATAL_ERROR
        "orc-stage-plugin-tbc-source: SoXR library not found. "
        "Install libsoxr-dev (Debian/Ubuntu), soxr (Homebrew/vcpkg), "
        "or ensure it is available via pkg-config.")
endif()

orc_add_stage_plugin(
    orc-stage-plugin-tbc-source
    OUTPUT_NAME orc-stage-plugin-tbc-source
    PLUGIN_VERSION "${ORC_VERSION}"
    SOURCES ${STAGE_PLUGIN_SOURCES}
    # SQLite is used by tbc_metadata_reader.cpp; declared here because the
    # SDK no longer propagates host third-party dependencies to plugins.
    LINK_LIBRARIES ${_SOXR_LINK_TARGET} SQLite::SQLite3
)
