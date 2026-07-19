/*
 * File:        markdown_examples.cpp
 * Module:      orc-cli
 * Purpose:     Implementation of the "## Examples" section extractor.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2025-2026 Simon Inns
 *
 * This translation unit deliberately depends only on the C++ standard
 * library so it can be unit-tested in isolation, in the same spirit as
 * filtergraph_parser.cpp.
 */

#include "markdown_examples.h"

#include <sstream>
#include <string>
#include <vector>

namespace orc {
namespace cli {

namespace {

std::string rstrip(const std::string& s) {
  size_t end = s.size();
  while (end > 0 &&
        (s[end - 1] == ' ' || s[end - 1] == '\t' || s[end - 1] == '\r')) {
    --end;
  }
  return s.substr(0, end);
}

std::string lstrip(const std::string& s) {
  size_t begin = 0;
  while (begin < s.size() && (s[begin] == ' ' || s[begin] == '\t')) {
    ++begin;
  }
  return s.substr(begin);
}

std::string strip(const std::string& s) { return lstrip(rstrip(s)); }

bool is_blank(const std::string& s) { return strip(s).empty(); }

/// Returns the heading level (1-6) if `line` is an ATX heading ("# ", "## ",
/// ...), or 0 if it is not a heading.
int heading_level(const std::string& line) {
  const std::string trimmed = rstrip(line);
  size_t i = 0;
  while (i < trimmed.size() && trimmed[i] == '#') {
    ++i;
  }
  if (i == 0 || i > 6) {
    return 0;
  }
  // ATX headings require a space (or end of line, for an empty heading)
  // after the '#' run.
  if (i < trimmed.size() && trimmed[i] != ' ') {
    return 0;
  }
  return static_cast<int>(i);
}

/// The heading text for an ATX heading line (trimmed, with any trailing
/// closing "#"s removed per the ATX spec).
std::string heading_text(const std::string& line, int level) {
  std::string rest = rstrip(line).substr(static_cast<size_t>(level));
  rest = strip(rest);
  // Strip an optional trailing run of '#' (closing sequence).
  size_t end = rest.size();
  while (end > 0 && rest[end - 1] == '#') {
    --end;
  }
  return strip(rest.substr(0, end));
}

/// Whether `line`, once leading whitespace is stripped, opens/closes a
/// fenced code block (a run of three or more backticks).
bool is_fence_line(const std::string& line) {
  const std::string trimmed = lstrip(line);
  return trimmed.size() >= 3 && trimmed[0] == '`' && trimmed[1] == '`' &&
        trimmed[2] == '`';
}

std::vector<std::string> split_lines(const std::string& text) {
  std::vector<std::string> lines;
  std::istringstream iss(text);
  std::string line;
  while (std::getline(iss, line)) {
    lines.push_back(line);
  }
  return lines;
}

std::string join_paragraph(const std::vector<std::string>& lines) {
  std::string out;
  for (size_t i = 0; i < lines.size(); ++i) {
    if (i > 0) {
      out += ' ';
    }
    out += strip(lines[i]);
  }
  return strip(out);
}

std::string join_fence(const std::vector<std::string>& lines) {
  std::string out;
  for (size_t i = 0; i < lines.size(); ++i) {
    if (i > 0) {
      out += '\n';
    }
    out += rstrip(lines[i]);
  }
  // Trim leading/trailing blank lines while preserving internal indentation.
  size_t begin = 0;
  std::vector<std::string> out_lines = split_lines(out);
  size_t first_non_blank = out_lines.size();
  size_t last_non_blank = 0;
  bool any = false;
  for (size_t i = 0; i < out_lines.size(); ++i) {
    if (!is_blank(out_lines[i])) {
      if (!any) {
        first_non_blank = i;
        any = true;
      }
      last_non_blank = i;
    }
  }
  if (!any) {
    return "";
  }
  std::string trimmed;
  for (size_t i = first_non_blank; i <= last_non_blank; ++i) {
    if (i > first_non_blank) {
      trimmed += '\n';
    }
    trimmed += out_lines[i];
  }
  (void)begin;
  return trimmed;
}

/// Locates the "## Examples" section within `lines`. Returns true and sets
/// `heading_index` (the heading line itself), `body_start` (first line after
/// the heading), and `body_end` (exclusive; the next heading of level <= 2,
/// or lines.size()) if found.
bool find_examples_section(const std::vector<std::string>& lines,
                          size_t& heading_index, size_t& body_start,
                          size_t& body_end) {
  for (size_t i = 0; i < lines.size(); ++i) {
    const int level = heading_level(lines[i]);
    if (level == 2 && heading_text(lines[i], level) == "Examples") {
      heading_index = i;
      body_start = i + 1;
      body_end = lines.size();
      for (size_t j = body_start; j < lines.size(); ++j) {
        const int next_level = heading_level(lines[j]);
        if (next_level != 0 && next_level <= 2) {
          body_end = j;
          break;
        }
      }
      return true;
    }
  }
  return false;
}

}  // namespace

MarkdownExamplesResult extract_examples_section(const std::string& markdown) {
  MarkdownExamplesResult result;

  const std::vector<std::string> lines = split_lines(markdown);

  size_t heading_index = 0, start = 0, end = 0;
  if (!find_examples_section(lines, heading_index, start, end)) {
    return result;
  }
  result.found = true;
  //    preceding paragraph of prose as its description.
  bool in_fence = false;
  std::vector<std::string> fence_lines;
  std::vector<std::string> pending_paragraph;  // in-progress, not yet closed
  std::vector<std::string> last_paragraph;     // most recently closed

  for (size_t i = start; i < end; ++i) {
    const std::string& line = lines[i];

    if (in_fence) {
      if (is_fence_line(line)) {
        // Closing fence.
        MarkdownExample example;
        example.command = join_fence(fence_lines);
        example.description = !pending_paragraph.empty()
                                  ? join_paragraph(pending_paragraph)
                                  : join_paragraph(last_paragraph);
        result.examples.push_back(std::move(example));
        in_fence = false;
        fence_lines.clear();
        pending_paragraph.clear();
        last_paragraph.clear();
      } else {
        fence_lines.push_back(line);
      }
      continue;
    }

    if (is_fence_line(line)) {
      in_fence = true;
      fence_lines.clear();
      continue;
    }

    if (is_blank(line)) {
      if (!pending_paragraph.empty()) {
        last_paragraph = pending_paragraph;
        pending_paragraph.clear();
      }
      continue;
    }

    pending_paragraph.push_back(line);
  }

  return result;
}

std::string remove_examples_section(const std::string& markdown) {
  const std::vector<std::string> lines = split_lines(markdown);

  size_t heading_index = 0, body_start = 0, body_end = 0;
  if (!find_examples_section(lines, heading_index, body_start, body_end)) {
    return markdown;
  }

  std::string out;
  for (size_t i = 0; i < lines.size(); ++i) {
    if (i >= heading_index && i < body_end) {
      continue;  // Skip the heading line and the section body.
    }
    if (!out.empty()) {
      out += '\n';
    }
    out += lines[i];
  }
  return out;
}

}  // namespace cli
}  // namespace orc
