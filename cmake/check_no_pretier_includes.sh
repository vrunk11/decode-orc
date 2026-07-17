#!/usr/bin/env bash
# File:        check_no_pretier_includes.sh
# Purpose:     Ensure in-tree code (host, bundled plugins, and tests) uses the
#              tiered SDK include paths only — never a deprecated pre-tier shim
#              path. The shim headers (orc/sdk/include) exist solely for
#              out-of-tree third-party source compatibility for one release;
#              nothing inside this repository may depend on them. Guards the
#              Phase 1 migration against regressions (CTest label "sdk").
#
#              The deprecated path set is derived from the manifest
#              (orc/sdk/sdk_headers.yaml, entries with deprecated: true) so this
#              gate needs no hand-maintained list.
#
# Usage:       check_no_pretier_includes.sh [<repo-root>]
#
# SPDX-License-Identifier: GPL-3.0-or-later

set -u
REPO_ROOT="${1:-.}"
MANIFEST="${REPO_ROOT}/orc/sdk/sdk_headers.yaml"

if [ ! -f "$MANIFEST" ]; then
    echo "❌ manifest not found: $MANIFEST"
    exit 1
fi

# Collect deprecated (pre-tier) header paths from the manifest.
dep_paths="$(awk '
    /^[[:space:]]*-[[:space:]]*path:/ { p=$0; sub(/^[^:]*:[[:space:]]*/,"",p); next }
    /^[[:space:]]*deprecated:[[:space:]]*true/ { print p }
' "$MANIFEST")"

if [ -z "$dep_paths" ]; then
    echo "✓ no deprecated pre-tier paths in the manifest (nothing to guard)"
    exit 0
fi

# Build an ERE alternation of the deprecated include directives. The single
# '.' after #include matches either delimiter ('<' or '"').
alt="$(printf '%s\n' "$dep_paths" | sed 's/[.]/\\./g' | paste -sd'|' -)"
pattern="#include[[:space:]]*.(${alt})"

# Scan all in-tree C/C++ source EXCEPT the SDK shim headers themselves.
hits="$(grep -rnE "$pattern" "${REPO_ROOT}/orc" "${REPO_ROOT}/orc-tests" \
    --include='*.h' --include='*.hpp' --include='*.hh' \
    --include='*.cpp' --include='*.cc' --include='*.cxx' 2>/dev/null \
    | grep -v "${REPO_ROOT}/orc/sdk/include/")"

if [ -z "$hits" ]; then
    echo "✓ in-tree code uses tiered SDK include paths only (no pre-tier shims)"
    exit 0
fi

echo "❌ in-tree code includes deprecated pre-tier SDK paths (use the tiered path):"
printf '%s\n' "$hits" | sed 's/^/   /'
exit 1
