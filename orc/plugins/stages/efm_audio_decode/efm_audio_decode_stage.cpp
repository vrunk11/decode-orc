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
#include <cstdint>
#include <limits>
#include <variant>

#include "audio-resample/audio_resampler.h"
#include "efm_audio_decode_stage_deps.h"

namespace orc {

namespace {

// IEC 60908 (CD-DA): EFM decodes to 44.1 kHz 16-bit stereo audio.
constexpr double kCdSampleRateHz = 44100.0;

}  // namespace

// ============================================================================
// EFMAudioChannelPairRepresentation
// ============================================================================

std::optional<AudioChannelPairDescriptor>
EFMAudioChannelPairRepresentation::get_audio_channel_pair_descriptor(
    size_t pair) const {
  if (pair == efm_pair_index()) {
    return AudioChannelPairDescriptor{"EFM digital audio", AudioOrigin::EFM};
  }
  // Out-of-range indices fall through to the source, which answers nullopt.
  return source_ ? source_->get_audio_channel_pair_descriptor(pair)
                 : std::nullopt;
}

std::vector<int32_t> EFMAudioChannelPairRepresentation::get_audio_samples(
    size_t pair, FrameID id) const {
  if (pair != efm_pair_index()) {
    // Out-of-range indices fall through to the source, which answers {}.
    return source_ ? source_->get_audio_samples(pair, id)
                   : std::vector<int32_t>{};
  }

  ensure_decoded();

  const auto params = source_ ? source_->get_video_parameters() : std::nullopt;
  const VideoSystem system = params ? params->system : VideoSystem::Unknown;
  const uint32_t block_pairs = audio_pairs_in_frame(id, system);
  if (block_pairs == 0) return {};

  // The contract requires exactly audio_pairs_in_frame(id, system) pairs:
  // start from silence and overlay whatever the cache holds, so a failed
  // decode or a short cache read still serves a full cadence-sized block.
  std::vector<int32_t> samples(static_cast<size_t>(block_pairs) * 2, 0);
  if (synchronous_audio_ready_) {
    const std::vector<int32_t> cached = deps_->read_synchronous_pairs(
        audio_pair_offset(id, system), block_pairs);
    std::copy_n(cached.begin(), std::min(cached.size(), samples.size()),
                samples.begin());
  }
  return samples;
}

void EFMAudioChannelPairRepresentation::ensure_decoded() const {
  std::call_once(decode_once_, [this] {
    if (!source_) {
      ORC_LOG_ERROR(
          "EFMAudioDecode: no source representation; the EFM audio channel "
          "pair will be silent");
      return;
    }
    const auto params = source_->get_video_parameters();
    const VideoSystem system = params ? params->system : VideoSystem::Unknown;
    const size_t frame_count = source_->frame_count();
    if (system == VideoSystem::Unknown || frame_count == 0) {
      ORC_LOG_ERROR(
          "EFMAudioDecode: unknown video system or empty frame range; the "
          "EFM audio channel pair will be silent");
      return;
    }

    ORC_LOG_INFO("EFMAudioDecode: starting lazy EFM audio decode");
    const EFMAudioDecodeResult decode_result =
        deps_->decode_to_cache(*source_, options_);
    if (!decode_result.success) {
      ORC_LOG_ERROR(
          "EFMAudioDecode: EFM audio decode failed ({}); the EFM audio "
          "channel pair will be silent",
          decode_result.error_message);
      return;
    }
    ORC_LOG_INFO("EFMAudioDecode: decoded {} stereo pairs of EFM audio",
                 decode_result.stream_pair_count);

    // Pull the raw decoded CD audio (44.1 kHz 16-bit stereo) and convert it
    // to the pipeline form: widen to the 24-bit-in-int32 carrier, resample
    // 44100 → 48000 Hz, and segment into cadence-sized per-frame blocks
    // totalling exactly audio_pair_offset(frame_count) pairs.
    const uint32_t raw_pairs = static_cast<uint32_t>(std::min<uint64_t>(
        decode_result.stream_pair_count, std::numeric_limits<uint32_t>::max()));
    const std::vector<int32_t> widened =
        AudioResampler::widen_16_to_24(deps_->read_cache_pairs(0, raw_pairs));
    std::vector<std::vector<int32_t>> frames =
        AudioResampler::resample_to_synchronous(widened, kCdSampleRateHz,
                                                system, frame_count);

    // Flatten the cadence-aligned blocks into the synchronous scratch cache;
    // per-frame serving seeks by audio_pair_offset(id, system).
    std::vector<int32_t> flat;
    flat.reserve(static_cast<size_t>(audio_pair_offset(frame_count, system)) *
                 2);
    for (auto& frame : frames) {
      flat.insert(flat.end(), frame.begin(), frame.end());
      frame.clear();
      frame.shrink_to_fit();
    }
    if (!deps_->write_synchronous_cache(flat)) {
      ORC_LOG_ERROR(
          "EFMAudioDecode: failed to store the converted synchronous audio; "
          "the EFM audio channel pair will be silent");
      return;
    }
    synchronous_audio_ready_ = true;
    ORC_LOG_INFO(
        "EFMAudioDecode: converted EFM audio to {} synchronous 48 kHz stereo "
        "pairs across {} frames",
        flat.size() / 2, frame_count);
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

  // The pair cap is enforced at every pair-adding stage, not just at the
  // container sink, so a DAG that validates also exports.
  if (vfr->audio_channel_pair_count() >= kMaxAudioChannelPairs) {
    throw DAGExecutionError(
        "EFMAudioDecodeStage: cannot append the EFM audio channel pair: the "
        "input already carries the maximum of " +
        std::to_string(kMaxAudioChannelPairs) + " audio channel pairs");
  }

  auto output = process(vfr);
  cached_output_ = output;

  std::vector<ArtifactPtr> outputs;
  outputs.push_back(std::const_pointer_cast<EFMAudioChannelPairRepresentation>(
      std::dynamic_pointer_cast<const EFMAudioChannelPairRepresentation>(
          output)));
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
  return std::make_shared<EFMAudioChannelPairRepresentation>(
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
