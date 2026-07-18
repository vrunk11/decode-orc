/*
 * File:        orc_plugin_sdk.h
 * Module:      decode-orc Plugin SDK
 * Purpose:     Umbrella include for the decode-orc stage plugin SDK
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 decode-orc contributors
 *
 * STABILITY: PUBLIC — This is the recommended single include point for plugin
 *            implementors. All types needed to implement, register, and export
 *            a decode-orc stage plugin are available after including this file.
 *
 * QUICK START:
 *
 *   #include <orc/abi/orc_plugin_sdk.h>
 *   #include "my_stage_implementation.h"   // Your DAGStage subclass
 *
 *   namespace {
 *   constexpr orc::StagePluginDescriptor kDescriptor =
 *       ORC_STAGE_PLUGIN_DESCRIPTOR("com.example.stage.my_filter",  // id
 *                                   "1.0.0",                        // version
 *                                   "MIT",   // license (SPDX)
 *                                   false);  // is_core_plugin
 *   } // namespace
 *
 *   // Expands to both required entrypoints. MyStage is registered under the
 *   // stage name reported by its own get_node_type_info().stage_name; list
 *   // additional stage types as further arguments to register more stages.
 *   ORC_DEFINE_STAGE_PLUGIN(kDescriptor, MyStage)
 *
 *   Plugins that need custom registration logic can instead write the two
 *   entrypoints by hand — see <orc/abi/orc_plugin_abi.h> for the raw
 *   entrypoint contract and <orc/abi/orc_plugin_registration.h> for the
 *   equivalent expanded form.
 *
 * CMAKE INTEGRATION:
 *   In your plugin's CMakeLists.txt:
 *
 *     find_package(decode-orc-plugin-sdk REQUIRED)
 *     orc_add_stage_plugin(
 *         my-stage-plugin
 *         OUTPUT_NAME my-stage-plugin
 *         PLUGIN_VERSION "1.0.0"
 *         SOURCES plugin.cpp my_stage_implementation.cpp
 *     )
 *
 *   Or for in-tree builds, link directly:
 *
 *     target_link_libraries(my-stage-plugin PRIVATE orc-plugin-sdk)
 */

#pragma once

// SDK TIER: abi — frozen binary contract (descriptor/entrypoints/registration/
// services). Any change to this header bumps the host ABI version.

// Host ABI contract: entrypoints, descriptor, versioning, export macro.
#include <orc/abi/orc_plugin_abi.h>

// Stage API: parameter types, node metadata, ParameterizedStage,
// TriggerableStage.
#include <orc/plugin/orc_stage_api.h>

// Stage runtime helpers for plugin stage implementations.
#include <orc/plugin/orc_stage_runtime.h>

// Preview capability and carrier contracts for preview-capable stages.
#include <orc/plugin/orc_stage_preview.h>

// SDK Stage API includes key stage infrastructure: ParameterizedStage,
// TriggerableStage, and preview interfaces. Consolidated stage services API:
// canonical host services for sink/file output and related stage runtime
// helpers.
#include <orc/plugin/orc_stage_services.h>

// Canonical stage helper/tooling descriptors used by GUI/CLI integrations.
#include <orc/plugin/orc_stage_tooling.h>

// Host service table: OrcPluginServices, OrcPluginLogLevel.
// (Must come after orc_stage_api.h so that forward declarations of preview
// types work.)
#include <orc/abi/orc_plugin_services.h>

// Plugin service helpers: ORC_PLUGIN_LOG_* logging macros.
// (Must come after all other includes so that preview types are fully defined
// when inline functions are instantiated.)
#include <orc/plugin/orc_plugin_services_helpers.h>

// Registration helpers: ORC_STAGE_PLUGIN_DESCRIPTOR and
// ORC_DEFINE_STAGE_PLUGIN boilerplate expansion.
// (Needs set_services() from orc_plugin_services.h above.)
#include <orc/abi/orc_plugin_registration.h>
