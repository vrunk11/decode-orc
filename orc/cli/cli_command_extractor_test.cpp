/*
 * File:        cli_command_extractor_test.cpp
 * Module:      orc-cli-tests
 * Purpose:     Unit tests for the pasted-CLI-command filtergraph extractor.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2025-2026 Simon Inns
 */

#include "cli_command_extractor.h"

#include <gtest/gtest.h>

namespace {

using orc::presenters::extract_filtergraph_from_pasted_command;

TEST(CliCommandExtractor, BareGraphUnchanged) {
  const std::string bare = "tbc_source=input_path=a.tbc, video_sink";
  EXPECT_EQ(extract_filtergraph_from_pasted_command(bare), bare);
}

TEST(CliCommandExtractor, FilterWithDoubleQuotes) {
  const std::string cmd =
      "orc-cli --filter \"tbc_source=input_path=a.tbc, video_sink\"";
  EXPECT_EQ(extract_filtergraph_from_pasted_command(cmd),
            "tbc_source=input_path=a.tbc, video_sink");
}

TEST(CliCommandExtractor, FilterWithSingleQuotes) {
  const std::string cmd =
      "orc-cli --filter 'tbc_source=input_path=a.tbc, video_sink'";
  EXPECT_EQ(extract_filtergraph_from_pasted_command(cmd),
            "tbc_source=input_path=a.tbc, video_sink");
}

TEST(CliCommandExtractor, WindowsExePathPrefix) {
  const std::string cmd =
      "C:\\Users\\me\\decode-orc\\build\\orc-cli.exe --filter "
      "\"tbc_source=input_path=a.tbc, video_sink\"";
  EXPECT_EQ(extract_filtergraph_from_pasted_command(cmd),
            "tbc_source=input_path=a.tbc, video_sink");
}

TEST(CliCommandExtractor, RealWorldExampleWithInternalQuotedPaths) {
  const std::string cmd =
      "orc-cli --filter \"tbc_source=input_path='D:/capture.tbc':"
      "y_path=''[n0]; [n0]hvd_chroma_decoder=output_path='D:/out':passes=1\"";
  const std::string extracted = extract_filtergraph_from_pasted_command(cmd);
  EXPECT_EQ(extracted.find("orc-cli"), std::string::npos);
  EXPECT_EQ(extracted.find("--filter"), std::string::npos);
  EXPECT_EQ(extracted.substr(0, 10), "tbc_source");
  EXPECT_NE(extracted.find("input_path='D:/capture.tbc'"), std::string::npos);
  EXPECT_EQ(extracted.back(), '1');
}

TEST(CliCommandExtractor, TriadInNormalOrder) {
  const std::string cmd =
      "orc-cli --input \"tbc_source=input_path=a.tbc\" --filters "
      "\"hvd_chroma_decoder\" --output \"video_sink=output_path=b.mp4\"";
  EXPECT_EQ(extract_filtergraph_from_pasted_command(cmd),
            "tbc_source=input_path=a.tbc,hvd_chroma_decoder,video_sink=output_"
            "path=b.mp4");
}

TEST(CliCommandExtractor, TriadInDifferentOrderStillComposesCorrectly) {
  const std::string cmd =
      "orc-cli --output \"video_sink\" --input \"tbc_source=input_path=a.tbc\"";
  EXPECT_EQ(extract_filtergraph_from_pasted_command(cmd),
            "tbc_source=input_path=a.tbc,video_sink");
}

TEST(CliCommandExtractor, OnlyOutputFlagPresent) {
  const std::string cmd = "orc-cli --output \"video_sink=output_path=b.mp4\"";
  EXPECT_EQ(extract_filtergraph_from_pasted_command(cmd),
            "video_sink=output_path=b.mp4");
}

TEST(CliCommandExtractor, UnquotedFilterArgument) {
  const std::string cmd =
      "orc-cli --filter tbc_source=input_path=a.tbc,video_sink";
  EXPECT_EQ(extract_filtergraph_from_pasted_command(cmd),
            "tbc_source=input_path=a.tbc,video_sink");
}

TEST(CliCommandExtractor, FiltersFlagNotConfusedWithFilterFlag) {
  const std::string cmd =
      "orc-cli --filters \"hvd_chroma_decoder\" --input "
      "\"tbc_source=input_path=a.tbc\" --output video_sink";
  EXPECT_EQ(extract_filtergraph_from_pasted_command(cmd),
            "tbc_source=input_path=a.tbc,hvd_chroma_decoder,video_sink");
}

TEST(CliCommandExtractor, DotSlashScriptPrefix) {
  const std::string cmd =
      "./orc-cli --filter \"tbc_source=input_path=a.tbc, video_sink\"";
  EXPECT_EQ(extract_filtergraph_from_pasted_command(cmd),
            "tbc_source=input_path=a.tbc, video_sink");
}

TEST(CliCommandExtractor, NoProgramNameJustTheFlag) {
  const std::string cmd =
      "--filter \"tbc_source=input_path=a.tbc, video_sink\"";
  EXPECT_EQ(extract_filtergraph_from_pasted_command(cmd),
            "tbc_source=input_path=a.tbc, video_sink");
}

TEST(CliCommandExtractor, MultiLineWithBackslashContinuation) {
  const std::string cmd =
      "orc-cli --input \"tbc_source=input_path=a.tbc\" \\\n  --output "
      "\"video_sink\"";
  EXPECT_EQ(extract_filtergraph_from_pasted_command(cmd),
            "tbc_source=input_path=a.tbc,video_sink");
}

}  // namespace
