/*
 * File:        audio_align_stage.h
 * Module:      orc-stage-plugin-audio_align
 * Purpose:     Per-track audio sync adjustment transform stage (VFrameR) —
 *              shifts one audio track in time relative to the video
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

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace orc {

// ============================================================================
// AlignedAudioTrackRepresentation
// ============================================================================
// Wraps a VideoFrameRepresentation and shifts one audio track in time by a
// whole number of stereo pairs. Video, dropout hints, EFM/AC3 signal data and
// non-targeted audio tracks pass through untouched. A positive pair offset
// delays the audio relative to the video (inserts lead-in silence); a
// negative offset advances it (trims from the start).
//
// Locked tracks: per-frame pair counts are unchanged — each frame's window is
// assembled from the neighbouring frames' samples, silence-filling past
// either end of the frame range, so the track stays spec-conformant locked
// audio. Free-running tracks: the stream origin shifts — silence pairs are
// prepended (positive) or trimmed from the start (negative) and the stream
// pair count adjusts accordingly.
//
// Thread safety: stateless after construction; safe for concurrent reads.
class AlignedAudioTrackRepresentation : public VideoFrameRepresentationWrapper,
                                        public Artifact {
 public:
  // |offset_pairs| is the offset in whole stereo pairs at the target track's
  // exact rational rate (positive = delay audio).
  AlignedAudioTrackRepresentation(
      std::shared_ptr<const VideoFrameRepresentation> source, size_t track,
      int64_t offset_pairs)
      : VideoFrameRepresentationWrapper(std::move(source)),
        Artifact(ArtifactID("audio_align"), Provenance{}),
        target_track_(track),
        offset_pairs_(offset_pairs) {}

  std::string type_name() const override {
    return "aligned_audio_track_representation";
  }

  // --- Audio tracks: only the target track's samples change ----------------
  // (track count, descriptors, and per-frame pair counts forward untouched)

  std::vector<int16_t> get_audio_samples(size_t track,
                                         FrameID id) const override;
  uint64_t get_audio_stream_pair_count(size_t track) const override;
  std::vector<int16_t> get_audio_stream_samples(
      size_t track, uint64_t first_pair, uint32_t pair_count) const override;

 private:
  // Assembles the shifted window of a locked track for frame |id| from the
  // neighbouring frames' samples, silence-filling past either end.
  std::vector<int16_t> assemble_locked_window(FrameID id) const;

  size_t target_track_;
  int64_t offset_pairs_;
};

// ============================================================================
// AudioAlignStage
// ============================================================================
class AudioAlignStage : public DAGStage,
                        public ParameterizedStage,
                        public IStagePreviewCapability {
 public:
  AudioAlignStage();

  std::string version() const override { return "1.0"; }
  ORC_STAGE_INSTRUCTIONS_MD
  NodeTypeInfo get_node_type_info() const override {
    return NodeTypeInfo{NodeType::TRANSFORM,
                        "audio_align",
                        "Audio Align",
                        "Shift one audio track in time relative to the video "
                        "to correct audio/video sync",
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
  double offset_ms_ = 0.0;

  mutable std::shared_ptr<const VideoFrameRepresentation> cached_output_;
};

}  // namespace orc
