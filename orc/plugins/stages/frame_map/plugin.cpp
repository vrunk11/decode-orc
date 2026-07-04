/*
 * File:        plugin.cpp
 * Module:      orc-stage-plugin-frame-map
 * Purpose:     Runtime plugin bundle for FrameMapStage
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 decode-orc contributors
 */

#include <orc/plugin/orc_plugin_sdk.h>

#include "frame_map_stage.h"

#ifndef ORC_STAGE_PLUGIN_VERSION
#define ORC_STAGE_PLUGIN_VERSION "dev"
#endif

namespace {

constexpr orc::StagePluginDescriptor kPluginDescriptor =
    ORC_STAGE_PLUGIN_DESCRIPTOR("decode-orc.stage.frame_map",
                                ORC_STAGE_PLUGIN_VERSION, "GPL-3.0-or-later",
                                true);

}  // namespace

ORC_DEFINE_STAGE_PLUGIN(kPluginDescriptor, orc::FrameMapStage)
