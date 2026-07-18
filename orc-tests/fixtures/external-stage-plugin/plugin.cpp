/*
 * File:        plugin.cpp
 * Module:      external-stage-plugin fixture
 * Purpose:     Plugin bundle for the SDK-package validation fixture stage
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#include <orc/abi/orc_plugin_sdk.h>

#include "fixture_passthrough_stage.h"

#ifndef ORC_STAGE_PLUGIN_VERSION
#define ORC_STAGE_PLUGIN_VERSION "dev"
#endif

namespace {

constexpr orc::StagePluginDescriptor kPluginDescriptor =
    ORC_STAGE_PLUGIN_DESCRIPTOR("decode-orc.fixture.external_passthrough",
                                ORC_STAGE_PLUGIN_VERSION, "GPL-3.0-or-later",
                                false);

}  // namespace

ORC_DEFINE_STAGE_PLUGIN(kPluginDescriptor, orc_fixture::FixturePassthroughStage)
