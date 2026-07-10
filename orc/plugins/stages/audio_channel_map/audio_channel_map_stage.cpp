/*
 * File:        audio_channel_map_stage.cpp
 * Module:      orc-stage-plugin-audio_channel_map
 * Purpose:     Audio channel routing transform stage (VFrameR)
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 decode-orc contributors
 */

#include "audio_channel_map_stage.h"

#include <orc/stage/error_types.h>
#include <orc/stage/logging.h>
#include <orc/stage/preview_helpers.h>

#include <utility>
#include <variant>

namespace orc {

namespace {

constexpr const char* kOpSplitDualMono = "split_dual_mono";
constexpr const char* kOpLeftToBoth = "left_to_both";
constexpr const char* kOpRightToBoth = "right_to_both";
constexpr const char* kOpSwapChannels = "swap_channels";

std::optional<AudioChannelMapOperation> parse_operation(
    const std::string& name) {
  if (name == kOpSplitDualMono) return AudioChannelMapOperation::kSplitDualMono;
  if (name == kOpLeftToBoth) return AudioChannelMapOperation::kLeftToBoth;
  if (name == kOpRightToBoth) return AudioChannelMapOperation::kRightToBoth;
  if (name == kOpSwapChannels) return AudioChannelMapOperation::kSwapChannels;
  return std::nullopt;
}

}  // namespace

// ============================================================================
// ChannelMappedRepresentation
// ============================================================================

size_t ChannelMappedRepresentation::audio_track_count() const {
  return source_track_count() + (is_split() ? 1 : 0);
}

size_t ChannelMappedRepresentation::source_track_for(size_t track) const {
  if (is_split() && track == appended_track_index()) return target_track_;
  return track;
}

ChannelMappedRepresentation::ChannelFill
ChannelMappedRepresentation::fill_for_track(size_t track) const {
  switch (operation_) {
    case AudioChannelMapOperation::kSplitDualMono:
      if (track == target_track_) return ChannelFill::kBothFromLeft;
      if (track == appended_track_index()) return ChannelFill::kBothFromRight;
      return ChannelFill::kNone;
    case AudioChannelMapOperation::kLeftToBoth:
      return track == target_track_ ? ChannelFill::kBothFromLeft
                                    : ChannelFill::kNone;
    case AudioChannelMapOperation::kRightToBoth:
      return track == target_track_ ? ChannelFill::kBothFromRight
                                    : ChannelFill::kNone;
    case AudioChannelMapOperation::kSwapChannels:
      return track == target_track_ ? ChannelFill::kSwap : ChannelFill::kNone;
  }
  return ChannelFill::kNone;
}

void ChannelMappedRepresentation::apply_fill(std::vector<int16_t>& samples,
                                             ChannelFill fill) {
  if (fill == ChannelFill::kNone) return;
  // Interleaved stereo pairs (L, R, L, R, …); ignore a trailing odd sample.
  const size_t pairs = samples.size() / 2;
  switch (fill) {
    case ChannelFill::kBothFromLeft:
      for (size_t p = 0; p < pairs; ++p) {
        samples[p * 2 + 1] = samples[p * 2];
      }
      break;
    case ChannelFill::kBothFromRight:
      for (size_t p = 0; p < pairs; ++p) {
        samples[p * 2] = samples[p * 2 + 1];
      }
      break;
    case ChannelFill::kSwap:
      for (size_t p = 0; p < pairs; ++p) {
        std::swap(samples[p * 2], samples[p * 2 + 1]);
      }
      break;
    case ChannelFill::kNone:
      break;
  }
}

std::optional<AudioTrackDescriptor>
ChannelMappedRepresentation::get_audio_track_descriptor(size_t track) const {
  if (!source_) return std::nullopt;
  if (is_split() &&
      (track == target_track_ || track == appended_track_index())) {
    auto desc = source_->get_audio_track_descriptor(target_track_);
    if (!desc) return std::nullopt;
    desc->name += (track == target_track_) ? " (L)" : " (R)";
    desc->origin = AudioTrackOrigin::DERIVED;
    return desc;
  }
  if (track >= audio_track_count()) return std::nullopt;
  return source_->get_audio_track_descriptor(track);
}

uint32_t ChannelMappedRepresentation::get_audio_sample_count(size_t track,
                                                             FrameID id) const {
  if (!source_ || track >= audio_track_count()) return 0;
  return source_->get_audio_sample_count(source_track_for(track), id);
}

std::vector<int16_t> ChannelMappedRepresentation::get_audio_samples(
    size_t track, FrameID id) const {
  if (!source_ || track >= audio_track_count()) return {};
  auto samples = source_->get_audio_samples(source_track_for(track), id);
  apply_fill(samples, fill_for_track(track));
  return samples;
}

uint64_t ChannelMappedRepresentation::get_audio_stream_pair_count(
    size_t track) const {
  if (!source_ || track >= audio_track_count()) return 0;
  return source_->get_audio_stream_pair_count(source_track_for(track));
}

std::vector<int16_t> ChannelMappedRepresentation::get_audio_stream_samples(
    size_t track, uint64_t first_pair, uint32_t pair_count) const {
  if (!source_ || track >= audio_track_count()) return {};
  auto samples = source_->get_audio_stream_samples(source_track_for(track),
                                                   first_pair, pair_count);
  apply_fill(samples, fill_for_track(track));
  return samples;
}

// ============================================================================
// AudioChannelMapStage
// ============================================================================

AudioChannelMapStage::AudioChannelMapStage() {
  // Both parameters have complete defaults.
  set_configuration_status(orc::ConfigurationStatus::Green);
}

std::vector<ArtifactPtr> AudioChannelMapStage::execute(
    const std::vector<ArtifactPtr>& inputs,
    const std::map<std::string, ParameterValue>& parameters,
    ObservationContext& observation_context) {
  (void)observation_context;
  if (inputs.empty()) {
    throw DAGExecutionError("AudioChannelMapStage requires one input");
  }
  auto vfr =
      std::dynamic_pointer_cast<const VideoFrameRepresentation>(inputs[0]);
  if (!vfr) {
    throw DAGExecutionError(
        "AudioChannelMapStage input must be VideoFrameRepresentation");
  }

  if (!parameters.empty()) set_parameters(parameters);

  const size_t track = static_cast<size_t>(track_);
  if (track >= vfr->audio_track_count()) {
    throw DAGExecutionError(
        "AudioChannelMapStage: track " + std::to_string(track_) +
        " is out of range: the input carries " +
        std::to_string(vfr->audio_track_count()) + " audio track(s)");
  }

  // The track cap is enforced at every track-adding stage, not just at the
  // container sink, so a DAG that validates also exports.
  if (operation_ == kOpSplitDualMono &&
      vfr->audio_track_count() >= kMaxAudioTracks) {
    throw DAGExecutionError("AudioChannelMapStage: cannot split track " +
                            std::to_string(track_) +
                            ": the input already carries the maximum of " +
                            std::to_string(kMaxAudioTracks) + " audio tracks");
  }

  auto output = process(vfr);
  cached_output_ = output;

  std::vector<ArtifactPtr> outputs;
  outputs.push_back(std::const_pointer_cast<ChannelMappedRepresentation>(
      std::dynamic_pointer_cast<const ChannelMappedRepresentation>(output)));
  return outputs;
}

std::shared_ptr<const VideoFrameRepresentation> AudioChannelMapStage::process(
    std::shared_ptr<const VideoFrameRepresentation> source) const {
  if (!source) return nullptr;
  const auto operation = parse_operation(operation_);
  return std::make_shared<ChannelMappedRepresentation>(
      std::move(source), static_cast<size_t>(track_),
      operation.value_or(AudioChannelMapOperation::kSplitDualMono));
}

std::vector<ParameterDescriptor>
AudioChannelMapStage::get_parameter_descriptors(VideoSystem project_format,
                                                SourceType source_type) const {
  (void)project_format;
  (void)source_type;
  std::vector<ParameterDescriptor> descriptors;

  {
    ParameterDescriptor desc;
    desc.name = "track";
    desc.display_name = "Track";
    desc.description =
        "Audio track the operation applies to (0-based, matching the CVBS "
        "container track numbering)";
    desc.type = ParameterType::INT32;
    desc.constraints.min_value = int32_t{0};
    desc.constraints.max_value = static_cast<int32_t>(kMaxAudioTracks) - 1;
    desc.constraints.default_value = int32_t{0};
    descriptors.push_back(desc);
  }

  {
    ParameterDescriptor desc;
    desc.name = "operation";
    desc.display_name = "Operation";
    desc.description =
        "Channel routing operation: 'split_dual_mono' replaces the track "
        "with '<name> (L)' in place and appends '<name> (R)' as a new track "
        "(bilingual/dual-mono material); 'left_to_both' / 'right_to_both' "
        "copy one channel to both in place; 'swap_channels' exchanges left "
        "and right in place.";
    desc.type = ParameterType::STRING;
    desc.constraints.default_value = std::string(kOpSplitDualMono);
    desc.constraints.allowed_strings = {kOpSplitDualMono, kOpLeftToBoth,
                                        kOpRightToBoth, kOpSwapChannels};
    descriptors.push_back(desc);
  }

  return descriptors;
}

std::map<std::string, ParameterValue> AudioChannelMapStage::get_parameters()
    const {
  return {{"track", track_}, {"operation", operation_}};
}

bool AudioChannelMapStage::set_parameters(
    const std::map<std::string, ParameterValue>& params) {
  for (const auto& [key, value] : params) {
    if (key == "track") {
      const auto* v = std::get_if<int32_t>(&value);
      if (!v || *v < 0 || *v >= static_cast<int32_t>(kMaxAudioTracks)) {
        ORC_LOG_ERROR("AudioChannelMapStage: invalid track parameter");
        return false;
      }
      track_ = *v;
    } else if (key == "operation") {
      const auto* v = std::get_if<std::string>(&value);
      if (!v || !parse_operation(*v)) {
        ORC_LOG_ERROR("AudioChannelMapStage: unknown operation '{}'",
                      v ? *v : std::string("<non-string>"));
        return false;
      }
      operation_ = *v;
    } else {
      ORC_LOG_WARN("AudioChannelMapStage: unknown parameter '{}'", key);
      return false;
    }
  }
  return true;
}

StagePreviewCapability AudioChannelMapStage::get_preview_capability() const {
  return PreviewHelpers::make_signal_preview_capability(cached_output_);
}

}  // namespace orc
