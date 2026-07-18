/*
 * File:        tbc_reader.h
 * Module:      orc-stage-plugin-tbc-source
 * Purpose:     TBC binary file reader
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2025-2026 Simon Inns
 */

#pragma once

#include <orc/stage/field_id.h>
#include <orc/support/lru_cache.h>

#include <memory>
#include <string>
#include <vector>

namespace orc {

/**
 * @brief Reader for TBC (Time Base Corrected) video files
 *
 * TBC files contain raw 16-bit video samples organized as sequential fields.
 * Based on legacy ld-decode SourceVideo class.
 */
class TBCReader {
 public:
  using sample_type = uint16_t;

  TBCReader();
  ~TBCReader();

  bool open(const std::string& filename, size_t field_length,
            size_t line_length = 0);
  void close();

  bool is_open() const { return is_open_; }

  std::vector<sample_type> read_field(FieldID field_id);
  std::vector<sample_type> read_field_lines(FieldID field_id, size_t start_line,
                                            size_t end_line);
  std::vector<sample_type> read_line(FieldID field_id, size_t line_number);

 private:
  int fd_;
  std::string filename_;
  bool is_open_;
  size_t field_count_;
  size_t field_length_;
  size_t field_byte_length_;
  size_t line_length_;

  mutable LRUCache<FieldID, std::shared_ptr<std::vector<sample_type>>>
      field_cache_;
};

}  // namespace orc
