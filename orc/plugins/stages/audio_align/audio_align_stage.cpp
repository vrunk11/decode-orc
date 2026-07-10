/*
 * File:        audio_align_stage.cpp
 * Module:      orc-stage-plugin-audio_align
 * Purpose:     Per-track audio sync adjustment transform stage (VFrameR)
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 decode-orc contributors
 */

#include "audio_align_stage.h"

#include <orc/stage/error_types.h>
#include <orc/stage/logging.h>
#include <orc/stage/preview_helpers.h>

#include <algorithm>
#include <cmath>
#include <variant>

namespace orc {

// ============================================================================
// AlignedAudioTrackRepresentation
// ============================================================================

std::vector<int16_t> AlignedAudioTrackRepresentation::get_audio_samples(
    size_t track, FrameID id) const {
  if (!source_) return {};
  if (track != target_track_) return source_->get_audio_samples(track, id);
  const auto desc = source_->get_audio_track_descriptor(track);
  if (!desc || !desc->locked) {
    // Free-running tracks answer the locked accessors with {}; forwarding
    // preserves that.
    return source_->get_audio_samples(track, id);
  }
  return assemble_locked_window(id);
}

std::vector<int16_t> AlignedAudioTrackRepresentation::assemble_locked_window(
    FrameID id) const {
  // Locked tracks carry a constant number of stereo pairs per frame (PAL
  // 1764, NTSC/PAL-M 1470), so this frame's own pair count doubles as the
  // global frame-window stride when mapping track positions to frames.
  const int64_t pairs =
      static_cast<int64_t>(source_->get_audio_sample_count(target_track_, id));
  if (pairs <= 0) return {};

  const auto range = source_->frame_range();
  if (id < range.first) return {};
  const int64_t index =
      static_cast<int64_t>(id) - static_cast<int64_t>(range.first);
  const int64_t last_index =
      static_cast<int64_t>(range.last) - static_cast<int64_t>(range.first);

  // A positive offset delays the audio relative to the video, so output
  // position q reads source position q - offset.
  std::vector<int16_t> out(static_cast<size_t>(pairs) * 2, 0);
  int64_t src_pos = index * pairs - offset_pairs_;
  int64_t out_pair = 0;
  while (out_pair < pairs) {
    if (src_pos < 0) {
      // Before the start of the track: leave silence.
      const int64_t skip = std::min(-src_pos, pairs - out_pair);
      src_pos += skip;
      out_pair += skip;
      continue;
    }
    const int64_t src_frame = src_pos / pairs;
    if (src_frame > last_index) break;  // past the end: silence tail
    const int64_t within = src_pos % pairs;
    const int64_t take = std::min(pairs - within, pairs - out_pair);
    const auto src = source_->get_audio_samples(
        target_track_, range.first + static_cast<FrameID>(src_frame));
    for (int64_t p = 0; p < take; ++p) {
      const size_t sp = static_cast<size_t>(within + p) * 2;
      if (sp + 1 < src.size()) {
        out[static_cast<size_t>(out_pair + p) * 2] = src[sp];
        out[static_cast<size_t>(out_pair + p) * 2 + 1] = src[sp + 1];
      }
    }
    src_pos += take;
    out_pair += take;
  }
  return out;
}

uint64_t AlignedAudioTrackRepresentation::get_audio_stream_pair_count(
    size_t track) const {
  if (!source_) return 0;
  const uint64_t total = source_->get_audio_stream_pair_count(track);
  if (track != target_track_) return total;
  const auto desc = source_->get_audio_track_descriptor(track);
  if (!desc || desc->locked) return total;
  if (offset_pairs_ >= 0) {
    return total + static_cast<uint64_t>(offset_pairs_);
  }
  const uint64_t trim = static_cast<uint64_t>(-offset_pairs_);
  return total > trim ? total - trim : 0;
}

std::vector<int16_t> AlignedAudioTrackRepresentation::get_audio_stream_samples(
    size_t track, uint64_t first_pair, uint32_t pair_count) const {
  if (!source_) return {};
  if (track != target_track_) {
    return source_->get_audio_stream_samples(track, first_pair, pair_count);
  }
  const auto desc = source_->get_audio_track_descriptor(track);
  if (!desc || desc->locked) {
    return source_->get_audio_stream_samples(track, first_pair, pair_count);
  }

  if (offset_pairs_ < 0) {
    // Advanced audio: the stream starts |offset_pairs_| pairs in.
    const uint64_t trim = static_cast<uint64_t>(-offset_pairs_);
    return source_->get_audio_stream_samples(track, first_pair + trim,
                                             pair_count);
  }

  // Delayed audio: silence lead-in of offset_pairs_ pairs, then the source
  // stream from position first_pair - offset_pairs_.
  const uint64_t total = get_audio_stream_pair_count(track);
  if (first_pair >= total) return {};
  const uint32_t clamped =
      static_cast<uint32_t>(std::min<uint64_t>(pair_count, total - first_pair));
  const uint64_t lead_in = static_cast<uint64_t>(offset_pairs_);

  std::vector<int16_t> out;
  out.reserve(static_cast<size_t>(clamped) * 2);
  uint32_t remaining = clamped;
  uint64_t pos = first_pair;
  if (pos < lead_in) {
    const uint32_t silence =
        static_cast<uint32_t>(std::min<uint64_t>(lead_in - pos, remaining));
    out.insert(out.end(), static_cast<size_t>(silence) * 2, 0);
    pos += silence;
    remaining -= silence;
  }
  if (remaining > 0) {
    const auto src =
        source_->get_audio_stream_samples(track, pos - lead_in, remaining);
    out.insert(out.end(), src.begin(), src.end());
  }
  return out;
}

// ============================================================================
// AudioAlignStage
// ============================================================================

AudioAlignStage::AudioAlignStage() {
  // Both parameters have complete defaults (zero offset = pass-through).
  set_configuration_status(orc::ConfigurationStatus::Green);
}

std::vector<ArtifactPtr> AudioAlignStage::execute(
    const std::vector<ArtifactPtr>& inputs,
    const std::map<std::string, ParameterValue>& parameters,
    ObservationContext& observation_context) {
  (void)observation_context;
  if (inputs.empty()) {
    throw DAGExecutionError("AudioAlignStage requires one input");
  }
  auto vfr =
      std::dynamic_pointer_cast<const VideoFrameRepresentation>(inputs[0]);
  if (!vfr) {
    throw DAGExecutionError(
        "AudioAlignStage input must be VideoFrameRepresentation");
  }

  if (!parameters.empty()) set_parameters(parameters);

  const size_t track = static_cast<size_t>(track_);
  if (track >= vfr->audio_track_count()) {
    throw DAGExecutionError("AudioAlignStage: track " + std::to_string(track_) +
                            " is out of range: the input carries " +
                            std::to_string(vfr->audio_track_count()) +
                            " audio track(s)");
  }

  auto output = process(vfr);
  cached_output_ = output;

  if (output == vfr) {
    // Zero offset: pass the input artifact through unchanged.
    return {inputs[0]};
  }

  std::vector<ArtifactPtr> outputs;
  outputs.push_back(std::const_pointer_cast<AlignedAudioTrackRepresentation>(
      std::dynamic_pointer_cast<const AlignedAudioTrackRepresentation>(
          output)));
  return outputs;
}

std::shared_ptr<const VideoFrameRepresentation> AudioAlignStage::process(
    std::shared_ptr<const VideoFrameRepresentation> source) const {
  if (!source) return nullptr;

  // Convert the millisecond offset to whole stereo pairs at the target
  // track's exact rational rate.
  const auto desc =
      source->get_audio_track_descriptor(static_cast<size_t>(track_));
  const AudioSampleRate rate =
      (desc && desc->sample_rate.num != 0 && desc->sample_rate.den != 0)
          ? desc->sample_rate
          : kFreeRunningAudioRate;
  const int64_t offset_pairs = static_cast<int64_t>(
      std::llround(offset_ms_ * rate.num / (1000.0 * rate.den)));

  if (offset_pairs == 0) return source;
  return std::make_shared<AlignedAudioTrackRepresentation>(
      std::move(source), static_cast<size_t>(track_), offset_pairs);
}

std::vector<ParameterDescriptor> AudioAlignStage::get_parameter_descriptors(
    VideoSystem project_format, SourceType source_type) const {
  (void)project_format;
  (void)source_type;
  std::vector<ParameterDescriptor> descriptors;

  {
    ParameterDescriptor desc;
    desc.name = "track";
    desc.display_name = "Track";
    desc.description =
        "Audio track to shift (0-based, matching the CVBS container track "
        "numbering)";
    desc.type = ParameterType::INT32;
    desc.constraints.min_value = int32_t{0};
    desc.constraints.max_value = static_cast<int32_t>(kMaxAudioTracks) - 1;
    desc.constraints.default_value = int32_t{0};
    descriptors.push_back(desc);
  }

  {
    ParameterDescriptor desc;
    desc.name = "offset_ms";
    desc.display_name = "Offset (ms)";
    desc.description =
        "Time offset in milliseconds. Positive values delay the audio "
        "relative to the video (insert lead-in); negative values advance it "
        "(trim from the start).";
    desc.type = ParameterType::DOUBLE;
    desc.constraints.default_value = 0.0;
    descriptors.push_back(desc);
  }

  return descriptors;
}

std::map<std::string, ParameterValue> AudioAlignStage::get_parameters() const {
  return {{"track", track_}, {"offset_ms", offset_ms_}};
}

bool AudioAlignStage::set_parameters(
    const std::map<std::string, ParameterValue>& params) {
  for (const auto& [key, value] : params) {
    if (key == "track") {
      const auto* v = std::get_if<int32_t>(&value);
      if (!v || *v < 0 || *v >= static_cast<int32_t>(kMaxAudioTracks)) {
        ORC_LOG_ERROR("AudioAlignStage: invalid track parameter");
        return false;
      }
      track_ = *v;
    } else if (key == "offset_ms") {
      const auto* v = std::get_if<double>(&value);
      if (!v) {
        ORC_LOG_ERROR("AudioAlignStage: offset_ms must be a number");
        return false;
      }
      offset_ms_ = *v;
    } else {
      ORC_LOG_WARN("AudioAlignStage: unknown parameter '{}'", key);
      return false;
    }
  }
  return true;
}

StagePreviewCapability AudioAlignStage::get_preview_capability() const {
  return PreviewHelpers::make_signal_preview_capability(cached_output_);
}

}  // namespace orc
