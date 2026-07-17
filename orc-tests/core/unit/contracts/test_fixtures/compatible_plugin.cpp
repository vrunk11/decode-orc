/*
 * File:        compatible_plugin.cpp
 * Module:      orc-core-tests / test fixtures
 * Purpose:     Minimal plugin whose descriptor fully matches the host (ABI,
 *              API, toolchain tag). Used by
 *              stage_plugin_loader_version_rejection_test.cpp to verify that
 *              a matching toolchain tag is accepted by the loader.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 decode-orc contributors
 */

#include <orc/abi/orc_plugin_abi.h>

namespace {

constexpr orc::StagePluginDescriptor kDescriptor{
    "test.compatible",  // plugin_id
    "0.0.1",            // plugin_version
    orc::kStagePluginHostAbiVersion,
    orc::kStagePluginApiVersion,
    "GPL-3.0-or-later",     // license_spdx
    false,                  // is_core_plugin
    ORC_SDK_TOOLCHAIN_TAG,  // toolchain_tag — matches the host
};

}  // namespace

ORC_STAGE_PLUGIN_EXPORT const orc::StagePluginDescriptor*
orc_get_stage_plugin_descriptor() {
  return &kDescriptor;
}

// Registers no stages; the fixture only exercises descriptor validation.
// Real stage registration is covered by the functional loader smoke tests.
ORC_STAGE_PLUGIN_EXPORT bool orc_register_stage_plugin(
    const orc::OrcPluginServices* /*services*/, void* /*host_context*/,
    bool (* /*register_stage*/)(void*, const char*, orc::OrcStageFactoryFn),
    const char** /*out_error*/) {
  return true;
}
