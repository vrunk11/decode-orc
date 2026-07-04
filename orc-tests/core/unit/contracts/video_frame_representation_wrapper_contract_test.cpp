/*
 * File:        video_frame_representation_wrapper_contract_test.cpp
 * Module:      orc-core-tests
 * Purpose:     Contract tests: VideoFrameRepresentationWrapper must expose a
 *              stage's OUTPUT on every read path; no accessor may bypass a
 *              derived override and reach unprocessed upstream data.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 decode-orc contributors
 */

#include <gtest/gtest.h>
#include <orc/stage/frame_line_util.h>
#include <orc/stage/video_frame_representation.h>

#include <algorithm>
#include <memory>
#include <vector>

namespace orc_unit_test {

namespace {

using orc::FrameDescriptor;
using orc::FrameID;
using orc::FrameIDRange;
using orc::SourceParameters;
using orc::VideoFrameRepresentation;
using orc::VideoFrameRepresentationWrapper;
using orc::VideoSystem;
using sample_type = orc::VideoFrameRepresentation::sample_type;

constexpr size_t kWidth = 16;
constexpr size_t kHeight = 8;
constexpr size_t kSamplesTotal = kWidth * kHeight;

// In-memory single-frame source with distinct composite / luma / chroma data.
// NTSC geometry keeps every line at kWidth samples.
class FakeSource : public VideoFrameRepresentation {
 public:
  explicit FakeSource(bool separate_channels = false)
      : separate_channels_(separate_channels) {
    frame_.resize(kSamplesTotal);
    luma_.resize(kSamplesTotal);
    chroma_.resize(kSamplesTotal);
    for (size_t i = 0; i < kSamplesTotal; ++i) {
      frame_[i] = static_cast<sample_type>(i);
      luma_[i] = static_cast<sample_type>(i + 1000);
      chroma_[i] = static_cast<sample_type>(i + 2000);
    }
  }

  FrameIDRange frame_range() const override { return {FrameID{0}, FrameID{0}}; }
  size_t frame_count() const override { return 1; }
  bool has_frame(FrameID id) const override { return id == FrameID{0}; }

  std::optional<FrameDescriptor> get_frame_descriptor(
      FrameID id) const override {
    if (!has_frame(id)) return std::nullopt;
    FrameDescriptor desc;
    desc.frame_id = id;
    desc.system = VideoSystem::NTSC;
    desc.height = kHeight;
    desc.samples_total = kSamplesTotal;
    desc.samples_per_line_nominal = kWidth;
    return desc;
  }

  const sample_type* get_frame(FrameID id) const override {
    return has_frame(id) ? frame_.data() : nullptr;
  }
  std::vector<sample_type> get_frame_copy(FrameID id) const override {
    return has_frame(id) ? frame_ : std::vector<sample_type>{};
  }

  bool has_separate_channels() const override { return separate_channels_; }
  const sample_type* get_frame_luma(FrameID id) const override {
    return (separate_channels_ && has_frame(id)) ? luma_.data() : nullptr;
  }
  const sample_type* get_frame_chroma(FrameID id) const override {
    return (separate_channels_ && has_frame(id)) ? chroma_.data() : nullptr;
  }

  std::optional<SourceParameters> get_video_parameters() const override {
    SourceParameters params;
    params.system = VideoSystem::NTSC;
    params.frame_width_nominal = static_cast<int32_t>(kWidth);
    params.frame_height = static_cast<int32_t>(kHeight);
    params.number_of_sequential_frames = 1;
    return params;
  }

 private:
  bool separate_channels_;
  std::vector<sample_type> frame_;
  std::vector<sample_type> luma_;
  std::vector<sample_type> chroma_;
};

// Transform wrapper that overrides ONLY the frame-level primitives, adding a
// fixed offset to every sample — the minimal implementation the wrapper
// contract requires of a data-modifying stage.
class OffsetTransform : public VideoFrameRepresentationWrapper {
 public:
  OffsetTransform(std::shared_ptr<const VideoFrameRepresentation> source,
                  sample_type offset)
      : VideoFrameRepresentationWrapper(std::move(source)), offset_(offset) {}

  const sample_type* get_frame(FrameID id) const override {
    return transformed(cached_frame_, source_->get_frame(id));
  }
  const sample_type* get_frame_luma(FrameID id) const override {
    return transformed(cached_luma_, source_->get_frame_luma(id));
  }
  const sample_type* get_frame_chroma(FrameID id) const override {
    return transformed(cached_chroma_, source_->get_frame_chroma(id));
  }

 private:
  const sample_type* transformed(std::vector<sample_type>& cache,
                                 const sample_type* input) const {
    if (!input) return nullptr;
    cache.resize(kSamplesTotal);
    std::transform(input, input + kSamplesTotal, cache.begin(),
                   [this](sample_type s) {
                     return static_cast<sample_type>(s + offset_);
                   });
    return cache.data();
  }

