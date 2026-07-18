/*
 * File:        eia608_decoder.h
 * Module:      decode-orc Plugin SDK
 * Purpose:     Deprecated include-path shim — forwards to the tiered SDK layout
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 *
 * DEPRECATED: <orc/stage/eia608_decoder.h> moved to the support tier at
 * <orc/support/eia608_decoder.h> (it is a compiled-into-plugin utility, not a
 * binary-ABI contract type). This shim is retained for one release so
 * third-party plugin source keeps compiling; include the new path directly.
 * Gated by the ORC_SDK_DEPRECATED_INCLUDE_SHIMS CMake option (default ON); when
 * OFF, the host defines ORC_SDK_NO_DEPRECATED_INCLUDE_SHIMS for plugin targets
 * and this shim becomes a hard compile error.
 */
#pragma once

#if defined(ORC_SDK_NO_DEPRECATED_INCLUDE_SHIMS)
#error \
    "Deprecated SDK include path <orc/stage/eia608_decoder.h>; include <orc/support/eia608_decoder.h> instead."
#endif

#include <orc/support/eia608_decoder.h>
