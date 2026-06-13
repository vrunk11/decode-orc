/*
 * File:        ntsc_tbc_converter_test.cpp
 * Module:      orc-tests/core/unit/stages/tbc_source
 * Purpose:     Unit tests for NtscTBCConverter level mapping and frame assembly,
 *              NtscTBCYCConverter phase alignment, and PalMTBCConverter.
 *
 * Tests:
 *   NtscTBCConverter:
 *   - tbc_to_cvbs level mapping at blanking and white (SMPTE 244M-2003 §4.1)
 *   - assemble_frame sample count equals kNtscFrameSamples = 477,750
 *   - assemble_frame throws on wrong field sizes
 *   - Field 1 (262 lines) appears before field 2 (263 lines) in output
 *   - map_field_phase_to_colour_frame_index: 1→0 (A), 2→1 (B), others→−1
 *
 *   NtscTBCYCConverter:
 *   - Aligned phases return true / empty error string
 *   - Misaligned phases return false / non-empty error string
 *
 *   PalMTBCConverter:
 *   - tbc_to_cvbs maps to NTSC signal levels (kNtscBlanking, kNtscWhite)
 *   - assemble_frame sample count equals kPalMFrameSamples = 477,225
 *   - map_field_phase_to_colour_frame_index: 4-frame cycle 1–4
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 decode-orc contributors
 */

#include "../../../../orc/plugins/stages/tbc_source/ntsc_tbc_converter.h"
#include "../../../../orc/plugins/stages/tbc_source/ntsc_tbc_yc_converter.h"
#include "../../../../orc/plugins/stages/tbc_source/pal_m_tbc_converter.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <optional>
#include <stdexcept>
#include <vector>

#include "../../../../orc/core/include/cvbs_signal_constants.h"

