#!/usr/bin/env bash
# File:        check_sdk_header_manifest.sh
# Purpose:     Guard that the SDK header manifest (orc/sdk/sdk_headers.yaml) is
#              the single source of truth for the SDK contract surface:
#                (1) the set of paths in the manifest exactly matches the set of
#                    *.h files on disk under orc/sdk/include, and
#                (2) the generated cmake/sdk_header_allowlist.txt is in sync with
#                    the manifest (i.e. tools/gen_sdk_header_allowlist.sh would
#                    produce no change).
#              Adding or deleting an SDK header without updating the manifest
#              fails this gate (CTest label "sdk").
#
# Usage:       check_sdk_header_manifest.sh [<repo-root>]
#
# SPDX-License-Identifier: GPL-3.0-or-later

set -u

# Match the collation used by tools/gen_sdk_header_allowlist.sh so the
# staleness comparison below is locale-independent (see that script's note).
export LC_ALL=C

REPO_ROOT="${1:-.}"
INC="${REPO_ROOT}/orc/sdk/include"
MANIFEST="${REPO_ROOT}/orc/sdk/sdk_headers.yaml"
ALLOWLIST="${REPO_ROOT}/cmake/sdk_header_allowlist.txt"
GEN="${REPO_ROOT}/tools/gen_sdk_header_allowlist.sh"

fail=0

if [ ! -f "$MANIFEST" ]; then
    echo "❌ manifest not found: $MANIFEST"
    exit 1
fi

tmp_manifest_paths="$(mktemp)"
tmp_disk_paths="$(mktemp)"
tmp_allowlist_now="$(mktemp)"
trap 'rm -f "$tmp_manifest_paths" "$tmp_disk_paths" "$tmp_allowlist_now"' EXIT

# (1) manifest paths vs on-disk headers
grep -E '^[[:space:]]*-[[:space:]]*path:' "$MANIFEST" \
    | sed -E 's/^[[:space:]]*-[[:space:]]*path:[[:space:]]*//' \
    | sort > "$tmp_manifest_paths"

( cd "$INC" && find orc -type f -name '*.h' | sort ) > "$tmp_disk_paths"

if ! diff -u "$tmp_manifest_paths" "$tmp_disk_paths" > /dev/null; then
    echo "❌ SDK header manifest is out of sync with orc/sdk/include/."
    echo "   (< manifest-only, > on-disk-only)"
    diff "$tmp_manifest_paths" "$tmp_disk_paths" | grep -E '^[<>]' \
        | sed 's/^/   /'
    echo
    echo "   Update orc/sdk/sdk_headers.yaml to match, then regenerate the"
    echo "   allowlist:  tools/gen_sdk_header_allowlist.sh"
    fail=1
fi

# (2) generated allowlist staleness
if [ -x "$GEN" ] || [ -f "$GEN" ]; then
    # Regenerate into a temp location and compare, without touching the tree.
    grep -E '^[[:space:]]*-[[:space:]]*path:' "$MANIFEST" \
        | sed -E 's/^[[:space:]]*-[[:space:]]*path:[[:space:]]*//' \
        | sort > "$tmp_allowlist_now"
    if [ -f "$ALLOWLIST" ]; then
        # Strip the generated comment header before comparing.
        if ! diff -u <(grep -vE '^#' "$ALLOWLIST") "$tmp_allowlist_now" > /dev/null; then
            echo "❌ cmake/sdk_header_allowlist.txt is stale relative to the manifest."
            echo "   Regenerate:  tools/gen_sdk_header_allowlist.sh"
            fail=1
        fi
    else
        echo "❌ cmake/sdk_header_allowlist.txt is missing."
        echo "   Generate:  tools/gen_sdk_header_allowlist.sh"
        fail=1
    fi
fi

if [ "$fail" -eq 0 ]; then
    echo "✓ SDK header manifest matches disk and the generated allowlist is current"
    exit 0
fi
exit 1
