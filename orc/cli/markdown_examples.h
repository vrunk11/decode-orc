/*
 * File:        markdown_examples.h
 * Module:      orc-cli
 * Purpose:     Extract a "## Examples" section (description + fenced code
 *              block pairs) from a stage's instructions.md, so CLI usage
 *              examples authored by plugin maintainers can be surfaced by
 *              'orc-cli plugins describe'.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2025-2026 Simon Inns
 */

#pragma once

#include <string>
#include <vector>

namespace orc {
namespace cli {

/**
 * @brief One example extracted from an "## Examples" section.
 *
 * `description` is the paragraph of prose immediately preceding the code
 * fence (may be empty if the fence has no preceding text). `command` is the
 * fenced code block's content, trimmed, with the fence markers removed.
 */
struct MarkdownExample {
  std::string description;
  std::string command;
};

/**
 * @brief Result of scanning a Markdown document for an "## Examples" section.
 */
struct MarkdownExamplesResult {
  bool found = false;  ///< Whether a level-2 "## Examples" heading was found.
  std::vector<MarkdownExample> examples;
};

/**
 * @brief Extract the "## Examples" section from a stage's instructions.md
 * (or any Markdown document following the same convention).
 *
 * This is entirely a documentation convention — it requires no change to
 * the stage plugin ABI or SDK. Any stage's get_instructions() (core or
 * third-party plugin) can opt in simply by including a section like:
 *
 * @code
 * ## Examples
 *
 * Decode a composite capture to MP4:
 *
 * ```bash
 * orc-cli --input "tbc_source=input_path=capture.tbc" \
 *         --output "video_sink=output_path=out.mp4"
 * ```
 * @endcode
 *
 * The section runs from a line matching exactly `## Examples` (any
 * heading level >= 2 other than exactly "## Examples" does not start a
 * new example section) until the next `##`-or-higher-level heading, or the
 * end of the document. Within it, each fenced code block (``` ... ```,
 * with or without a language tag) becomes one example; the paragraph of
 * text immediately before the fence (i.e. since the last blank line or the
 * section start) becomes that example's description.
 *
 * Parsing is best-effort and never fails: a document with no "## Examples"
 * heading simply yields `found == false`; a heading with no fenced code
 * blocks yields `found == true` with an empty `examples` list.
 *
 * @param markdown Full Markdown document (e.g. a stage's get_instructions()).
 * @return Parsed examples, or `found == false` if no section exists.
 */
MarkdownExamplesResult extract_examples_section(const std::string& markdown);

/**
 * @brief Return `markdown` with its "## Examples" section (if any) removed.
 *
 * Used to avoid showing the same content twice when a caller wants to print
 * the general instructions text and a separately, prominently formatted
 * rendering of the examples (see `extract_examples_section`). If there is no
 * "## Examples" section, returns `markdown` unchanged.
 *
 * @param markdown Full Markdown document.
 * @return `markdown` with the Examples section's lines removed.
 */
std::string remove_examples_section(const std::string& markdown);

}  // namespace cli
}  // namespace orc