namespace orc_unit_test {

// ============================================================================
// Helpers
// ============================================================================

static std::vector<uint16_t> make_field_u16(int32_t lines, int32_t width,
                                            uint16_t fill) {
  return std::vector<uint16_t>(
      static_cast<size_t>(lines) * static_cast<size_t>(width), fill);
}

// Reference TBC levels (ld-decode NTSC defaults).
constexpr int32_t kRefBlanking = 16384;
constexpr int32_t kRefWhite = 54400;

// NTSC expected field line counts.
constexpr int32_t kNtscF1Lines = orc::kNtscField1Lines;                    // 262
constexpr int32_t kNtscF2Lines = orc::kNtscFrameLines - orc::kNtscField1Lines;  // 263
constexpr int32_t kNtscLineW = orc::kNtscSamplesPerLine;                   // 910

// PAL_M expected field line counts.
constexpr int32_t kPalMF1Lines = orc::kPalMField1Lines;                     // 262
constexpr int32_t kPalMF2Lines = orc::kPalMFrameLines - orc::kPalMField1Lines;  // 263
constexpr int32_t kPalMLineW = orc::kPalMSamplesPerLine;                    // 909

// ============================================================================
// NtscTBCConverter — level mapping
// ============================================================================

TEST(NtscTBCConverterTest, LevelMapping_BlankingInputYieldsCVBSBlanking) {
  // SMPTE 244M-2003: blanking → kNtscBlanking = 240.
  const int16_t result = orc::NtscTBCConverter::tbc_to_cvbs(
      static_cast<uint16_t>(kRefBlanking), kRefBlanking, kRefWhite);
  EXPECT_EQ(result, static_cast<int16_t>(orc::kNtscBlanking));
}

TEST(NtscTBCConverterTest, LevelMapping_WhiteInputYieldsCVBSWhite) {
  // SMPTE 244M-2003: white level → kNtscWhite = 800.
  const int16_t result = orc::NtscTBCConverter::tbc_to_cvbs(
      static_cast<uint16_t>(kRefWhite), kRefBlanking, kRefWhite);
  EXPECT_EQ(result, static_cast<int16_t>(orc::kNtscWhite));
}

TEST(NtscTBCConverterTest, LevelMapping_LinearInterpolationMidpoint) {
  const int32_t tbc_mid = (kRefBlanking + kRefWhite) / 2;
  const int16_t result = orc::NtscTBCConverter::tbc_to_cvbs(
      static_cast<uint16_t>(tbc_mid), kRefBlanking, kRefWhite);
  const int32_t expected = (orc::kNtscBlanking + orc::kNtscWhite) / 2;
  EXPECT_NEAR(result, expected, 1);
}

TEST(NtscTBCConverterTest,
     LevelMapping_BelowBlankingProducesValueBelowCVBSBlanking) {
  const int16_t result = orc::NtscTBCConverter::tbc_to_cvbs(
      static_cast<uint16_t>(kRefBlanking - 1000), kRefBlanking, kRefWhite);
  EXPECT_LT(result, static_cast<int16_t>(orc::kNtscBlanking));
}

// ============================================================================
// NtscTBCConverter — frame assembly
// ============================================================================

TEST(NtscTBCConverterTest, AssembleFrame_OutputSampleCountEqualsKNtscFrameSamples) {
  auto f1 = make_field_u16(kNtscF1Lines, kNtscLineW,
                           static_cast<uint16_t>(kRefBlanking));
  auto f2 = make_field_u16(kNtscF2Lines, kNtscLineW,
                           static_cast<uint16_t>(kRefBlanking));

  const auto frame = orc::NtscTBCConverter::assemble_frame(
      f1, f2, kRefBlanking, kRefWhite);

  EXPECT_EQ(static_cast<int32_t>(frame.size()), orc::kNtscFrameSamples);
}

TEST(NtscTBCConverterTest, AssembleFrame_ThrowsOnWrongField1Size) {
  // Field 1 must be exactly 262 × 910 = 238,220 samples.
  auto bad_f1 = make_field_u16(kNtscF2Lines, kNtscLineW,
                               static_cast<uint16_t>(kRefBlanking));  // 263, wrong
  auto f2 = make_field_u16(kNtscF2Lines, kNtscLineW,
                           static_cast<uint16_t>(kRefBlanking));
  EXPECT_THROW(orc::NtscTBCConverter::assemble_frame(bad_f1, f2, kRefBlanking,
                                                      kRefWhite),
               std::invalid_argument);
}

TEST(NtscTBCConverterTest, AssembleFrame_ThrowsOnWrongField2Size) {
  auto f1 = make_field_u16(kNtscF1Lines, kNtscLineW,
                           static_cast<uint16_t>(kRefBlanking));
  auto bad_f2 = make_field_u16(kNtscF1Lines, kNtscLineW,
                               static_cast<uint16_t>(kRefBlanking));  // 262, wrong
  EXPECT_THROW(orc::NtscTBCConverter::assemble_frame(f1, bad_f2, kRefBlanking,
                                                      kRefWhite),
               std::invalid_argument);
}

TEST(NtscTBCConverterTest, AssembleFrame_Field1IsFirst262Lines) {
  // Fill field 1 with white, field 2 with blanking.
  // The first 262 × 910 samples in the output must be white-level.
  const uint16_t tbc_white = static_cast<uint16_t>(kRefWhite);
  const uint16_t tbc_blank = static_cast<uint16_t>(kRefBlanking);

  auto f1 = make_field_u16(kNtscF1Lines, kNtscLineW, tbc_white);
  auto f2 = make_field_u16(kNtscF2Lines, kNtscLineW, tbc_blank);

  const auto frame = orc::NtscTBCConverter::assemble_frame(
      f1, f2, kRefBlanking, kRefWhite);

  // First sample must be from field 1 (white).
  EXPECT_EQ(frame[0], static_cast<int16_t>(orc::kNtscWhite));

  // First sample of field 2 is at offset 262 × 910 = 238,220.
  const size_t field2_start =
      static_cast<size_t>(kNtscF1Lines) * static_cast<size_t>(kNtscLineW);
  ASSERT_LT(field2_start, frame.size());
  EXPECT_EQ(frame[field2_start], static_cast<int16_t>(orc::kNtscBlanking));
}

TEST(NtscTBCConverterTest, AssembleFrame_NoExtraSamplesInserted) {
  // NTSC is orthogonal: output size = exactly 477,750 with no padding or
  // extra samples.  Verified via: 262×910 + 263×910 = 477,750 = kNtscFrameSamples.
  auto f1 = make_field_u16(kNtscF1Lines, kNtscLineW,
                           static_cast<uint16_t>(kRefBlanking));
  auto f2 = make_field_u16(kNtscF2Lines, kNtscLineW,
                           static_cast<uint16_t>(kRefBlanking));

  const auto frame = orc::NtscTBCConverter::assemble_frame(
      f1, f2, kRefBlanking, kRefWhite);

  EXPECT_EQ(frame.size(),
            static_cast<size_t>(kNtscF1Lines) * static_cast<size_t>(kNtscLineW) +
                static_cast<size_t>(kNtscF2Lines) * static_cast<size_t>(kNtscLineW));
}

// ============================================================================
// NtscTBCConverter — colour frame sequence
// ============================================================================

TEST(NtscTBCConverterTest, ColourFrameIndex_AbsentPhase_ReturnsMinusOne) {
  EXPECT_EQ(orc::NtscTBCConverter::map_field_phase_to_colour_frame_index(
                std::nullopt),
            -1);
}

TEST(NtscTBCConverterTest, ColourFrameIndex_OutOfRange_ReturnsMinusOne) {
  EXPECT_EQ(orc::NtscTBCConverter::map_field_phase_to_colour_frame_index(0),
            -1);
  EXPECT_EQ(orc::NtscTBCConverter::map_field_phase_to_colour_frame_index(3),
            -1);
  EXPECT_EQ(orc::NtscTBCConverter::map_field_phase_to_colour_frame_index(-1),
            -1);
}

TEST(NtscTBCConverterTest, ColourFrameIndex_Phase1_ReturnsZeroFrameA) {
  // SMPTE 244M-2003 §3.2: field_phase_id 1 → frame A → colour_frame_index 0.
  EXPECT_EQ(orc::NtscTBCConverter::map_field_phase_to_colour_frame_index(1),
            0);
}

TEST(NtscTBCConverterTest, ColourFrameIndex_Phase2_ReturnsOneFrameB) {
  // SMPTE 244M-2003 §3.2: field_phase_id 2 → frame B → colour_frame_index 1.
  EXPECT_EQ(orc::NtscTBCConverter::map_field_phase_to_colour_frame_index(2),
            1);
}

// ============================================================================
// NtscTBCYCConverter — phase alignment
// ============================================================================

TEST(NtscTBCYCConverterTest, CheckAlignment_SamePhasesAreAligned) {
  EXPECT_TRUE(orc::NtscTBCYCConverter::check_yc_phase_alignment(0, 0));
  EXPECT_TRUE(orc::NtscTBCYCConverter::check_yc_phase_alignment(1, 1));
}

TEST(NtscTBCYCConverterTest, CheckAlignment_BothUnknownIsAligned) {
  EXPECT_TRUE(orc::NtscTBCYCConverter::check_yc_phase_alignment(-1, -1));
}

TEST(NtscTBCYCConverterTest, CheckAlignment_DifferentPhasesAreMisaligned) {
  EXPECT_FALSE(orc::NtscTBCYCConverter::check_yc_phase_alignment(0, 1));
  EXPECT_FALSE(orc::NtscTBCYCConverter::check_yc_phase_alignment(1, 0));
}

TEST(NtscTBCYCConverterTest, ErrorString_EmptyWhenAligned) {
  EXPECT_EQ(orc::NtscTBCYCConverter::yc_alignment_error(0, 0), "");
  EXPECT_EQ(orc::NtscTBCYCConverter::yc_alignment_error(-1, -1), "");
}

TEST(NtscTBCYCConverterTest, ErrorString_NonEmptyWhenMisaligned) {
  const std::string msg =
      orc::NtscTBCYCConverter::yc_alignment_error(0, 1);
  EXPECT_FALSE(msg.empty());
  // Message should mention NTSC.
  EXPECT_NE(msg.find("NTSC"), std::string::npos);
}

// ============================================================================
// PalMTBCConverter — level mapping
// ============================================================================

TEST(PalMTBCConverterTest, LevelMapping_BlankingInputYieldsCVBSBlanking) {
  // ITU-R BT.1700-1 Annex 1 Part B: PAL_M uses NTSC levels (kNtscBlanking=240).
  const int16_t result = orc::PalMTBCConverter::tbc_to_cvbs(
      static_cast<uint16_t>(kRefBlanking), kRefBlanking, kRefWhite);
  EXPECT_EQ(result, static_cast<int16_t>(orc::kNtscBlanking));
}

TEST(PalMTBCConverterTest, LevelMapping_WhiteInputYieldsCVBSWhite) {
  const int16_t result = orc::PalMTBCConverter::tbc_to_cvbs(
      static_cast<uint16_t>(kRefWhite), kRefBlanking, kRefWhite);
  EXPECT_EQ(result, static_cast<int16_t>(orc::kNtscWhite));
}

// ============================================================================
// PalMTBCConverter — frame assembly
// ============================================================================

TEST(PalMTBCConverterTest, AssembleFrame_OutputSampleCountEqualsKPalMFrameSamples) {
  auto f1 = make_field_u16(kPalMF1Lines, kPalMLineW,
                           static_cast<uint16_t>(kRefBlanking));
  auto f2 = make_field_u16(kPalMF2Lines, kPalMLineW,
                           static_cast<uint16_t>(kRefBlanking));

  const auto frame = orc::PalMTBCConverter::assemble_frame(
      f1, f2, kRefBlanking, kRefWhite);

  EXPECT_EQ(static_cast<int32_t>(frame.size()), orc::kPalMFrameSamples);
}

TEST(PalMTBCConverterTest, AssembleFrame_ThrowsOnWrongFieldSize) {
  auto bad_f1 = make_field_u16(kPalMF2Lines, kPalMLineW,
                               static_cast<uint16_t>(kRefBlanking));
  auto f2 = make_field_u16(kPalMF2Lines, kPalMLineW,
                           static_cast<uint16_t>(kRefBlanking));
  EXPECT_THROW(orc::PalMTBCConverter::assemble_frame(bad_f1, f2, kRefBlanking,
                                                      kRefWhite),
               std::invalid_argument);
}

TEST(PalMTBCConverterTest, AssembleFrame_OrthogonalNoExtraSamples) {
  // PAL_M: 262×909 + 263×909 = 477,225 = kPalMFrameSamples.
  auto f1 = make_field_u16(kPalMF1Lines, kPalMLineW,
                           static_cast<uint16_t>(kRefBlanking));
  auto f2 = make_field_u16(kPalMF2Lines, kPalMLineW,
                           static_cast<uint16_t>(kRefBlanking));

  const auto frame = orc::PalMTBCConverter::assemble_frame(
      f1, f2, kRefBlanking, kRefWhite);

  EXPECT_EQ(frame.size(),
            static_cast<size_t>(kPalMF1Lines) * static_cast<size_t>(kPalMLineW) +
                static_cast<size_t>(kPalMF2Lines) * static_cast<size_t>(kPalMLineW));
}

// ============================================================================
// PalMTBCConverter — colour frame sequence
// ============================================================================

TEST(PalMTBCConverterTest, ColourFrameIndex_AbsentPhase_ReturnsMinusOne) {
  EXPECT_EQ(orc::PalMTBCConverter::map_field_phase_to_colour_frame_index(
                std::nullopt),
            -1);
}

TEST(PalMTBCConverterTest, ColourFrameIndex_Phase1And2_ReturnOne) {
  // ITU-R BT.1700-1 Annex 1 Part B: 4-frame cycle identical to PAL.
  EXPECT_EQ(orc::PalMTBCConverter::map_field_phase_to_colour_frame_index(1), 1);
  EXPECT_EQ(orc::PalMTBCConverter::map_field_phase_to_colour_frame_index(2), 1);
}

TEST(PalMTBCConverterTest, ColourFrameIndex_Phase3And4_ReturnTwo) {
  EXPECT_EQ(orc::PalMTBCConverter::map_field_phase_to_colour_frame_index(3), 2);
  EXPECT_EQ(orc::PalMTBCConverter::map_field_phase_to_colour_frame_index(4), 2);
}

TEST(PalMTBCConverterTest, ColourFrameIndex_Phase5And6_ReturnThree) {
  EXPECT_EQ(orc::PalMTBCConverter::map_field_phase_to_colour_frame_index(5), 3);
  EXPECT_EQ(orc::PalMTBCConverter::map_field_phase_to_colour_frame_index(6), 3);
}

TEST(PalMTBCConverterTest, ColourFrameIndex_Phase7And8_ReturnFour) {
  EXPECT_EQ(orc::PalMTBCConverter::map_field_phase_to_colour_frame_index(7), 4);
  EXPECT_EQ(orc::PalMTBCConverter::map_field_phase_to_colour_frame_index(8), 4);
}

TEST(PalMTBCConverterTest, ColourFrameIndex_OutOfRange_ReturnsMinusOne) {
  EXPECT_EQ(orc::PalMTBCConverter::map_field_phase_to_colour_frame_index(0),
            -1);
  EXPECT_EQ(orc::PalMTBCConverter::map_field_phase_to_colour_frame_index(9),
            -1);
}

}  // namespace orc_unit_test