  sample_type offset_;
  mutable std::vector<sample_type> cached_frame_;
  mutable std::vector<sample_type> cached_luma_;
  mutable std::vector<sample_type> cached_chroma_;
};

// Wrapper with no overrides at all — a pure pass-through stage.
class PassThrough : public VideoFrameRepresentationWrapper {
 public:
  explicit PassThrough(std::shared_ptr<const VideoFrameRepresentation> source)
      : VideoFrameRepresentationWrapper(std::move(source)) {}
};

}  // namespace

TEST(VFRWrapperContractTest, GetLineSamples_ReflectsOverriddenGetFrame) {
  auto source = std::make_shared<FakeSource>();
  OffsetTransform transform(source, 100);

  for (size_t line = 0; line < kHeight; ++line) {
    const auto samples = transform.get_line_samples(FrameID{0}, line);
    ASSERT_EQ(samples.size(), kWidth) << "line " << line;
    for (size_t i = 0; i < kWidth; ++i) {
      EXPECT_EQ(samples[i], static_cast<sample_type>(line * kWidth + i + 100))
          << "line " << line << " sample " << i;
    }
  }
}

TEST(VFRWrapperContractTest, GetLine_ReflectsOverriddenGetFrame) {
  auto source = std::make_shared<FakeSource>();
  OffsetTransform transform(source, 100);

  const sample_type* line = transform.get_line(FrameID{0}, 3);
  ASSERT_NE(line, nullptr);
  EXPECT_EQ(line[0], static_cast<sample_type>(3 * kWidth + 100));
  EXPECT_EQ(line[kWidth - 1],
            static_cast<sample_type>(3 * kWidth + kWidth - 1 + 100));
}

TEST(VFRWrapperContractTest, GetFrameCopy_ReflectsOverriddenGetFrame) {
  auto source = std::make_shared<FakeSource>();
  OffsetTransform transform(source, 100);

  const auto copy = transform.get_frame_copy(FrameID{0});
  ASSERT_EQ(copy.size(), kSamplesTotal);
  EXPECT_EQ(copy.front(), static_cast<sample_type>(100));
  EXPECT_EQ(copy.back(), static_cast<sample_type>(kSamplesTotal - 1 + 100));
}

TEST(VFRWrapperContractTest, GetLineLumaChroma_ReflectOverriddenFrameLevelYC) {
  auto source = std::make_shared<FakeSource>(/*separate_channels=*/true);
  OffsetTransform transform(source, 100);

  const sample_type* luma = transform.get_line_luma(FrameID{0}, 2);
  ASSERT_NE(luma, nullptr);
  EXPECT_EQ(luma[0], static_cast<sample_type>(2 * kWidth + 1000 + 100));

  const sample_type* chroma = transform.get_line_chroma(FrameID{0}, 2);
  ASSERT_NE(chroma, nullptr);
  EXPECT_EQ(chroma[0], static_cast<sample_type>(2 * kWidth + 2000 + 100));
}

TEST(VFRWrapperContractTest, ChainedTransforms_ApplyEveryStageOnLineReads) {
  // Miniature of a real pipeline (source → stacker → dropout correction):
  // reads at the tail wrapper must include every stage's transform.
  auto source = std::make_shared<FakeSource>();
  auto first = std::make_shared<OffsetTransform>(source, 100);
  OffsetTransform second(first, 10);

  const auto samples = second.get_line_samples(FrameID{0}, 1);
  ASSERT_EQ(samples.size(), kWidth);
  EXPECT_EQ(samples[0], static_cast<sample_type>(kWidth + 110));
}

TEST(VFRWrapperContractTest, PassThroughWrapper_ForwardsSourceDataUnchanged) {
  auto source = std::make_shared<FakeSource>(/*separate_channels=*/true);
  PassThrough wrapper(source);

  EXPECT_EQ(wrapper.frame_count(), 1u);
  EXPECT_TRUE(wrapper.has_frame(FrameID{0}));

  const auto samples = wrapper.get_line_samples(FrameID{0}, 1);
  ASSERT_EQ(samples.size(), kWidth);
  EXPECT_EQ(samples[0], static_cast<sample_type>(kWidth));

  const auto copy = wrapper.get_frame_copy(FrameID{0});
  ASSERT_EQ(copy.size(), kSamplesTotal);
  EXPECT_EQ(copy.back(), static_cast<sample_type>(kSamplesTotal - 1));

  const sample_type* luma = wrapper.get_line_luma(FrameID{0}, 1);
  ASSERT_NE(luma, nullptr);
  EXPECT_EQ(luma[0], static_cast<sample_type>(kWidth + 1000));
}

TEST(VFRWrapperContractTest, MissingFrame_ReturnsEmptyOnDerivedAccessors) {
  auto source = std::make_shared<FakeSource>();
  OffsetTransform transform(source, 100);

  EXPECT_EQ(transform.get_line(FrameID{5}, 0), nullptr);
  EXPECT_TRUE(transform.get_line_samples(FrameID{5}, 0).empty());
  EXPECT_TRUE(transform.get_frame_copy(FrameID{5}).empty());
  EXPECT_EQ(transform.get_line_luma(FrameID{5}, 0), nullptr);
}

TEST(VFRWrapperContractTest, OutOfRangeLine_ReturnsEmptyOnDerivedAccessors) {
  auto source = std::make_shared<FakeSource>(/*separate_channels=*/true);
  OffsetTransform transform(source, 100);

  EXPECT_EQ(transform.get_line(FrameID{0}, kHeight), nullptr);
  EXPECT_TRUE(transform.get_line_samples(FrameID{0}, kHeight).empty());
  EXPECT_EQ(transform.get_line_luma(FrameID{0}, kHeight), nullptr);
  EXPECT_EQ(transform.get_line_chroma(FrameID{0}, kHeight), nullptr);
}

}  // namespace orc_unit_test
