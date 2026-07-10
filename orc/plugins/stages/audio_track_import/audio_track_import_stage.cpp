/*
 * File:        audio_track_import_stage.cpp
 * Module:      orc-stage-plugin-audio_track_import
 * Purpose:     External WAV import transform stage (VFrameR)
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 decode-orc contributors
 */

#include "audio_track_import_stage.h"

#include <orc/stage/error_types.h>
#include <orc/stage/logging.h>
#include <orc/stage/preview_helpers.h>

#include <algorithm>
#include <variant>

#include "audio_track_import_stage_deps.h"

namespace orc {

namespace {

constexpr const char* kLockModeAuto = "auto";
constexpr const char* kLockModeLocked = "locked";
constexpr const char* kLockModeFreeRunning = "free_running";

// Nearest integer to a rational sample rate — what a WAV header can carry
// (e.g. NTSC/PAL-M locked 44100000/1001 Hz → 44056).
uint32_t nearest_integer_rate(AudioSampleRate rate) {
  if (rate.den == 0) return 0;
  return static_cast<uint32_t>(
      (static_cast<uint64_t>(rate.num) + rate.den / 2) / rate.den);
}

// "path/to/My Track.wav" → "My Track"; empty input yields the fallback.
std::string file_stem(const std::string& path) {
  const size_t sep = path.find_last_of("/\\");
  std::string name = (sep == std::string::npos) ? path : path.substr(sep + 1);
  const size_t dot = name.find_last_of('.');
  if (dot != std::string::npos && dot > 0) name = name.substr(0, dot);
  return name.empty() ? "Imported audio" : name;
}

}  // namespace

// ============================================================================
// ImportedAudioTrackRepresentation
// ============================================================================

std::optional<AudioTrackDescriptor>
ImportedAudioTrackRepresentation::get_audio_track_descriptor(
    size_t track) const {
  if (track == imported_track_index()) return descriptor_;
  return source_ ? source_->get_audio_track_descriptor(track) : std::nullopt;
}

uint32_t ImportedAudioTrackRepresentation::get_audio_sample_count(
    size_t track, FrameID id) const {
  if (track != imported_track_index()) {
    return source_ ? source_->get_audio_sample_count(track, id) : 0;
  }
  if (!descriptor_.locked || pairs_per_frame_ == 0 || !source_ ||
      !source_->has_frame(id)) {
    return 0;
  }
  return pairs_per_frame_;
}

std::vector<int16_t> ImportedAudioTrackRepresentation::get_audio_samples(
    size_t track, FrameID id) const {
  if (track != imported_track_index()) {
    return source_ ? source_->get_audio_samples(track, id)
                   : std::vector<int16_t>{};
  }
  if (!descriptor_.locked || pairs_per_frame_ == 0 || !source_ ||
      !source_->has_frame(id)) {
    return {};
  }
  const auto range = source_->frame_range();
  if (id < range.first) return {};
  const uint64_t index = id - range.first;

  // The frame's window is a direct slice of the WAV; anything past
  // end-of-file is silence so every frame stays exactly pairs_per_frame_.
  std::vector<int16_t> samples =
      deps_->read_pairs(index * pairs_per_frame_, pairs_per_frame_);
  samples.resize(static_cast<size_t>(pairs_per_frame_) * 2, 0);
  return samples;
}

uint64_t ImportedAudioTrackRepresentation::get_audio_stream_pair_count(
    size_t track) const {
  if (track != imported_track_index()) {
    return source_ ? source_->get_audio_stream_pair_count(track) : 0;
  }
  return descriptor_.locked ? 0 : wav_pair_count_;
}

std::vector<int16_t> ImportedAudioTrackRepresentation::get_audio_stream_samples(
    size_t track, uint64_t first_pair, uint32_t pair_count) const {
  if (track != imported_track_index()) {
    return source_ ? source_->get_audio_stream_samples(track, first_pair,
                                                       pair_count)
                   : std::vector<int16_t>{};
  }
  if (descriptor_.locked || first_pair >= wav_pair_count_) return {};
  const uint32_t clamped = static_cast<uint32_t>(
      std::min<uint64_t>(pair_count, wav_pair_count_ - first_pair));
  return deps_->read_pairs(first_pair, clamped);
}

// ============================================================================
// AudioTrackImportStage
// ============================================================================

AudioTrackImportStage::AudioTrackImportStage() {
  // wav_path is required.
  set_configuration_status(orc::ConfigurationStatus::Red);
}

std::vector<ArtifactPtr> AudioTrackImportStage::execute(
    const std::vector<ArtifactPtr>& inputs,
    const std::map<std::string, ParameterValue>& parameters,
    ObservationContext& observation_context) {
  (void)observation_context;
  if (inputs.empty()) {
    throw DAGExecutionError("AudioTrackImportStage requires one input");
  }
  auto vfr =
      std::dynamic_pointer_cast<const VideoFrameRepresentation>(inputs[0]);
  if (!vfr) {
    throw DAGExecutionError(
        "AudioTrackImportStage input must be VideoFrameRepresentation");
  }

  if (!parameters.empty()) set_parameters(parameters);

  if (wav_path_.empty()) {
    throw DAGExecutionError("AudioTrackImportStage: wav_path is not set");
  }

  // The track cap is enforced at every track-adding stage, not just at the
  // container sink, so a DAG that validates also exports.
  if (vfr->audio_track_count() >= kMaxAudioTracks) {
    throw DAGExecutionError(
        "AudioTrackImportStage: cannot append a track: the input already "
        "carries the maximum of " +
        std::to_string(kMaxAudioTracks) + " audio tracks");
  }

  auto deps = deps_override_ ? deps_override_
                             : std::static_pointer_cast<IAudioTrackImportDeps>(
                                   std::make_shared<AudioTrackImportDeps>());
  const WavProbeResult probe = deps->open(wav_path_);
  if (!probe.valid) {
    throw DAGExecutionError("AudioTrackImportStage: '" + wav_path_ +
                            "' is not importable: " + probe.error);
  }

  // Resolve the track's lock state against the project's video standard.
  const auto params = vfr->get_video_parameters();
  const VideoSystem system = params ? params->system : VideoSystem::Unknown;
  const uint32_t pairs_per_frame = locked_audio_pairs_per_frame(system);
  const AudioSampleRate locked_rate = locked_audio_sample_rate(system);
  const uint64_t locked_total =
      static_cast<uint64_t>(vfr->frame_count()) * pairs_per_frame;

  bool locked = false;
  if (lock_mode_ == kLockModeLocked) {
    if (pairs_per_frame == 0) {
      throw DAGExecutionError(
          "AudioTrackImportStage: lock_mode 'locked' needs a known video "
          "standard to define the locked rate");
    }
    locked = true;
  } else if (lock_mode_ == kLockModeAuto) {
    // Locked when the WAV is exactly frame_count × pairs-per-frame at the
    // locked header rate; otherwise free-running.
    locked = pairs_per_frame != 0 && probe.pair_count == locked_total &&
             probe.sample_rate == nearest_integer_rate(locked_rate);
  }
  if (!locked && probe.sample_rate != kFreeRunningAudioRate.num) {
    throw DAGExecutionError(
        "AudioTrackImportStage: '" + wav_path_ +
        "' cannot be imported free-running: free-running tracks must be "
        "44100 Hz (header reports " +
        std::to_string(probe.sample_rate) + " Hz)");
  }

  AudioTrackDescriptor descriptor;
  descriptor.name = track_name_.empty() ? file_stem(wav_path_) : track_name_;
  descriptor.origin = AudioTrackOrigin::IMPORTED;
  descriptor.locked = locked;
  descriptor.sample_rate = locked ? locked_rate : kFreeRunningAudioRate;

  auto output = std::make_shared<ImportedAudioTrackRepresentation>(
      vfr, std::move(deps), std::move(descriptor), probe.pair_count,
      locked ? pairs_per_frame : 0);
  cached_output_ = output;

  return {output};
}

std::vector<ParameterDescriptor>
AudioTrackImportStage::get_parameter_descriptors(VideoSystem project_format,
                                                 SourceType source_type) const {
  (void)project_format;
  (void)source_type;
  std::vector<ParameterDescriptor> descriptors;

  {
    ParameterDescriptor desc;
    desc.name = "wav_path";
    desc.display_name = "WAV File";
    desc.description =
        "External WAV file to attach as a new audio track. Must be "
        "RIFF/WAVE, PCM, stereo, 16-bit (the CVBS-permitted encoding).";
    desc.type = ParameterType::FILE_PATH;
    desc.file_extension_hint = ".wav";
    desc.constraints.default_value = std::string("");
    desc.constraints.required = true;
    descriptors.push_back(desc);
  }

  {
    ParameterDescriptor desc;
    desc.name = "track_name";
    desc.display_name = "Track Name";
    desc.description =
        "Human-readable name for the imported track. Empty uses the WAV "
        "file name.";
    desc.type = ParameterType::STRING;
    desc.constraints.default_value = std::string("");
    descriptors.push_back(desc);
  }

  {
    ParameterDescriptor desc;
    desc.name = "lock_mode";
    desc.display_name = "Lock Mode";
    desc.description =
        "Timing of the imported track: 'auto' selects frame-locked when the "
        "WAV's length is exactly frame_count × pairs-per-frame at the locked "
        "rate, otherwise free-running (44100 Hz required); 'locked' and "
        "'free_running' force the choice.";
    desc.type = ParameterType::STRING;
    desc.constraints.default_value = std::string(kLockModeAuto);
    desc.constraints.allowed_strings = {kLockModeAuto, kLockModeLocked,
                                        kLockModeFreeRunning};
    descriptors.push_back(desc);
  }

  return descriptors;
}

std::map<std::string, ParameterValue> AudioTrackImportStage::get_parameters()
    const {
  return {{"wav_path", wav_path_},
          {"track_name", track_name_},
          {"lock_mode", lock_mode_}};
}

bool AudioTrackImportStage::set_parameters(
    const std::map<std::string, ParameterValue>& params) {
  for (const auto& [key, value] : params) {
    if (key == "wav_path") {
      const auto* v = std::get_if<std::string>(&value);
      if (!v) return false;
      wav_path_ = *v;
    } else if (key == "track_name") {
      const auto* v = std::get_if<std::string>(&value);
      if (!v) return false;
      track_name_ = *v;
    } else if (key == "lock_mode") {
      const auto* v = std::get_if<std::string>(&value);
      if (!v || (*v != kLockModeAuto && *v != kLockModeLocked &&
                 *v != kLockModeFreeRunning)) {
        ORC_LOG_ERROR("AudioTrackImportStage: unknown lock_mode '{}'",
                      v ? *v : std::string("<non-string>"));
        return false;
      }
      lock_mode_ = *v;
    } else {
      ORC_LOG_WARN("AudioTrackImportStage: unknown parameter '{}'", key);
      return false;
    }
  }
  set_configuration_status(wav_path_.empty() ? orc::ConfigurationStatus::Red
                                             : orc::ConfigurationStatus::Green);
  return true;
}

StagePreviewCapability AudioTrackImportStage::get_preview_capability() const {
  return PreviewHelpers::make_signal_preview_capability(cached_output_);
}

}  // namespace orc
