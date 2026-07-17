/*
 * File:        orc_plugin_registration.h
 * Module:      decode-orc Plugin SDK
 * Purpose:     Deprecated include-path shim — forwards to the tiered SDK layout
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 decode-orc contributors
 *
 * DEPRECATED: <orc/plugin/orc_plugin_registration.h> moved to
 * <orc/abi/orc_plugin_registration.h>. This shim is retained for one release so
 * third-party plugin source keeps compiling; include the new path directly.
 * Gated by the ORC_SDK_DEPRECATED_INCLUDE_SHIMS CMake option (default ON); when
 * OFF, the host defines ORC_SDK_NO_DEPRECATED_INCLUDE_SHIMS for plugin targets
 * and this shim becomes a hard compile error.
 */
#pragma once

#if defined(ORC_SDK_NO_DEPRECATED_INCLUDE_SHIMS)
#error \
    "Deprecated SDK include path <orc/plugin/orc_plugin_registration.h>; include <orc/abi/orc_plugin_registration.h> instead."
#endif

#include <orc/abi/orc_plugin_registration.h>
