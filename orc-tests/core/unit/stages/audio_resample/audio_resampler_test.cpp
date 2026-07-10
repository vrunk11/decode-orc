/*
 * File:        audio_resampler_test.cpp
 * Module:      orc-tests/core/unit/stages/audio_resample
 * Purpose:     Unit tests for the shared AudioResampler library
 *              (free-running 44100 Hz ↔ frame-locked rate conversion)
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#include "audio-resample/audio_resampler.h"

#include <gtest/gtest.h>
#include <orc/stage/audio_track.h>
#include <orc/stage/common_types.h>

#include <cstdint>
#include <cstdlib>
#include <vector>

namespace orc {
namespace tests {

namespace {

// Interleaved stereo ramp: pair i carries (i % 1000) in both channels.
std::vector<int16_t> make_ramp_pairs(size_t pair_count) {
  std::vector<int16_t> samples;
  samples.reserve(pair_count * 2);
  for (size_t i = 0; i < pair_count; ++i) {
    const int16_t v = static_cast<int16_t>(i % 1000);
    samples.push_back(v);
    samples.push_back(v);
  }
  return samples;
}

}  // namespace

// ===========================================================================
// resample
// ===========================================================================

TEST(AudioResamplerTest, Resample_EmptyInputReturnsEmpty) {
  EXPECT_TRUE(AudioResampler::resample({}, 44100.0, 48000.0).empty());
}

TEST(AudioResamplerTest, Resample_OutputLengthTracksRateRatio) {
  // 2:1 downsample of one second of audio: expect ~22050 pairs.
  const auto input = make_ramp_pairs(44100);
  const auto output = AudioResampler::resample(input, 44100.0, 22050.0);
  const auto pairs = static_cast<int64_t>(output.size() / 2);
  EXPECT_NEAR(static_cast<double>(pairs), 22050.0, 32.0);
}

TEST(AudioResamplerTest, Resample_PreservesDCLevel) {
  // A constant (DC) signal must stay at the same level through SoXR.
  const std::vector<int16_t> input(44100 * 2, int16_t{1000});
  const auto output =
      AudioResampler::resample(input, AudioResampler::kFreeRunningRateHz,
                               AudioResampler::kNtscLockedRateHz);
  ASSERT_FALSE(output.empty());
  // Sample away from the filter edges.
  const size_t mid = (output.size() / 4) * 2;
  EXPECT_NEAR(static_cast<double>(output[mid]), 1000.0, 2.0);
}

// ===========================================================================
// lock_and_segment
// ===========================================================================

TEST(AudioResamplerTest, LockAndSegment_PAL_IsSampleExactSegmentation) {
  // PAL locked audio is 44100 Hz — no resample, pure 1764-pair slicing.
  const auto input = make_ramp_pairs(1764 * 2);
  const auto frames =
      AudioResampler::lock_and_segment(input, VideoSystem::PAL, 2);
  ASSERT_EQ(frames.size(), 2u);
  ASSERT_EQ(frames[0].size(), 1764u * 2);
  ASSERT_EQ(frames[1].size(), 1764u * 2);
  EXPECT_EQ(frames[0][0], int16_t{0});
  EXPECT_EQ(frames[0][2], int16_t{1});
  // Frame 1 starts at pair 1764 exactly.
  EXPECT_EQ(frames[1][0], int16_t{1764 % 1000});
}

TEST(AudioResamplerTest, LockAndSegment_PAL_ZeroPadsShortFinalFrame) {
  // 1764 + 100 pairs: frame 1 has 100 real pairs then silence.
  const auto input = make_ramp_pairs(1764 + 100);
  const auto frames =
      AudioResampler::lock_and_segment(input, VideoSystem::PAL, 2);
  ASSERT_EQ(frames.size(), 2u);
  ASSERT_EQ(frames[1].size(), 1764u * 2);
  EXPECT_EQ(frames[1][0], int16_t{1764 % 1000});
  EXPECT_EQ(frames[1][100 * 2], int16_t{0});  // padding starts at pair 100
  EXPECT_EQ(frames[1].back(), int16_t{0});
}

TEST(AudioResamplerTest, LockAndSegment_NTSC_Produces1470PairFrames) {
  // Two NTSC frames of free-running audio: 2 × 1471.47 pairs at 44100 Hz.
  const auto input = make_ramp_pairs(2943);
  const auto frames =
      AudioResampler::lock_and_segment(input, VideoSystem::NTSC, 2);
  ASSERT_EQ(frames.size(), 2u);
  EXPECT_EQ(frames[0].size(), 1470u * 2);
  EXPECT_EQ(frames[1].size(), 1470u * 2);
}

TEST(AudioResamplerTest, LockAndSegment_EmptyInputYieldsSilentFrames) {
  const auto frames =
      AudioResampler::lock_and_segment({}, VideoSystem::NTSC, 3);
  ASSERT_EQ(frames.size(), 3u);
  for (const auto& f : frames) {
    ASSERT_EQ(f.size(), 1470u * 2);
    EXPECT_EQ(f[0], int16_t{0});
  }
}

TEST(AudioResamplerTest, LockAndSegment_UnknownSystemYieldsEmptyFrames) {
  const auto input = make_ramp_pairs(1000);
  const auto frames =
      AudioResampler::lock_and_segment(input, VideoSystem::Unknown, 2);
  ASSERT_EQ(frames.size(), 2u);
  EXPECT_TRUE(frames[0].empty());
  EXPECT_TRUE(frames[1].empty());
}

// ===========================================================================
// SDK helpers used by the resampler contract
// ===========================================================================

TEST(AudioTrackHelpersTest, LockedPairsPerFrame_MatchSpec) {
  EXPECT_EQ(locked_audio_pairs_per_frame(VideoSystem::PAL), 1764u);
  EXPECT_EQ(locked_audio_pairs_per_frame(VideoSystem::NTSC), 1470u);
  EXPECT_EQ(locked_audio_pairs_per_frame(VideoSystem::PAL_M), 1470u);
  EXPECT_EQ(locked_audio_pairs_per_frame(VideoSystem::Unknown), 0u);
}

}  // namespace tests
}  // namespace orc
