/*
 * File:        cli_command_extractor.cpp
 * Module:      orc-presenters
 * Purpose:     Implementation of the pasted-CLI-command filtergraph
 *              extractor.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2025-2026 Simon Inns
 *
 * This translation unit deliberately depends only on the C++ standard
 * library so it can be unit-tested in isolation, in the same spirit as
 * filtergraph_parser.cpp.
 */

#include "cli_command_extractor.h"

#include <cctype>
#include <string>
#include <vector>

namespace orc {
namespace presenters {

namespace {

bool is_word_boundary_char(char c) {
  return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

std::string rstrip(const std::string& s) {
  size_t end = s.size();
  while (end > 0 && is_word_boundary_char(s[end - 1])) {
    --end;
  }
  return s.substr(0, end);
}

std::string lstrip(const std::string& s) {
  size_t begin = 0;
  while (begin < s.size() && is_word_boundary_char(s[begin])) {
    ++begin;
  }
  return s.substr(begin);
}

std::string strip(const std::string& s) { return lstrip(rstrip(s)); }

/**
 * Find the next occurrence of `word` in `text` as a standalone token: the
 * character before the match (if any) and the character after it (if any)
 * must both be whitespace. This is what stops a search for "--filter" from
 * matching inside "--filters".
 */
size_t find_whole_word(const std::string& text, const std::string& word,
                       size_t from_pos) {
  size_t pos = from_pos;
  while (true) {
    pos = text.find(word, pos);
    if (pos == std::string::npos) {
      return std::string::npos;
    }
    const bool start_ok = (pos == 0) || is_word_boundary_char(text[pos - 1]);
    const size_t after = pos + word.size();
    const bool end_ok =
        (after >= text.size()) || is_word_boundary_char(text[after]);
    if (start_ok && end_ok) {
      return pos;
    }
    pos += 1;  // Keep searching past this false match.
  }
}

/// The whole-word flags this extractor recognises.
const std::vector<std::string>& recognised_flags() {
  static const std::vector<std::string> flags = {"--filter", "--input",
                                                 "--filters", "--output"};
  return flags;
}

/// Position of the next recognised flag at or after `from_pos`, or
/// std::string::npos if none appears.
size_t find_next_flag(const std::string& text, size_t from_pos) {
  size_t best = std::string::npos;
  for (const auto& flag : recognised_flags()) {
    const size_t pos = find_whole_word(text, flag, from_pos);
    if (pos != std::string::npos && (best == std::string::npos || pos < best)) {
      best = pos;
    }
  }
  return best;
}

/**
 * Extract the argument following a flag, starting at `pos` (which must
 * already be past the flag and any whitespace). Returns the unquoted
 * argument text and updates `pos` to just past what was consumed.
 *
 * If the argument is quoted, uses the same quote-tracking rule
 * `filtergraph_parser` itself uses: the *other* quote character, and
 * backslashes, are left completely alone inside the quoted span — only a
 * matching, unescaped closing quote ends it. If unquoted, the argument
 * simply runs until the next recognised flag or the end of the text.
 */
std::string extract_argument(const std::string& text, size_t& pos) {
  if (pos >= text.size()) {
    return "";
  }

  const char c = text[pos];
  if (c == '\'' || c == '"') {
    const char quote_char = c;
    size_t i = pos + 1;
    while (i < text.size()) {
      if (text[i] == '\\' && i + 1 < text.size()) {
        i += 2;  // Skip an escaped character (e.g. \" inside a "..." span).
        continue;
      }
      if (text[i] == quote_char) {
        break;
      }
      ++i;
    }
    const std::string argument = text.substr(pos + 1, i - (pos + 1));
    pos = (i < text.size()) ? i + 1 : i;
    return argument;
  }

  const size_t next_flag = find_next_flag(text, pos);
  const size_t end = (next_flag == std::string::npos) ? text.size() : next_flag;
  const std::string argument = strip(text.substr(pos, end - pos));
  pos = end;
  return argument;
}

/// If `flag` appears (as a whole word) anywhere in `text`, returns its
/// argument; otherwise returns an empty string.
std::string extract_flag_argument(const std::string& text,
                                  const std::string& flag) {
  const size_t flag_pos = find_whole_word(text, flag, 0);
  if (flag_pos == std::string::npos) {
    return "";
  }
  size_t pos = flag_pos + flag.size();
  while (pos < text.size() && is_word_boundary_char(text[pos])) {
    ++pos;
  }
  return extract_argument(text, pos);
}

}  // namespace

std::string extract_filtergraph_from_pasted_command(
    const std::string& pasted_text) {
  // --filter takes priority: if present, its argument *is* the graph.
  const size_t filter_pos = find_whole_word(pasted_text, "--filter", 0);
  if (filter_pos != std::string::npos) {
    size_t pos = filter_pos + std::string("--filter").size();
    while (pos < pasted_text.size() &&
           is_word_boundary_char(pasted_text[pos])) {
      ++pos;
    }
    return extract_argument(pasted_text, pos);
  }

  // Otherwise, look for the input/filters/output triad and compose it
  // exactly as orc-cli itself does: comma-joined, in that fixed order,
  // regardless of the order the flags appeared in.
  const std::string input_part = extract_flag_argument(pasted_text, "--input");
  const std::string filters_part =
      extract_flag_argument(pasted_text, "--filters");
  const std::string output_part =
      extract_flag_argument(pasted_text, "--output");

  if (input_part.empty() && filters_part.empty() && output_part.empty()) {
    // None of the recognised flags were found anywhere: this is presumably
    // already a bare filtergraph, so leave it completely untouched.
    return pasted_text;
  }

  std::string combined;
  for (const auto& part : {input_part, filters_part, output_part}) {
    if (part.empty()) {
      continue;
    }
    if (!combined.empty()) {
      combined += ",";
    }
    combined += part;
  }
  return combined;
}

}  // namespace presenters
}  // namespace orc
