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

size_t ChannelMappedRepresentation::audio_channel_pair_count() const {
  return source_pair_count() + (is_split() ? 1 : 0);
}

size_t ChannelMappedRepresentation::source_pair_for(size_t pair) const {
  if (is_split() && pair == appended_pair_index()) return target_pair_;
  return pair;
}

ChannelMappedRepresentation::ChannelFill
ChannelMappedRepresentation::fill_for_pair(size_t pair) const {
  switch (operation_) {
    case AudioChannelMapOperation::kSplitDualMono:
      if (pair == target_pair_) return ChannelFill::kBothFromLeft;
      if (pair == appended_pair_index()) return ChannelFill::kBothFromRight;
      return ChannelFill::kNone;
    case AudioChannelMapOperation::kLeftToBoth:
      return pair == target_pair_ ? ChannelFill::kBothFromLeft
                                  : ChannelFill::kNone;
    case AudioChannelMapOperation::kRightToBoth:
      return pair == target_pair_ ? ChannelFill::kBothFromRight
                                  : ChannelFill::kNone;
    case AudioChannelMapOperation::kSwapChannels:
      return pair == target_pair_ ? ChannelFill::kSwap : ChannelFill::kNone;
  }
  return ChannelFill::kNone;
}

void ChannelMappedRepresentation::apply_fill(std::vector<int32_t>& samples,
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

std::optional<AudioChannelPairDescriptor>
ChannelMappedRepresentation::get_audio_channel_pair_descriptor(
    size_t pair) const {
  if (!source_) return std::nullopt;
  if (is_split() && (pair == target_pair_ || pair == appended_pair_index())) {
    auto desc = source_->get_audio_channel_pair_descriptor(target_pair_);
    if (!desc) return std::nullopt;
    desc->name += (pair == target_pair_) ? " (L)" : " (R)";
    desc->origin = AudioOrigin::DERIVED;
    return desc;
  }
  if (pair >= audio_channel_pair_count()) return std::nullopt;
  return source_->get_audio_channel_pair_descriptor(pair);
}

std::vector<int32_t> ChannelMappedRepresentation::get_audio_samples(
    size_t pair, FrameID id) const {
  if (!source_ || pair >= audio_channel_pair_count()) return {};
  auto samples = source_->get_audio_samples(source_pair_for(pair), id);
  apply_fill(samples, fill_for_pair(pair));
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

  const size_t pair = static_cast<size_t>(channel_pair_);
  if (pair >= vfr->audio_channel_pair_count()) {
    throw DAGExecutionError("AudioChannelMapStage: channel pair " +
                            std::to_string(channel_pair_) +
                            " is out of range: the input carries " +
                            std::to_string(vfr->audio_channel_pair_count()) +
                            " audio channel pair(s)");
  }

  // The channel-pair cap is enforced at every pair-adding stage, not just at
  // the container sink, so a DAG that validates also exports.
  if (operation_ == kOpSplitDualMono &&
      vfr->audio_channel_pair_count() >= kMaxAudioChannelPairs) {
    throw DAGExecutionError("AudioChannelMapStage: cannot split channel pair " +
                            std::to_string(channel_pair_) +
                            ": the input already carries the maximum of " +
                            std::to_string(kMaxAudioChannelPairs) +
                            " audio channel pairs");
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
      std::move(source), static_cast<size_t>(channel_pair_),
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
    desc.name = "channel_pair";
    desc.display_name = "Channel pair";
    desc.description =
        "Audio channel pair the operation applies to (0-based, matching the "
        "CVBS container channel-pair numbering)";
    desc.type = ParameterType::INT32;
    desc.constraints.min_value = int32_t{0};
    desc.constraints.max_value =
        static_cast<int32_t>(kMaxAudioChannelPairs) - 1;
    desc.constraints.default_value = int32_t{0};
    descriptors.push_back(desc);
  }

  {
    ParameterDescriptor desc;
    desc.name = "operation";
    desc.display_name = "Operation";
    desc.description =
        "Channel routing operation: 'split_dual_mono' replaces the channel "
        "pair with '<name> (L)' in place and appends '<name> (R)' as a new "
        "channel pair (bilingual/dual-mono material); 'left_to_both' / "
        "'right_to_both' copy one channel to both in place; 'swap_channels' "
        "exchanges left and right in place.";
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
  return {{"channel_pair", channel_pair_}, {"operation", operation_}};
}

bool AudioChannelMapStage::set_parameters(
    const std::map<std::string, ParameterValue>& params) {
  for (const auto& [key, value] : params) {
    if (key == "channel_pair") {
      const auto* v = std::get_if<int32_t>(&value);
      if (!v || *v < 0 || *v >= static_cast<int32_t>(kMaxAudioChannelPairs)) {
        ORC_LOG_ERROR("AudioChannelMapStage: invalid channel_pair parameter");
        return false;
      }
      channel_pair_ = *v;
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
