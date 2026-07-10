/*
 * File:        efm_audio_decode_stage.h
 * Module:      orc-stage-plugin-efm_audio_decode
 * Purpose:     EFM audio decode transform stage (VFrameR) — decodes the EFM
 *              t-value stream to CD audio and appends it as a free-running
 *              audio track
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
#include <mutex>

#include "efm_audio_decode_stage_deps_interface.h"

namespace orc {

// ============================================================================
// EFMAudioTrackRepresentation
// ============================================================================
// Wraps a VideoFrameRepresentation and appends one free-running audio track
// carrying the decoded EFM digital audio ({origin: EFM, locked: false,
// 44100/1 Hz}). Video, dropout hints, EFM/AC3 signal data and all existing
// audio tracks pass through untouched.
//
// EFM decode is whole-stream sequential (CIRC interleaving spans sectors), so
// it cannot run per-frame. The decode runs lazily, at most once, on the first
// access to the appended track's stream accessors (std::call_once); decoded
// audio is cached on disk by the deps. Video-only access never pays for the
// decode. Pair 0 of the decoded stream is treated as synchronous with the
// first frame of the wrapped representation.
//
// Thread safety: safe for concurrent reads; the lazy decode is serialised by
// std::call_once and cache reads are stateless.
class EFMAudioTrackRepresentation : public VideoFrameRepresentationWrapper,
                                    public Artifact {
 public:
  EFMAudioTrackRepresentation(
      std::shared_ptr<const VideoFrameRepresentation> source,
      std::shared_ptr<IEFMAudioDecodeDeps> deps,
      const EFMAudioDecodeOptions& options)
      : VideoFrameRepresentationWrapper(std::move(source)),
        Artifact(ArtifactID("efm_audio_track"), Provenance{}),
        deps_(std::move(deps)),
        options_(options) {}

  std::string type_name() const override {
    return "efm_audio_track_representation";
  }

  // --- Audio tracks: source tracks forward; one EFM track is appended ------

  size_t audio_track_count() const override { return source_track_count() + 1; }

  std::optional<AudioTrackDescriptor> get_audio_track_descriptor(
      size_t track) const override;

  // The appended track is free-running: locked per-frame accessors answer
  // 0 / {} for it, per the VFR audio contract.
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
  size_t efm_track_index() const { return source_track_count(); }

  // Runs the whole-stream EFM decode at most once; safe to call from any
  // accessor thread. Failures are logged and leave the track empty.
  void ensure_decoded() const;

  std::shared_ptr<IEFMAudioDecodeDeps> deps_;
  EFMAudioDecodeOptions options_;

  mutable std::once_flag decode_once_;
  mutable EFMAudioDecodeResult decode_result_;
};

// ============================================================================
// EFMAudioDecodeStage
// ============================================================================
class EFMAudioDecodeStage : public DAGStage,
                            public ParameterizedStage,
                            public IStagePreviewCapability {
 public:
  EFMAudioDecodeStage();

  std::string version() const override { return "1.0"; }
  ORC_STAGE_INSTRUCTIONS_MD
  NodeTypeInfo get_node_type_info() const override {
    return NodeTypeInfo{NodeType::TRANSFORM,
                        "efm_audio_decode",
                        "EFM Audio Decode",
                        "Decodes the EFM t-value stream to CD audio and "
                        "appends it as a free-running audio track",
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

  // Test seam: replaces the production decode/cache deps.
  void set_deps_override(std::shared_ptr<IEFMAudioDecodeDeps> deps) {
    deps_override_ = std::move(deps);
  }

 private:
  bool no_timecodes_ = false;
  bool no_audio_concealment_ = false;

  mutable std::shared_ptr<const VideoFrameRepresentation> cached_output_;
  std::shared_ptr<IEFMAudioDecodeDeps> deps_override_;
};

}  // namespace orc
