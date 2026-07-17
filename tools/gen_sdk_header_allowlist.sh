#!/usr/bin/env bash
# File:        gen_sdk_header_allowlist.sh
# Purpose:     Generate cmake/sdk_header_allowlist.txt (the flat SDK contract
#              header allowlist consumed by check_plugin_private_includes.sh)
#              from the single-source manifest orc/sdk/sdk_headers.yaml.
#
#              The allowlist is the deprecation-window superset: every canonical
#              tiered header plus every retained pre-tier shim path, so plugin
#              source that has not yet migrated still passes the include gate.
#
# Usage:       tools/gen_sdk_header_allowlist.sh [<repo-root>]
#
# SPDX-License-Identifier: GPL-3.0-or-later

set -eu

REPO_ROOT="${1:-.}"
MANIFEST="${REPO_ROOT}/orc/sdk/sdk_headers.yaml"
OUT="${REPO_ROOT}/cmake/sdk_header_allowlist.txt"

if [ ! -f "$MANIFEST" ]; then
    echo "gen_sdk_header_allowlist: manifest not found: $MANIFEST" >&2
    exit 1
fi

{
    echo "# GENERATED — do not edit. Source: orc/sdk/sdk_headers.yaml"
    echo "# Regenerate: tools/gen_sdk_header_allowlist.sh"
    grep -E '^[[:space:]]*-[[:space:]]*path:' "$MANIFEST" \
        | sed -E 's/^[[:space:]]*-[[:space:]]*path:[[:space:]]*//' \
        | sort
} > "$OUT"

echo "Wrote $OUT ($(grep -cvE '^#' "$OUT") header paths)"
