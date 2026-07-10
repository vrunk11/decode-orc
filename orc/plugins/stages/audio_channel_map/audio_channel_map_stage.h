/*
 * File:        audio_channel_map_stage.h
 * Module:      orc-stage-plugin-audio_channel_map
 * Purpose:     Audio channel routing transform stage (VFrameR) — dual-mono
 *              split, mono fill, and channel swap for a target audio track
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 decode-orc contributors
 */

#pragma once

#include <orc/plugin/orc_stage_preview.h>
#include <orc/plugin/orc_stage_runtime.h>
#include <orc/stage/artifact.h>
#include <orc/stage/audio_track.h>
#include <orc/stage/stage_parameter.h>
#include <orc/stage/video_frame_representation.h>

#include <memory>
#include <string>
#include <vector>

namespace orc {

// One channel-routing operation applied to a single target track.
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
// to a single target audio track. Video, dropout hints, EFM/AC3 signal data
// and non-targeted audio tracks pass through untouched.
//
// split_dual_mono replaces the target track in place with the left-channel
// mono track ("<name> (L)") and appends the right-channel mono track
// ("<name> (R)") as the last track, so all other track indices are stable.
// Both derived tracks report origin DERIVED and inherit the target's lock
// state and sample rate.
//
// The remap is a pure per-sample channel operation, so it works identically
// on locked (per-frame) and free-running (stream) tracks.
//
// Thread safety: stateless after construction; safe for concurrent reads.
class ChannelMappedRepresentation : public VideoFrameRepresentationWrapper,
                                    public Artifact {
 public:
  ChannelMappedRepresentation(
      std::shared_ptr<const VideoFrameRepresentation> source, size_t track,
      AudioChannelMapOperation operation)
      : VideoFrameRepresentationWrapper(std::move(source)),
        Artifact(ArtifactID("audio_channel_map"), Provenance{}),
        target_track_(track),
        operation_(operation) {}

  std::string type_name() const override {
    return "channel_mapped_representation";
  }

  // --- Audio tracks ---------------------------------------------------------

  size_t audio_track_count() const override;
  std::optional<AudioTrackDescriptor> get_audio_track_descriptor(
      size_t track) const override;
  uint32_t get_audio_sample_count(size_t track, FrameID id) const override;
  std::vector<int16_t> get_audio_samples(size_t track,
                                         FrameID id) const override;
  uint64_t get_audio_stream_pair_count(size_t track) const override;
  std::vector<int16_t> get_audio_stream_samples(
      size_t track, uint64_t first_pair, uint32_t pair_count) const override;

 private:
  // How the channels of an output track derive from its source track.
  enum class ChannelFill {
    kNone,           // pass through unchanged
    kBothFromLeft,   // L → both channels
    kBothFromRight,  // R → both channels
    kSwap,           // exchange L and R
  };

  size_t source_track_count() const {
    return source_ ? source_->audio_track_count() : 0;
  }
  bool is_split() const {
    return operation_ == AudioChannelMapOperation::kSplitDualMono;
  }
  // Index of the appended "(R)" track in split mode.
  size_t appended_track_index() const { return source_track_count(); }

  // Source track index serving output track |track| (appended → target).
  size_t source_track_for(size_t track) const;
  ChannelFill fill_for_track(size_t track) const;

  // Applies |fill| in place to interleaved stereo samples.
  static void apply_fill(std::vector<int16_t>& samples, ChannelFill fill);

  size_t target_track_;
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
                        "Route audio channels of one track: split dual-mono "
                        "into two tracks, fill both channels from one, or "
                        "swap channels",
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
  int32_t track_ = 0;
  std::string operation_ = "split_dual_mono";

  mutable std::shared_ptr<const VideoFrameRepresentation> cached_output_;
};

}  // namespace orc
