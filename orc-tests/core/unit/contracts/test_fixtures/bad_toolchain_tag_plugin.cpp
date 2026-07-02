/*
 * File:        bad_toolchain_tag_plugin.cpp
 * Module:      orc-core-tests / test fixtures
 * Purpose:     Minimal plugin with matching version numbers but a mismatched
 *              toolchain tag. Used by
 *              stage_plugin_loader_version_rejection_test.cpp to verify that
 *              the loader rejects plugins built with an incompatible
 *              toolchain and names both tags in the diagnostic.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 decode-orc contributors
 */

#include <orc/plugin/orc_plugin_abi.h>

namespace {

constexpr orc::StagePluginDescriptor kDescriptor{
    "test.bad-toolchain-tag",  // plugin_id
    "0.0.1",                   // plugin_version
    orc::kStagePluginHostAbiVersion,
    orc::kStagePluginApiVersion,
    "GPL-3.0-or-later",               // license_spdx
    false,                            // is_core_plugin
    "test-mismatched-toolchain-tag",  // toolchain_tag — intentionally wrong
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
