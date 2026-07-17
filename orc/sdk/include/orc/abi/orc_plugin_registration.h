/*
 * File:        orc_plugin_registration.h
 * Module:      decode-orc Plugin SDK
 * Purpose:     Descriptor and entrypoint boilerplate helpers for stage plugins
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 decode-orc contributors
 *
 * STABILITY: PUBLIC — This header is part of the stable plugin SDK.
 *
 * USAGE:
 *   Include via the umbrella header <orc/abi/orc_plugin_sdk.h>.
 *
 *   Most plugins consist of a descriptor and the two required entrypoints,
 *   which register one or more default-constructible DAGStage subclasses.
 *   The helpers below expand that entire pattern from two statements:
 *
 *     #include <orc/abi/orc_plugin_sdk.h>
 *     #include "my_stage.h"
 *
 *     namespace {
 *     constexpr orc::StagePluginDescriptor kPluginDescriptor =
 *         ORC_STAGE_PLUGIN_DESCRIPTOR("com.example.stage.my_filter",
 *                                     "1.0.0", "MIT", false);
 *     }  // namespace
 *
 *     ORC_DEFINE_STAGE_PLUGIN(kPluginDescriptor, MyStage)
 *
 *   ORC_DEFINE_STAGE_PLUGIN accepts one or more stage types; each is
 *   registered under the stage name reported by its own
 *   get_node_type_info().stage_name, so the stage class is the single source
 *   of truth for registration metadata. Stage types must be either
 *   default-constructible or constructible from IStageServices* (the host's
 *   consolidated stage services are injected automatically in that case).
 *
 *   Plugins that need custom registration logic (other constructor
 *   signatures, conditional registration, ...) can keep hand-written
 *   entrypoints; the raw entrypoint contract in
 *   <orc/abi/orc_plugin_abi.h> remains fully supported.
 */

#pragma once

// SDK TIER: abi — frozen binary contract (descriptor/entrypoints/registration/
// services). Any change to this header bumps the host ABI version.

#include <orc/abi/orc_plugin_abi.h>
#include <orc/abi/orc_plugin_services.h>
#include <orc/stage/stage.h>

#include <memory>
#include <string>
#include <type_traits>

namespace orc::plugin {

/// Stage factory the host stores and invokes on demand. Matches
/// OrcStageFactoryFn so its address can be passed to register_stage.
///
/// Stages that declare an IStageServices* constructor (typically sink stages
/// using the host's buffered file writers) receive the consolidated stage
/// services at creation time; all other stages are default-constructed.
template <typename StageT>
std::shared_ptr<DAGStage> make_stage() {
  if constexpr (std::is_constructible_v<StageT, IStageServices*>) {
    return std::make_shared<StageT>(get_stage_services());
  } else {
    return std::make_shared<StageT>();
  }
}

/// Registers a single stage type under the name reported by its own
/// NodeTypeInfo. On failure, *error_message points to a static string.
template <typename StageT>
bool register_single_stage(void* context,
                           bool (*register_stage)(void* context,
                                                  const char* stage_name,
                                                  OrcStageFactoryFn factory),
                           const char** error_message) {
  const std::shared_ptr<DAGStage> probe = make_stage<StageT>();
  if (!probe) {
    if (error_message) {
      *error_message = "Stage factory returned a null stage instance";
    }
    return false;
  }

  const std::string stage_name = probe->get_node_type_info().stage_name;
  if (stage_name.empty()) {
    if (error_message) {
      *error_message = "Stage reported an empty NodeTypeInfo stage_name";
    }
    return false;
  }

  // The host copies the name during the callback, so the temporary string
  // only needs to outlive the register_stage call itself.
  if (!register_stage(context, stage_name.c_str(), &make_stage<StageT>)) {
    if (error_message) {
      *error_message = "Host rejected stage registration";
    }
    return false;
  }

  return true;
}

/// Full orc_register_stage_plugin() body: stores the host service table,
/// validates the callback, and registers every listed stage type in order,
/// stopping at the first failure.
template <typename... StageTs>
bool register_stage_plugin(const OrcPluginServices* services, void* context,
                           bool (*register_stage)(void* context,
                                                  const char* stage_name,
                                                  OrcStageFactoryFn factory),
                           const char** error_message) {
  static_assert(sizeof...(StageTs) > 0,
                "ORC_DEFINE_STAGE_PLUGIN requires at least one stage type");

  set_services(services);

  if (!register_stage) {
    if (error_message) {
      *error_message = "Missing stage registration callback";
    }
    return false;
  }

  return (
      register_single_stage<StageTs>(context, register_stage, error_message) &&
      ...);
}

}  // namespace orc::plugin

/// Expands to a StagePluginDescriptor braced initialiser with the host ABI
/// version, plugin API version, and toolchain tag filled in from the SDK the
/// plugin is compiled against — the fields a plugin author must never set by
/// hand.
#define ORC_STAGE_PLUGIN_DESCRIPTOR(plugin_id, plugin_version, license_spdx, \
                                    is_core_plugin)                          \
  ::orc::StagePluginDescriptor {                                             \
    plugin_id, plugin_version, ::orc::kStagePluginHostAbiVersion,            \
        ::orc::kStagePluginApiVersion, license_spdx, is_core_plugin,         \
        ORC_SDK_TOOLCHAIN_TAG                                                \
  }

/// Expands to both required plugin entrypoints. `descriptor` is an lvalue of
/// static storage duration (typically built with ORC_STAGE_PLUGIN_DESCRIPTOR);
/// the remaining arguments are one or more default-constructible DAGStage
/// subclasses, each registered under its own NodeTypeInfo stage_name.
#define ORC_DEFINE_STAGE_PLUGIN(descriptor, ...)                    \
  ORC_STAGE_PLUGIN_EXPORT const orc::StagePluginDescriptor*         \
  orc_get_stage_plugin_descriptor() {                               \
    return &(descriptor);                                           \
  }                                                                 \
  ORC_STAGE_PLUGIN_EXPORT bool orc_register_stage_plugin(           \
      const orc::OrcPluginServices* services, void* context,        \
      bool (*register_stage)(void* context, const char* stage_name, \
                             orc::OrcStageFactoryFn factory),       \
      const char** error_message) {                                 \
    return orc::plugin::register_stage_plugin<__VA_ARGS__>(         \
        services, context, register_stage, error_message);          \
  }
