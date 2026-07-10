/*
 * File:        audio_track_import_stage.h
 * Module:      orc-stage-plugin-audio_track_import
 * Purpose:     External WAV import transform stage (VFrameR) — attaches an
 *              external WAV file as a new audio track
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

#include "audio_track_import_stage_deps_interface.h"

namespace orc {

// ============================================================================
// ImportedAudioTrackRepresentation
// ============================================================================
// Wraps a VideoFrameRepresentation and appends one audio track served from an
// external WAV file ({origin: IMPORTED}). Video, dropout hints, EFM/AC3
// signal data and all existing audio tracks pass through untouched.
//
// A locked track is served per frame — each frame's window is the WAV slice
// [index × pairs_per_frame, (index + 1) × pairs_per_frame), silence-filled
// past end-of-file. A free-running track is served through the stream
// accessors, pair 0 synchronous with the wrapped representation's first
// frame (the CVBS spec origin convention).
//
// Thread safety: stateless after construction; safe for concurrent reads
// (deps reads are stateless).
class ImportedAudioTrackRepresentation : public VideoFrameRepresentationWrapper,
                                         public Artifact {
 public:
  ImportedAudioTrackRepresentation(
      std::shared_ptr<const VideoFrameRepresentation> source,
      std::shared_ptr<IAudioTrackImportDeps> deps,
      AudioTrackDescriptor descriptor, uint64_t wav_pair_count,
      uint32_t pairs_per_frame)
      : VideoFrameRepresentationWrapper(std::move(source)),
        Artifact(ArtifactID("audio_track_import"), Provenance{}),
        deps_(std::move(deps)),
        descriptor_(std::move(descriptor)),
        wav_pair_count_(wav_pair_count),
        pairs_per_frame_(pairs_per_frame) {}

  std::string type_name() const override {
    return "imported_audio_track_representation";
  }

  // --- Audio tracks: source tracks forward; one track is appended ----------

  size_t audio_track_count() const override { return source_track_count() + 1; }
  std::optional<AudioTrackDescriptor> get_audio_track_descriptor(
      size_t track) const override;
  uint32_t get_audio_sample_count(size_t track, FrameID id) const override;
  std::vector<int16_t> get_audio_samples(size_t track,
                                         FrameID id) const override;
  uint64_t get_audio_stream_pair_count(size_t track) const override;
  std::vector<int16_t> get_audio_stream_samples(
      size_t track, uint64_t first_pair, uint32_t pair_count) const override;

 private:
  size_t source_track_count() const {
    return source_ ? source_->audio_track_count() : 0;
  }
  // The appended track is always the last index (track order is stable
  // through the DAG; track-adding stages append).
  size_t imported_track_index() const { return source_track_count(); }

  std::shared_ptr<IAudioTrackImportDeps> deps_;
  AudioTrackDescriptor descriptor_;
  uint64_t wav_pair_count_;
  uint32_t pairs_per_frame_;  // locked window stride; 0 for free-running
};

// ============================================================================
// AudioTrackImportStage
// ============================================================================
class AudioTrackImportStage : public DAGStage,
                              public ParameterizedStage,
                              public IStagePreviewCapability {
 public:
  AudioTrackImportStage();

  std::string version() const override { return "1.0"; }
  ORC_STAGE_INSTRUCTIONS_MD
  NodeTypeInfo get_node_type_info() const override {
    return NodeTypeInfo{NodeType::TRANSFORM,
                        "audio_track_import",
                        "Audio Track Import",
                        "Attach an external WAV file as a new audio track",
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

  // ParameterizedStage
  std::vector<ParameterDescriptor> get_parameter_descriptors(
      VideoSystem project_format, SourceType source_type) const override;
  using ParameterizedStage::get_parameter_descriptors;
  std::map<std::string, ParameterValue> get_parameters() const override;
  bool set_parameters(
      const std::map<std::string, ParameterValue>& params) override;

  // IStagePreviewCapability
  StagePreviewCapability get_preview_capability() const override;

  // Test seam: replaces the production WAV file deps.
  void set_deps_override(std::shared_ptr<IAudioTrackImportDeps> deps) {
    deps_override_ = std::move(deps);
  }

 private:
  std::string wav_path_;
  std::string track_name_;
  std::string lock_mode_ = "auto";

  mutable std::shared_ptr<const VideoFrameRepresentation> cached_output_;
  std::shared_ptr<IAudioTrackImportDeps> deps_override_;
};

}  // namespace orc
