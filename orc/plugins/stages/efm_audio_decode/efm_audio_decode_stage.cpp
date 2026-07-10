/*
 * File:        efm_audio_decode_stage.cpp
 * Module:      orc-stage-plugin-efm_audio_decode
 * Purpose:     EFM audio decode transform stage (VFrameR)
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 decode-orc contributors
 */

#include "efm_audio_decode_stage.h"

#include <orc/stage/error_types.h>
#include <orc/stage/logging.h>
#include <orc/stage/preview_helpers.h>

#include <algorithm>
#include <variant>

#include "efm_audio_decode_stage_deps.h"

namespace orc {

// ============================================================================
// EFMAudioTrackRepresentation
// ============================================================================

std::optional<AudioTrackDescriptor>
EFMAudioTrackRepresentation::get_audio_track_descriptor(size_t track) const {
  if (track == efm_track_index()) {
    return AudioTrackDescriptor{"EFM digital audio", AudioTrackOrigin::EFM,
                                /*locked=*/false, kFreeRunningAudioRate};
  }
  return source_ ? source_->get_audio_track_descriptor(track) : std::nullopt;
}

uint32_t EFMAudioTrackRepresentation::get_audio_sample_count(size_t track,
                                                             FrameID id) const {
  if (track == efm_track_index()) return 0;  // free-running track
  return source_ ? source_->get_audio_sample_count(track, id) : 0;
}

std::vector<int16_t> EFMAudioTrackRepresentation::get_audio_samples(
    size_t track, FrameID id) const {
  if (track == efm_track_index()) return {};  // free-running track
  return source_ ? source_->get_audio_samples(track, id)
                 : std::vector<int16_t>{};
}

uint64_t EFMAudioTrackRepresentation::get_audio_stream_pair_count(
    size_t track) const {
  if (track != efm_track_index()) {
    return source_ ? source_->get_audio_stream_pair_count(track) : 0;
  }
  ensure_decoded();
  return decode_result_.success ? decode_result_.stream_pair_count : 0;
}

std::vector<int16_t> EFMAudioTrackRepresentation::get_audio_stream_samples(
    size_t track, uint64_t first_pair, uint32_t pair_count) const {
  if (track != efm_track_index()) {
    return source_ ? source_->get_audio_stream_samples(track, first_pair,
                                                       pair_count)
                   : std::vector<int16_t>{};
  }
  ensure_decoded();
  if (!decode_result_.success ||
      first_pair >= decode_result_.stream_pair_count) {
    return {};
  }
  const uint64_t available = decode_result_.stream_pair_count - first_pair;
  const uint32_t clamped_count =
      static_cast<uint32_t>(std::min<uint64_t>(pair_count, available));
  return deps_->read_cache_pairs(first_pair, clamped_count);
}

void EFMAudioTrackRepresentation::ensure_decoded() const {
  std::call_once(decode_once_, [this] {
    if (!source_) {
      decode_result_ = {false, "no source representation", 0};
      return;
    }
    ORC_LOG_INFO("EFMAudioDecode: starting lazy EFM audio decode");
    decode_result_ = deps_->decode_to_cache(*source_, options_);
    if (decode_result_.success) {
      ORC_LOG_INFO("EFMAudioDecode: decoded {} stereo pairs of EFM audio",
                   decode_result_.stream_pair_count);
    } else {
      ORC_LOG_ERROR(
          "EFMAudioDecode: EFM audio decode failed ({}); the EFM audio track "
          "will be empty",
          decode_result_.error_message);
    }
  });
}

// ============================================================================
// EFMAudioDecodeStage
// ============================================================================

EFMAudioDecodeStage::EFMAudioDecodeStage() {
  // Both parameters are optional with safe defaults.
  set_configuration_status(orc::ConfigurationStatus::Green);
}

std::vector<ArtifactPtr> EFMAudioDecodeStage::execute(
    const std::vector<ArtifactPtr>& inputs,
    const std::map<std::string, ParameterValue>& parameters,
    ObservationContext& observation_context) {
  (void)observation_context;
  if (inputs.empty()) {
    throw DAGExecutionError("EFMAudioDecodeStage requires one input");
  }
  auto vfr =
      std::dynamic_pointer_cast<const VideoFrameRepresentation>(inputs[0]);
  if (!vfr) {
    throw DAGExecutionError(
        "EFMAudioDecodeStage input must be VideoFrameRepresentation");
  }

  if (!parameters.empty()) set_parameters(parameters);

  // The track cap is enforced at every track-adding stage, not just at the
  // container sink, so a DAG that validates also exports.
  if (vfr->audio_track_count() >= kMaxAudioTracks) {
    throw DAGExecutionError(
        "EFMAudioDecodeStage: cannot append the EFM audio track: the input "
        "already carries the maximum of " +
        std::to_string(kMaxAudioTracks) + " audio tracks");
  }

  auto output = process(vfr);
  cached_output_ = output;

  std::vector<ArtifactPtr> outputs;
  outputs.push_back(std::const_pointer_cast<EFMAudioTrackRepresentation>(
      std::dynamic_pointer_cast<const EFMAudioTrackRepresentation>(output)));
  return outputs;
}

std::shared_ptr<const VideoFrameRepresentation> EFMAudioDecodeStage::process(
    std::shared_ptr<const VideoFrameRepresentation> source) const {
  if (!source) return nullptr;
  auto deps = deps_override_ ? deps_override_
                             : std::static_pointer_cast<IEFMAudioDecodeDeps>(
                                   std::make_shared<EFMAudioDecodeDeps>());
  EFMAudioDecodeOptions options;
  options.no_timecodes = no_timecodes_;
  options.no_audio_concealment = no_audio_concealment_;
  return std::make_shared<EFMAudioTrackRepresentation>(
      std::move(source), std::move(deps), options);
}

std::vector<ParameterDescriptor> EFMAudioDecodeStage::get_parameter_descriptors(
    VideoSystem project_format, SourceType source_type) const {
  (void)project_format;
  (void)source_type;
  std::vector<ParameterDescriptor> descriptors;

  {
    ParameterDescriptor desc;
    desc.name = "no_timecodes";
    desc.display_name = "No Timecodes";
    desc.description =
        "Disable timecode verification during decode. Needed for early CAV "
        "discs that pre-date the EFM timecode specification.";
    desc.type = ParameterType::BOOL;
    desc.constraints.default_value = false;
    descriptors.push_back(desc);
  }

  {
    ParameterDescriptor desc;
    desc.name = "no_audio_concealment";
    desc.display_name = "Disable Audio Concealment";
    desc.description = "Disable interpolation-based audio error concealment";
    desc.type = ParameterType::BOOL;
    desc.constraints.default_value = false;
    descriptors.push_back(desc);
  }

  return descriptors;
}

std::map<std::string, ParameterValue> EFMAudioDecodeStage::get_parameters()
    const {
  return {{"no_timecodes", no_timecodes_},
          {"no_audio_concealment", no_audio_concealment_}};
}

bool EFMAudioDecodeStage::set_parameters(
    const std::map<std::string, ParameterValue>& params) {
  const auto get_bool = [&params](const std::string& name, bool current) {
    const auto it = params.find(name);
    if (it == params.end()) return current;
    if (const bool* b = std::get_if<bool>(&it->second)) return *b;
    return current;
  };
  no_timecodes_ = get_bool("no_timecodes", no_timecodes_);
  no_audio_concealment_ =
      get_bool("no_audio_concealment", no_audio_concealment_);
  return true;
}

StagePreviewCapability EFMAudioDecodeStage::get_preview_capability() const {
  return PreviewHelpers::make_signal_preview_capability(cached_output_);
}

}  // namespace orc
