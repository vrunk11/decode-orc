#!/usr/bin/env bash
# File:        check_sdk_header_docs.sh
# Purpose:     Fail if the generated SDK header tables in
#              docs/technical/plugin-sdk.md (between the GENERATED SDK HEADER
#              TABLES markers) diverge from tools/gen_sdk_header_docs.sh output,
#              i.e. the docs are stale relative to orc/sdk/sdk_headers.yaml.
#              Regenerate with:
#                tools/gen_sdk_header_docs.sh . > /tmp/block && \
#                  splice it back between the markers.
#
# Usage:       check_sdk_header_docs.sh [<repo-root>]
#
# SPDX-License-Identifier: GPL-3.0-or-later

set -u
REPO_ROOT="${1:-.}"
DOC="${REPO_ROOT}/docs/technical/plugin-sdk.md"
GEN="${REPO_ROOT}/tools/gen_sdk_header_docs.sh"

tmp_gen="$(mktemp)"
tmp_doc="$(mktemp)"
trap 'rm -f "$tmp_gen" "$tmp_doc"' EXIT

bash "$GEN" "$REPO_ROOT" > "$tmp_gen"

# Extract the committed block, markers inclusive.
awk '
  /BEGIN GENERATED SDK HEADER TABLES/ { inb=1 }
  inb { print }
  /END GENERATED SDK HEADER TABLES/ { inb=0 }
' "$DOC" > "$tmp_doc"

if diff -u "$tmp_doc" "$tmp_gen" > /dev/null; then
    echo "✓ plugin-sdk.md SDK header tables are in sync with the manifest"
    exit 0
fi
echo "❌ plugin-sdk.md SDK header tables are stale relative to the manifest."
echo "   Regenerate the block between the GENERATED SDK HEADER TABLES markers:"
echo "     tools/gen_sdk_header_docs.sh . (splice its output into the doc)"
echo "   (< committed, > generated):"
diff "$tmp_doc" "$tmp_gen" | sed 's/^/   /' | head -40
exit 1
