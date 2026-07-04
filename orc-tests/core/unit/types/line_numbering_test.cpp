/*
 * File:        line_numbering_test.cpp
 * Module:      orc-core-tests
 * Purpose:     Unit tests for line_numbering.h conversion utilities
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#include <gtest/gtest.h>
#include <line_numbering.h>

using namespace orc;

// ============================================================================
// kFrameFlat0Based — all systems return the raw index as a string
// ============================================================================

TEST(LineNumbering_FrameFlat0Based, PAL_FirstLine_IsZero) {
  auto label =
      make_line_label(0, VideoSystem::PAL, LineNumberingMode::kFrameFlat0Based);
  EXPECT_EQ(label.display, "0");
}

TEST(LineNumbering_FrameFlat0Based, PAL_LastLine_Is624) {
  auto label = make_line_label(624, VideoSystem::PAL,
                               LineNumberingMode::kFrameFlat0Based);
  EXPECT_EQ(label.display, "624");
}

TEST(LineNumbering_FrameFlat0Based, NTSC_LastLine_Is524) {
  auto label = make_line_label(524, VideoSystem::NTSC,
                               LineNumberingMode::kFrameFlat0Based);
  EXPECT_EQ(label.display, "524");
}

// ============================================================================
// kFrameSequential1Based — 0-based → 1-based offset
// ============================================================================

TEST(LineNumbering_FrameSequential1Based, PAL_FirstLine_IsOne) {
  auto label = make_line_label(0, VideoSystem::PAL,
                               LineNumberingMode::kFrameSequential1Based);
  EXPECT_EQ(label.display, "1");
}

TEST(LineNumbering_FrameSequential1Based, PAL_LastLine_Is625) {
  auto label = make_line_label(624, VideoSystem::PAL,
                               LineNumberingMode::kFrameSequential1Based);
  EXPECT_EQ(label.display, "625");
}

TEST(LineNumbering_FrameSequential1Based, NTSC_FirstLine_IsOne) {
  auto label = make_line_label(0, VideoSystem::NTSC,
                               LineNumberingMode::kFrameSequential1Based);
  EXPECT_EQ(label.display, "1");
}

TEST(LineNumbering_FrameSequential1Based, NTSC_LastLine_Is525) {
  auto label = make_line_label(524, VideoSystem::NTSC,
                               LineNumberingMode::kFrameSequential1Based);
  EXPECT_EQ(label.display, "525");
}

// ============================================================================
// kFieldRelative — field and line_in_field populated
// ============================================================================

TEST(LineNumbering_FieldRelative, PAL_FrameLine0_IsField1Line1) {
  auto label =
      make_line_label(0, VideoSystem::PAL, LineNumberingMode::kFieldRelative);
  EXPECT_EQ(label.field, 1);
  EXPECT_EQ(label.line_in_field, 1);
  EXPECT_EQ(label.display, "F1L1");
}

TEST(LineNumbering_FieldRelative, PAL_LastLineOfField1_IsField1Line313) {
  // frame_line 312 is the last line of PAL field 1 (0-based, 313 lines)
  auto label =
      make_line_label(312, VideoSystem::PAL, LineNumberingMode::kFieldRelative);
  EXPECT_EQ(label.field, 1);
  EXPECT_EQ(label.line_in_field, 313);
}

TEST(LineNumbering_FieldRelative, PAL_FirstLineOfField2_IsField2Line1) {
  // frame_line 313 is the first line of PAL field 2
  auto label =
      make_line_label(313, VideoSystem::PAL, LineNumberingMode::kFieldRelative);
  EXPECT_EQ(label.field, 2);
  EXPECT_EQ(label.line_in_field, 1);
  EXPECT_EQ(label.display, "F2L1");
}

TEST(LineNumbering_FieldRelative, PAL_LastLineOfFrame_IsField2Line312) {
  // frame_line 624 = field 2, line 312 (1-based), 0-based: 624 - 313 = 311 → +1
  // = 312
  auto label =
      make_line_label(624, VideoSystem::PAL, LineNumberingMode::kFieldRelative);
  EXPECT_EQ(label.field, 2);
  EXPECT_EQ(label.line_in_field, 312);
}

TEST(LineNumbering_FieldRelative, NTSC_FrameLine0_IsField1Line1) {
  auto label =
      make_line_label(0, VideoSystem::NTSC, LineNumberingMode::kFieldRelative);
  EXPECT_EQ(label.field, 1);
  EXPECT_EQ(label.line_in_field, 1);
}

TEST(LineNumbering_FieldRelative, NTSC_LastLineOfField1_IsField1Line262) {
  // frame_line 261 = last line of NTSC field 1 (262 lines)
  auto label = make_line_label(261, VideoSystem::NTSC,
                               LineNumberingMode::kFieldRelative);
  EXPECT_EQ(label.field, 1);
  EXPECT_EQ(label.line_in_field, 262);
}

TEST(LineNumbering_FieldRelative, NTSC_FirstLineOfField2_IsField2Line1) {
  // VFR field 1 has 263 lines (0-based 0..262), so field 2 starts at frame_line
  // 263.
  auto label = make_line_label(263, VideoSystem::NTSC,
                               LineNumberingMode::kFieldRelative);
  EXPECT_EQ(label.field, 2);
  EXPECT_EQ(label.line_in_field, 1);
}

TEST(LineNumbering_FieldRelative, NTSC_LastLineOfFrame_IsField2Line262) {
  // VFR field 2 has 262 lines (indices 263..524): frame_line 524 → line
  // 524-263+1 = 262.
  auto label = make_line_label(524, VideoSystem::NTSC,
                               LineNumberingMode::kFieldRelative);
  EXPECT_EQ(label.field, 2);
  EXPECT_EQ(label.line_in_field, 262);
}

TEST(LineNumbering_FieldRelative, PALM_FirstLineOfField2_IsField2Line1) {
  // PAL_M: same field structure as NTSC (263 lines in VFR field 1; field 2
  // starts at line 263).
  auto label = make_line_label(263, VideoSystem::PAL_M,
                               LineNumberingMode::kFieldRelative);
  EXPECT_EQ(label.field, 2);
  EXPECT_EQ(label.line_in_field, 1);
}

// ============================================================================
// kBroadcastInterlaced — PAL
// ============================================================================
// ITU-R BT.470-6 §1.1: PAL field 1 → odd broadcast lines (1, 3, 5, …, 625)
//                       PAL field 2 → even broadcast lines (2, 4, 6, …, 624)

TEST(LineNumbering_BroadcastInterlaced_PAL, FrameLine0_IsBroadcast1) {
  // frame_line 0 = field 1, line 0 → broadcast 1
  auto label = make_line_label(0, VideoSystem::PAL,
                               LineNumberingMode::kBroadcastInterlaced);
  EXPECT_EQ(label.broadcast_line, 1);
  EXPECT_EQ(label.display, "1");
}

TEST(LineNumbering_BroadcastInterlaced_PAL, FrameLine1_IsBroadcast3) {
  // frame_line 1 = field 1, line 1 → broadcast 3
  auto label = make_line_label(1, VideoSystem::PAL,
                               LineNumberingMode::kBroadcastInterlaced);
  EXPECT_EQ(label.broadcast_line, 3);
}

TEST(LineNumbering_BroadcastInterlaced_PAL, LastField1Line_IsBroadcast625) {
  // frame_line 312 = field 1, line 312 → broadcast 1 + 312*2 = 625
  auto label = make_line_label(312, VideoSystem::PAL,
                               LineNumberingMode::kBroadcastInterlaced);
  EXPECT_EQ(label.broadcast_line, 625);
}

TEST(LineNumbering_BroadcastInterlaced_PAL, FirstField2Line_IsBroadcast2) {
  // frame_line 313 = field 2, line 0 → broadcast 2
  auto label = make_line_label(313, VideoSystem::PAL,
                               LineNumberingMode::kBroadcastInterlaced);
  EXPECT_EQ(label.broadcast_line, 2);
}

TEST(LineNumbering_BroadcastInterlaced_PAL, LastFrame_IsBroadcast624) {
  // frame_line 624 = field 2, line 311 → broadcast 2 + 311*2 = 624
  auto label = make_line_label(624, VideoSystem::PAL,
                               LineNumberingMode::kBroadcastInterlaced);
  EXPECT_EQ(label.broadcast_line, 624);
}

// ============================================================================
// kBroadcastInterlaced — NTSC
// ============================================================================
// SMPTE 170M-2004 §11.3: NTSC field 1 → even broadcast lines (2, 4, …, 524)
//                         NTSC field 2 → odd broadcast lines (1, 3, …, 525)

TEST(LineNumbering_BroadcastInterlaced_NTSC, FrameLine0_IsBroadcast1) {
  // VFR field 1 (0..262) → odd broadcast: frame_line 0 → 1 + 0*2 = 1.
  auto label = make_line_label(0, VideoSystem::NTSC,
                               LineNumberingMode::kBroadcastInterlaced);
  EXPECT_EQ(label.broadcast_line, 1);
}

TEST(LineNumbering_BroadcastInterlaced_NTSC, FirstField2Line_IsBroadcast2) {
  // VFR field 2 (263..524) → even broadcast: frame_line 263 → 2 + (263-263)*2
  // = 2.
  auto label = make_line_label(263, VideoSystem::NTSC,
                               LineNumberingMode::kBroadcastInterlaced);
  EXPECT_EQ(label.broadcast_line, 2);
}

TEST(LineNumbering_BroadcastInterlaced_NTSC, LastField1Line_IsBroadcast525) {
  // VFR field 1 has 263 lines (0..262): frame_line 262 → 1 + 262*2 = 525.
  auto label = make_line_label(262, VideoSystem::NTSC,
                               LineNumberingMode::kBroadcastInterlaced);
  EXPECT_EQ(label.broadcast_line, 525);
}

TEST(LineNumbering_BroadcastInterlaced_NTSC, LastFrameLine_IsBroadcast524) {
  // VFR field 2 (263..524): frame_line 524 → 2 + (524-263)*2 = 2 + 261*2 = 524.
  auto label = make_line_label(524, VideoSystem::NTSC,
                               LineNumberingMode::kBroadcastInterlaced);
  EXPECT_EQ(label.broadcast_line, 524);
}

// ============================================================================
// broadcast_line_to_frame_line — round-trip sanity
// ============================================================================

TEST(LineNumbering_BroadcastToFrameLine, Line1_MapsToFrameLine0) {
  EXPECT_EQ(broadcast_line_to_frame_line(1), 0u);
}

TEST(LineNumbering_BroadcastToFrameLine, Line625_MapsToFrameLine624) {
  EXPECT_EQ(broadcast_line_to_frame_line(625), 624u);
}

TEST(LineNumbering_BroadcastToFrameLine, Line525_MapsToFrameLine524) {
  EXPECT_EQ(broadcast_line_to_frame_line(525), 524u);
}

// ============================================================================
// Switching mode does not alter the internal frame_line
// ============================================================================

TEST(LineNumbering_ModeSwitching, PAL_SameLine_DifferentModes_AreConsistent) {
  // frame_line 313 (first of PAL field 2) under each mode
  constexpr size_t kLine = 313;

  auto flat = make_line_label(kLine, VideoSystem::PAL,
                              LineNumberingMode::kFrameFlat0Based);
  auto seq = make_line_label(kLine, VideoSystem::PAL,
                             LineNumberingMode::kFrameSequential1Based);
  auto field_rel = make_line_label(kLine, VideoSystem::PAL,
                                   LineNumberingMode::kFieldRelative);
  auto bcast = make_line_label(kLine, VideoSystem::PAL,
                               LineNumberingMode::kBroadcastInterlaced);

  EXPECT_EQ(flat.display, "313");
  EXPECT_EQ(seq.display, "314");
  EXPECT_EQ(field_rel.field, 2);
  EXPECT_EQ(field_rel.line_in_field, 1);
  EXPECT_EQ(bcast.broadcast_line, 2);
}
