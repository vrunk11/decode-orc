/*
 * File:        abi_v4_plugin.cpp
 * Module:      orc-core-tests / test fixtures
 * Purpose:     Minimal plugin that declares the previous host ABI version (4).
 *              Used by stage_plugin_loader_version_rejection_test.cpp to verify
 *              that plugins built against the pre-toolchain-tag ABI are
 *              rejected with a version mismatch diagnostic.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 decode-orc contributors
 */

#include <orc/plugin/orc_plugin_abi.h>

namespace {

// A real ABI-4 descriptor ends at is_core_plugin (no toolchain_tag field).
// The loader must reject on the version number alone, before ever reading
// the appended v5 field.
constexpr orc::StagePluginDescriptor kDescriptor{
    "test.abi-v4",  // plugin_id
    "0.0.1",        // plugin_version
    4u,             // host_abi_version — previous ABI revision
    orc::kStagePluginApiVersion,
    "GPL-3.0-or-later",  // license_spdx
    false,               // is_core_plugin
    nullptr,             // toolchain_tag — absent in a genuine ABI-4 binary
};

}  // namespace

ORC_STAGE_PLUGIN_EXPORT const orc::StagePluginDescriptor*
orc_get_stage_plugin_descriptor() {
  return &kDescriptor;
}

ORC_STAGE_PLUGIN_EXPORT bool orc_register_stage_plugin(
    const orc::OrcPluginServices* /*services*/, void* /*host_context*/,
    bool (* /*register_stage*/)(void*, const char*, orc::OrcStageFactoryFn),
    const char** /*out_error*/) {
  return false;
}
