/*
 * File:        cli_command_extractor.h
 * Module:      orc-presenters
 * Purpose:     Recover the underlying filtergraph string from a pasted
 *              full CLI invocation (program name, --filter or
 *              --input/--filters/--output, shell quoting) so a GUI "Paste
 *              CLI Command" feature can accept whatever someone actually
 *              copies, not just the bare graph.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2025-2026 Simon Inns
 */

#pragma once

#include <string>

namespace orc {
namespace presenters {

/**
 * @brief Extract the filtergraph argument from a pasted CLI command.
 *
 * People will reasonably paste any of these:
 * @code
 *   tbc_source=input_path=a.tbc, video_sink              // bare graph
 *   orc-cli --filter "tbc_source=input_path=a.tbc, video_sink"
 *   orc-cli.exe --filter 'tbc_source=input_path=a.tbc, video_sink'
 *   C:\path\to\orc-cli.exe --input "tbc_source=input_path=a.tbc" --output
 * video_sink
 * @endcode
 *
 * This function locates `--filter`, `--input`, `--filters`, or `--output`
 * as whole-word flags anywhere in the pasted text (so a leading program
 * name, path, or wrapper script is simply never touched — there is no
 * "skip the first word" heuristic to get wrong) and extracts the argument
 * that follows each one:
 *
 * - If the argument is wrapped in matching `'...'` or `"..."`, the quotes
 *   are stripped and the *other* quote character, `:` `/` `\` and spaces
 *   inside are left exactly as typed (the same quote-tracking rules
 *   `parse_filtergraph()` itself uses, so anything that would round-trip
 *   through `--filter` round-trips through a paste too).
 * - If unquoted, the argument runs until the next recognised flag or the
 *   end of the text.
 *
 * `--input`/`--filters`/`--output` are composed into one graph exactly as
 * orc-cli itself does (comma-joined, in that order), regardless of what
 * order they appeared in the pasted text.
 *
 * If none of `--filter`/`--input`/`--filters`/`--output` appear anywhere in
 * the text, it is returned completely unchanged — a bare filtergraph paste
 * (no wrapper at all) is unaffected by this function.
 *
 * @param pasted_text Whatever was pasted into the "Paste CLI Command" box.
 * @return The filtergraph string, ready for `parse_filtergraph()` /
 * `import_filtergraph_into_project()`.
 */
std::string extract_filtergraph_from_pasted_command(
    const std::string& pasted_text);

}  // namespace presenters
}  // namespace orc
