/*
 * File:        audio_channel_map_stage.h
 * Module:      orc-stage-plugin-audio_channel_map
 * Purpose:     Audio channel routing transform stage (VFrameR) — dual-mono
 *              split, mono fill, and channel swap for a target audio
 *              channel pair
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 decode-orc contributors
 */

#pragma once

#include <orc/plugin/orc_stage_preview.h>
#include <orc/plugin/orc_stage_runtime.h>
#include <orc/stage/artifact.h>
#include <orc/stage/audio_channel_pair.h>
#include <orc/stage/stage_parameter.h>
#include <orc/stage/video_frame_representation.h>

#include <memory>
#include <string>
#include <vector>

namespace orc {

// One channel-routing operation applied to a single target channel pair.
enum class AudioChannelMapOperation {
  kSplitDualMono,  // target → "<name> (L)" in place + "<name> (R)" appended
  kLeftToBoth,     // in-place: left channel copied to both channels
  kRightToBoth,    // in-place: right channel copied to both channels
  kSwapChannels,   // in-place: left and right channels exchanged
};

// ============================================================================
// ChannelMappedRepresentation
// ============================================================================
// Wraps a VideoFrameRepresentation and applies one channel-routing operation
// to a single target audio channel pair. Video, dropout hints, EFM/AC3 signal
// data and non-targeted channel pairs pass through untouched.
//
// split_dual_mono replaces the target pair in place with the left-channel
// mono pair ("<name> (L)") and appends the right-channel mono pair
// ("<name> (R)") as the last pair, so all other pair indices are stable.
// Both derived pairs report origin DERIVED.
//
// The remap is a pure per-sample channel operation on the synchronous
// 24-bit-in-int32 stereo pairs; timing and per-frame pair counts are
// untouched.
//
// Thread safety: stateless after construction; safe for concurrent reads.
class ChannelMappedRepresentation : public VideoFrameRepresentationWrapper,
                                    public Artifact {
 public:
  ChannelMappedRepresentation(
      std::shared_ptr<const VideoFrameRepresentation> source,
      size_t channel_pair, AudioChannelMapOperation operation)
      : VideoFrameRepresentationWrapper(std::move(source)),
        Artifact(ArtifactID("audio_channel_map"), Provenance{}),
        target_pair_(channel_pair),
        operation_(operation) {}

  std::string type_name() const override {
    return "channel_mapped_representation";
  }

  // --- Audio channel pairs --------------------------------------------------

  size_t audio_channel_pair_count() const override;
  std::optional<AudioChannelPairDescriptor> get_audio_channel_pair_descriptor(
      size_t pair) const override;
  std::vector<int32_t> get_audio_samples(size_t pair,
                                         FrameID id) const override;

 private:
  // How the channels of an output pair derive from its source pair.
  enum class ChannelFill {
    kNone,           // pass through unchanged
    kBothFromLeft,   // L → both channels
    kBothFromRight,  // R → both channels
    kSwap,           // exchange L and R
  };

  size_t source_pair_count() const {
    return source_ ? source_->audio_channel_pair_count() : 0;
  }
  bool is_split() const {
    return operation_ == AudioChannelMapOperation::kSplitDualMono;
  }
  // Index of the appended "(R)" channel pair in split mode.
  size_t appended_pair_index() const { return source_pair_count(); }

  // Source pair index serving output pair |pair| (appended → target).
  size_t source_pair_for(size_t pair) const;
  ChannelFill fill_for_pair(size_t pair) const;

  // Applies |fill| in place to interleaved stereo samples.
  static void apply_fill(std::vector<int32_t>& samples, ChannelFill fill);

  size_t target_pair_;
  AudioChannelMapOperation operation_;
};

// ============================================================================
// AudioChannelMapStage
// ============================================================================
class AudioChannelMapStage : public DAGStage,
                             public ParameterizedStage,
                             public IStagePreviewCapability {
 public:
  AudioChannelMapStage();

  std::string version() const override { return "1.0"; }
  ORC_STAGE_INSTRUCTIONS_MD
  NodeTypeInfo get_node_type_info() const override {
    return NodeTypeInfo{NodeType::TRANSFORM,
                        "audio_channel_map",
                        "Audio Channel Map",
                        "Route audio channels of one channel pair: split "
                        "dual-mono into two channel pairs, fill both channels "
                        "from one, or swap channels",
                        1,
                        1,
                        1,
                        UINT32_MAX,
                        VideoFormatCompatibility::ALL,
                        SinkCategory::CORE,
                        "Transform"};
  }

  std::vector<ArtifactPtr> execute(
      const std::vector<ArtifactPtr>& inputs,
      const std::map<std::string, ParameterValue>& parameters,
      ObservationContext& observation_context) override;

  size_t required_input_count() const override { return 1; }
  size_t output_count() const override { return 1; }

  std::shared_ptr<const VideoFrameRepresentation> process(
      std::shared_ptr<const VideoFrameRepresentation> source) const;

  // ParameterizedStage
  std::vector<ParameterDescriptor> get_parameter_descriptors(
      VideoSystem project_format, SourceType source_type) const override;
  using ParameterizedStage::get_parameter_descriptors;
  std::map<std::string, ParameterValue> get_parameters() const override;
  bool set_parameters(
      const std::map<std::string, ParameterValue>& params) override;

  // IStagePreviewCapability
  StagePreviewCapability get_preview_capability() const override;

 private:
  int32_t channel_pair_ = 0;
  std::string operation_ = "split_dual_mono";

  mutable std::shared_ptr<const VideoFrameRepresentation> cached_output_;
};

}  // namespace orc
