/*
 * File:        frame_map_stage.cpp
 * Module:      orc-stage-plugin-frame-map
 * Purpose:     Frame mapping/reordering and sequence-correction stage
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#include "frame_map_stage.h"

#include <orc/stage/error_types.h>
#include <orc/stage/logging.h>
#include <orc/stage/preview_helpers.h>

#include <algorithm>
#include <sstream>

namespace orc {

// ============================================================================
// FrameMappedRepresentation
// ============================================================================

static constexpr FrameID kPaddingFrameID{UINT64_MAX};

FrameMappedRepresentation::FrameMappedRepresentation(
    std::shared_ptr<const VideoFrameRepresentation> source,
    std::vector<FrameID> frame_mapping,
    std::vector<PaddingDescriptor> padding_descriptors, const std::string& tag)
    : VideoFrameRepresentationWrapper(std::move(source)),
      Artifact(ArtifactID("frame_map_" + tag), Provenance{}),
      frame_mapping_(std::move(frame_mapping)),
      padding_descriptors_(std::move(padding_descriptors)) {}

std::optional<size_t> FrameMappedRepresentation::resolve_index(
    FrameID id) const {
  if (id >= static_cast<FrameID>(frame_mapping_.size())) {
    return std::nullopt;
  }
  return static_cast<size_t>(id);
}

bool FrameMappedRepresentation::is_padding(size_t index) const {
  return frame_mapping_[index] == kPaddingFrameID;
}

const FrameMappedRepresentation::PaddingDescriptor*
FrameMappedRepresentation::find_padding(FrameID output_id) const {
  for (const auto& pd : padding_descriptors_) {
    if (pd.output_id == output_id) return &pd;
  }
  return nullptr;
}

void FrameMappedRepresentation::ensure_black_frame() const {
  if (!black_frame_.empty()) return;
  if (!source_) return;
  auto params = source_->get_video_parameters();
  if (!params) return;
  size_t total =
      static_cast<size_t>(params->number_of_sequential_frames > 0
                              ? params->frame_width_nominal
                              : 0) *
      static_cast<size_t>(params->frame_height > 0 ? params->frame_height : 0);
  if (total == 0) {
    // Fallback: use first real frame size
    auto range = source_->frame_range();
    for (FrameID fid = range.first; fid <= range.last; ++fid) {
      auto desc = source_->get_frame_descriptor(fid);
      if (desc) {
        total = desc->samples_total;
        break;
      }
    }
  }
  black_frame_.assign(total,
                      static_cast<sample_type>(
                          source_->get_video_parameters()->blanking_level));
}

FrameIDRange FrameMappedRepresentation::frame_range() const {
  if (frame_mapping_.empty()) return FrameIDRange{FrameID{0}, FrameID{0}};
  // FrameIDRange.last is inclusive; the last valid index is size() - 1.
  return FrameIDRange{FrameID{0}, FrameID{frame_mapping_.size() - 1}};
}

bool FrameMappedRepresentation::has_frame(FrameID id) const {
  auto idx = resolve_index(id);
  if (!idx) return false;
  if (is_padding(*idx)) return true;
  return source_ && source_->has_frame(frame_mapping_[*idx]);
}

std::optional<FrameDescriptor> FrameMappedRepresentation::get_frame_descriptor(
    FrameID id) const {
  auto idx = resolve_index(id);
  if (!idx) return std::nullopt;

  if (is_padding(*idx)) {
    const auto* pd = find_padding(id);
    if (!pd) {
      // Range-spec PAD frames (from "PAD_N" in the mapping spec) have no
      // PaddingDescriptor because apply_pad_gaps is not called for them.
      // Returning nullopt here would cause the stacker to treat the frame as
      // present-but-unknown and search ±4 frames for a CFI match — the PAL
      // 4-frame cycle guarantees a hit 4 positions back, which is a real frame
      // from a different disc picture. Synthesise a minimal is_padding_frame
      // descriptor so callers correctly exclude this source.
      FrameDescriptor desc;
      desc.frame_id = id;
      desc.is_padding_frame = true;
      if (source_) {
        if (const auto params = source_->get_video_parameters()) {
          desc.system = params->system;
          desc.height = static_cast<size_t>(params->frame_height);
          desc.samples_per_line_nominal =
              static_cast<size_t>(params->frame_width_nominal);
          desc.samples_total = desc.height * desc.samples_per_line_nominal;
        }
      }
      return desc;
    }
    FrameDescriptor desc;
    desc.frame_id = id;
    desc.system = pd->system;
    desc.height = pd->height;
    desc.samples_total = pd->samples_total;
    desc.samples_per_line_nominal = pd->samples_per_line_nominal;
    desc.colour_frame_index = pd->colour_frame_index;
    desc.is_padding_frame = true;
    return desc;
  }

  if (!source_) return std::nullopt;
  auto desc = source_->get_frame_descriptor(frame_mapping_[*idx]);
  if (desc) desc->frame_id = id;
  return desc;
}

const VideoFrameRepresentation::sample_type*
FrameMappedRepresentation::get_frame(FrameID id) const {
  auto idx = resolve_index(id);
  if (!idx) return nullptr;
  if (is_padding(*idx)) {
    ensure_black_frame();
    return black_frame_.empty() ? nullptr : black_frame_.data();
  }
  return source_ ? source_->get_frame(frame_mapping_[*idx]) : nullptr;
}

const VideoFrameRepresentation::sample_type*
FrameMappedRepresentation::get_line(FrameID id, size_t line) const {
  auto idx = resolve_index(id);
  if (!idx) return nullptr;
  if (is_padding(*idx)) {
    ensure_black_frame();
    return black_frame_.empty() ? nullptr : black_frame_.data();
  }
  return source_ ? source_->get_line(frame_mapping_[*idx], line) : nullptr;
}

std::vector<VideoFrameRepresentation::sample_type>
FrameMappedRepresentation::get_frame_copy(FrameID id) const {
  auto idx = resolve_index(id);
  if (!idx) return {};
  if (is_padding(*idx)) {
    ensure_black_frame();
    return black_frame_;
  }
  return source_ ? source_->get_frame_copy(frame_mapping_[*idx])
                 : std::vector<sample_type>{};
}

const VideoFrameRepresentation::sample_type*
FrameMappedRepresentation::get_frame_luma(FrameID id) const {
  auto idx = resolve_index(id);
  if (!idx) return nullptr;
  if (is_padding(*idx)) return nullptr;
  return source_ ? source_->get_frame_luma(frame_mapping_[*idx]) : nullptr;
}

const VideoFrameRepresentation::sample_type*
FrameMappedRepresentation::get_frame_chroma(FrameID id) const {
  auto idx = resolve_index(id);
  if (!idx) return nullptr;
  if (is_padding(*idx)) return nullptr;
  return source_ ? source_->get_frame_chroma(frame_mapping_[*idx]) : nullptr;
}

const VideoFrameRepresentation::sample_type*
FrameMappedRepresentation::get_line_luma(FrameID id, size_t line) const {
  auto idx = resolve_index(id);
  if (!idx) return nullptr;
  if (is_padding(*idx)) return nullptr;
  return source_ ? source_->get_line_luma(frame_mapping_[*idx], line) : nullptr;
}

const VideoFrameRepresentation::sample_type*
FrameMappedRepresentation::get_line_chroma(FrameID id, size_t line) const {
  auto idx = resolve_index(id);
  if (!idx) return nullptr;
  if (is_padding(*idx)) return nullptr;
  return source_ ? source_->get_line_chroma(frame_mapping_[*idx], line)
                 : nullptr;
}

std::vector<DropoutRun> FrameMappedRepresentation::get_dropout_hints(
    FrameID id) const {
  auto idx = resolve_index(id);
  if (!idx) return {};
  if (is_padding(*idx)) return {};
  if (!source_) return {};
  // Rewrite frame IDs so the runs describe this representation's frame, not
  // the source frame they were mapped from (consumers such as dropout_map
  // may rely on the frame_id field).
  auto runs = source_->get_dropout_hints(frame_mapping_[*idx]);
  for (auto& run : runs) {
    run.frame_id = id;
  }
  return runs;
}

uint32_t FrameMappedRepresentation::get_audio_sample_count(size_t track,
                                                           FrameID id) const {
  auto idx = resolve_index(id);
  if (!idx || is_padding(*idx)) return 0;
  return source_ ? source_->get_audio_sample_count(track, frame_mapping_[*idx])
                 : 0;
}

std::vector<int16_t> FrameMappedRepresentation::get_audio_samples(
    size_t track, FrameID id) const {
  auto idx = resolve_index(id);
  if (!idx) return {};
  if (is_padding(*idx)) {
    // Padding frames get silence only for locked tracks; free-running tracks
    // have no per-frame samples.
    if (!source_) return {};
    const auto desc = source_->get_audio_track_descriptor(track);
    if (!desc || !desc->locked) return {};
    auto params = source_->get_video_parameters();
    if (!params) return {};
    const size_t pairs = locked_audio_pairs_per_frame(params->system);
    if (pairs == 0) return {};
    if (silence_audio_.size() != pairs * 2) {
      silence_audio_.assign(pairs * 2, 0);
    }
    return silence_audio_;
  }
  return source_ ? source_->get_audio_samples(track, frame_mapping_[*idx])
                 : std::vector<int16_t>{};
}

const FrameMappedRepresentation::StreamMap*
FrameMappedRepresentation::ensure_stream_map(size_t track) const {
  if (!source_) return nullptr;
  const auto desc = source_->get_audio_track_descriptor(track);
  if (!desc || desc->locked) return nullptr;
  const auto params = source_->get_video_parameters();
  if (!params || params->system == VideoSystem::Unknown) return nullptr;
  if (desc->sample_rate.num == 0 || desc->sample_rate.den == 0) return nullptr;

  std::lock_guard<std::mutex> lk(stream_map_mutex_);
  auto it = stream_maps_.find(track);
  if (it != stream_maps_.end()) return &it->second;

  StreamMap map;
  map.rate = desc->sample_rate;
  map.system = params->system;
  map.cumulative.reserve(frame_mapping_.size() + 1);
  map.cumulative.push_back(0);
  const FrameID src_first = source_->frame_range().first;
  uint64_t total = 0;
  for (size_t i = 0; i < frame_mapping_.size(); ++i) {
    // Window sizes come from the cumulative offset helper: per-frame pair
    // counts are NOT constant (44100 Hz NTSC alternates 1471/1472 pairs).
    // Padding frames contribute a silence window sized at the OUTPUT
    // position; real frames contribute their source frame's window.
    const uint64_t m = (frame_mapping_[i] == kPaddingFrameID)
                           ? static_cast<uint64_t>(i)
                           : frame_mapping_[i] - src_first;
    total += audio_stream_pair_offset(m + 1, map.rate, map.system) -
             audio_stream_pair_offset(m, map.rate, map.system);
    map.cumulative.push_back(total);
  }
  auto [ins, inserted] = stream_maps_.emplace(track, std::move(map));
  (void)inserted;
  return &ins->second;
}

uint64_t FrameMappedRepresentation::get_audio_stream_pair_count(
    size_t track) const {
  const StreamMap* map = ensure_stream_map(track);
  if (!map) {
    return source_ ? source_->get_audio_stream_pair_count(track) : 0;
  }
  return map->cumulative.back();
}

std::vector<int16_t> FrameMappedRepresentation::get_audio_stream_samples(
    size_t track, uint64_t first_pair, uint32_t pair_count) const {
  const StreamMap* map = ensure_stream_map(track);
  if (!map) {
    return source_ ? source_->get_audio_stream_samples(track, first_pair,
                                                       pair_count)
                   : std::vector<int16_t>{};
  }

  const uint64_t total = map->cumulative.back();
  if (first_pair >= total) return {};
  const uint32_t clamped =
      static_cast<uint32_t>(std::min<uint64_t>(pair_count, total - first_pair));

  // Locate the first window covering first_pair, then stitch the requested
  // range across window boundaries.
  const size_t start_window =
      static_cast<size_t>(std::upper_bound(map->cumulative.begin(),
                                           map->cumulative.end(), first_pair) -
                          map->cumulative.begin()) -
      1;
  const FrameID src_first = source_->frame_range().first;

  std::vector<int16_t> result;
  result.reserve(static_cast<size_t>(clamped) * 2);
  uint64_t pos = first_pair;
  uint32_t remaining = clamped;
  for (size_t w = start_window; remaining > 0 && w < frame_mapping_.size();
       ++w) {
    const uint64_t window_end = map->cumulative[w + 1];
    if (pos >= window_end) continue;  // zero-length window
    const uint32_t take =
        static_cast<uint32_t>(std::min<uint64_t>(window_end - pos, remaining));
    if (frame_mapping_[w] == kPaddingFrameID) {
      result.insert(result.end(), static_cast<size_t>(take) * 2, 0);
    } else {
      const uint64_t m = frame_mapping_[w] - src_first;
      const uint64_t src_start =
          audio_stream_pair_offset(m, map->rate, map->system) +
          (pos - map->cumulative[w]);
      auto chunk = source_->get_audio_stream_samples(track, src_start, take);
      // Silence-fill short source reads so window alignment is preserved.
      chunk.resize(static_cast<size_t>(take) * 2, 0);
      result.insert(result.end(), chunk.begin(), chunk.end());
    }
    pos += take;
    remaining -= take;
  }
  return result;
}

size_t FrameMappedRepresentation::count_free_running_discontinuities(
    const std::vector<FrameID>& mapping) {
  size_t discontinuities = 0;
  for (size_t i = 1; i < mapping.size(); ++i) {
    const bool prev_pad = mapping[i - 1] == kPaddingFrameID;
    const bool cur_pad = mapping[i] == kPaddingFrameID;
    if (prev_pad && cur_pad) continue;  // silence into silence
    if (!prev_pad && !cur_pad && mapping[i] == mapping[i - 1] + 1) continue;
    ++discontinuities;
  }
  return discontinuities;
}

uint32_t FrameMappedRepresentation::get_efm_sample_count(FrameID id) const {
  auto idx = resolve_index(id);
  if (!idx || is_padding(*idx)) return 0;
  return source_ ? source_->get_efm_sample_count(frame_mapping_[*idx]) : 0;
}

std::vector<uint8_t> FrameMappedRepresentation::get_efm_samples(
    FrameID id) const {
  auto idx = resolve_index(id);
  if (!idx || is_padding(*idx)) return {};
  return source_ ? source_->get_efm_samples(frame_mapping_[*idx])
                 : std::vector<uint8_t>{};
}

uint32_t FrameMappedRepresentation::get_ac3_symbol_count(FrameID id) const {
  auto idx = resolve_index(id);
  if (!idx || is_padding(*idx)) return 0;
  return source_ ? source_->get_ac3_symbol_count(frame_mapping_[*idx]) : 0;
}

std::vector<uint8_t> FrameMappedRepresentation::get_ac3_symbols(
    FrameID id) const {
  auto idx = resolve_index(id);
  if (!idx || is_padding(*idx)) return {};
  return source_ ? source_->get_ac3_symbols(frame_mapping_[*idx])
                 : std::vector<uint8_t>{};
}

// ============================================================================
// FrameMapStage
// ============================================================================

FrameMapStage::FrameMapStage() {
  set_configuration_status(orc::ConfigurationStatus::Red);
}

std::vector<std::pair<uint64_t, uint64_t>> FrameMapStage::parse_ranges(
    const std::string& spec) {
  std::vector<std::pair<uint64_t, uint64_t>> ranges;
  if (spec.empty()) return ranges;

  std::istringstream iss(spec);
  std::string token;
  while (std::getline(iss, token, ',')) {
    // Trim whitespace
    auto start_it = token.find_first_not_of(" \t");
    auto end_it = token.find_last_not_of(" \t");
    if (start_it == std::string::npos) continue;
    token = token.substr(start_it, end_it - start_it + 1);

    // PAD_N directive
    if (token.size() > 4 && token.substr(0, 4) == "PAD_") {
      try {
        uint64_t n = std::stoull(token.substr(4));
        ranges.emplace_back(UINT64_MAX, n);
        continue;
      } catch (...) {
        ORC_LOG_ERROR("FrameMapStage: invalid PAD directive '{}'", token);
        return {};
      }
    }

    size_t dash = token.find('-');
    if (dash == std::string::npos) {
      // Single frame
      try {
        uint64_t id = std::stoull(token);
        ranges.emplace_back(id, id);
      } catch (...) {
        ORC_LOG_ERROR("FrameMapStage: invalid frame id '{}'", token);
        return {};
      }
    } else {
      // Range start-end
      auto lhs = token.substr(0, dash);
      auto rhs = token.substr(dash + 1);
      try {
        uint64_t s = std::stoull(lhs);
        uint64_t e = std::stoull(rhs);
        if (s > e) {
          ORC_LOG_ERROR("FrameMapStage: range start > end in '{}'", token);
          return {};
        }
        ranges.emplace_back(s, e);
      } catch (...) {
        ORC_LOG_ERROR("FrameMapStage: invalid range '{}'", token);
        return {};
      }
    }
  }
  return ranges;
}

std::vector<FrameID> FrameMapStage::build_frame_mapping(
    const std::vector<std::pair<uint64_t, uint64_t>>& ranges,
    const VideoFrameRepresentation& source) {
  std::vector<FrameID> mapping;
  FrameIDRange src_range = source.frame_range();
  uint64_t src_end = src_range.last;

  for (const auto& [s, e] : ranges) {
    if (s == UINT64_MAX) {
      // PAD_N: insert e padding slots
      for (uint64_t i = 0; i < e; ++i) {
        mapping.push_back(FrameID{UINT64_MAX});
      }
      continue;
    }
    for (uint64_t id = s; id <= e; ++id) {
      if (id > src_end) {
        ORC_LOG_WARN("FrameMapStage: frame {} out of source range, skipping",
                     id);
        continue;
      }
      FrameID fid{id};
      if (source.has_frame(fid)) {
        mapping.push_back(fid);
      }
    }
  }
  return mapping;
}

size_t FrameMapStage::apply_remove_duplicates(
    std::vector<FrameID>& mapping, const VideoFrameRepresentation& source) {
  size_t removed = 0;
  size_t i = 1;
  while (i < mapping.size()) {
    FrameID prev = mapping[i - 1];
    FrameID cur = mapping[i];

    // Skip padding frames in comparison
    if (prev == FrameID{UINT64_MAX} || cur == FrameID{UINT64_MAX}) {
      ++i;
      continue;
    }

    auto prev_desc = source.get_frame_descriptor(prev);
    auto cur_desc = source.get_frame_descriptor(cur);

    if (prev_desc && cur_desc && prev_desc->colour_frame_index >= 0 &&
        cur_desc->colour_frame_index >= 0 &&
        prev_desc->colour_frame_index == cur_desc->colour_frame_index) {
      ORC_LOG_DEBUG(
          "FrameMapStage: removing duplicate frame {} (colour_frame_index {})",
          cur.value(), cur_desc->colour_frame_index);
      mapping.erase(mapping.begin() + static_cast<std::ptrdiff_t>(i));
      ++removed;
      // Don't advance i — re-compare the new pair at position i
    } else {
      ++i;
    }
  }
  return removed;
}

int FrameMapStage::next_colour_index(int current, VideoSystem sys) {
  if (current < 0) return -1;
  switch (sys) {
    case VideoSystem::PAL:
    case VideoSystem::PAL_M:
      // EBU Tech. 3280-E §1.1.1 / ITU-R BT.1700-1: 4-frame sequence 1-2-3-4
      return (current % 4) + 1;
    case VideoSystem::NTSC:
      // SMPTE 244M-2003 §3.2: 2-frame A/B sequence 0-1
      return 1 - current;
    default:
      return -1;
  }
}

size_t FrameMapStage::apply_pad_gaps(
    std::vector<FrameID>& mapping,
    std::vector<FrameMappedRepresentation::PaddingDescriptor>& pads,
    const VideoFrameRepresentation& source, const std::string& pad_strategy,
    std::string& gap_positions_out) {
  size_t total_inserted = 0;
  std::ostringstream gap_ss;
  bool first_gap = true;

  // Gather geometry from first real frame
  VideoSystem sys = VideoSystem::Unknown;
  size_t height = 0;
  size_t samples_total = 0;
  size_t samples_per_line_nominal = 0;
  {
    for (const auto& fid : mapping) {
      if (fid == FrameID{UINT64_MAX}) continue;
      auto desc = source.get_frame_descriptor(fid);
      if (desc) {
        sys = desc->system;
        height = desc->height;
        samples_total = desc->samples_total;
        samples_per_line_nominal = desc->samples_per_line_nominal;
        break;
      }
    }
  }

  if (sys == VideoSystem::Unknown) return 0;

  size_t i = 1;
  while (i < mapping.size()) {
    FrameID prev_fid = mapping[i - 1];
    FrameID cur_fid = mapping[i];

    if (prev_fid == FrameID{UINT64_MAX} || cur_fid == FrameID{UINT64_MAX}) {
      ++i;
      continue;
    }

    auto prev_desc = source.get_frame_descriptor(prev_fid);
    auto cur_desc = source.get_frame_descriptor(cur_fid);

    if (!prev_desc || !cur_desc) {
      ++i;
      continue;
    }

    int prev_idx = prev_desc->colour_frame_index;
    int cur_idx = cur_desc->colour_frame_index;

    if (prev_idx < 0 || cur_idx < 0) {
      ++i;
      continue;
    }

    int expected = next_colour_index(prev_idx, sys);
    if (expected == cur_idx) {
      ++i;
      continue;
    }

    // Gap detected — how many frames are missing?
    size_t insert_pos = i;
    size_t gap_count = 0;
    int fill_idx = expected;
    while (fill_idx != cur_idx && gap_count < 8) {
      FrameMappedRepresentation::PaddingDescriptor pd;
      // The output FrameID will be determined after insertion; we use a
      // placeholder here and fix it up after the insert loop
      pd.output_id = FrameID{UINT64_MAX};  // filled below
      pd.colour_frame_index = fill_idx;
      pd.system = sys;
      pd.height = height;
      pd.samples_total = samples_total;
      pd.samples_per_line_nominal = samples_per_line_nominal;

      if (pad_strategy == "nearest") {
        // Use the nearest real source frame — keep as padding for descriptor
        // but mark so get_frame returns the nearest real frame sample data
        // We still set as padding (UINT64_MAX) so consumers see
        // is_padding_frame
      }

      mapping.insert(mapping.begin() + static_cast<std::ptrdiff_t>(insert_pos),
                     FrameID{UINT64_MAX});
      pads.push_back(pd);

      ++insert_pos;
      ++gap_count;
      ++total_inserted;

      fill_idx = next_colour_index(fill_idx, sys);
      if (fill_idx == cur_idx) break;
    }

    if (gap_count > 0) {
      if (!first_gap) gap_ss << ",";
      gap_ss << i;
      first_gap = false;
      ORC_LOG_DEBUG(
          "FrameMapStage: inserted {} padding frame(s) at position {} "
          "(colour index gap {} → {})",
          gap_count, i, prev_idx, cur_idx);
    }

    i += gap_count + 1;
  }

  // Fix up output_id fields for all inserted padding descriptors
  // Re-walk mapping to assign correct output FrameIDs to padding descriptors
  size_t pad_assign_idx = 0;
  for (size_t pos = 0; pos < mapping.size(); ++pos) {
    if (mapping[pos] == FrameID{UINT64_MAX} && pad_assign_idx < pads.size()) {
      // Only assign if this pad doesn't already have a valid output_id
      if (pads[pad_assign_idx].output_id == FrameID{UINT64_MAX}) {
        pads[pad_assign_idx].output_id = FrameID{pos};
        ++pad_assign_idx;
      }
    }
  }

  gap_positions_out = gap_ss.str();
  return total_inserted;
}

std::vector<ArtifactPtr> FrameMapStage::execute(
    const std::vector<ArtifactPtr>& inputs,
    const std::map<std::string, ParameterValue>& parameters,
    ObservationContext& observation_context) {
  if (inputs.empty()) {
    throw DAGExecutionError("FrameMapStage requires one input");
  }

  auto source =
      std::dynamic_pointer_cast<const VideoFrameRepresentation>(inputs[0]);
  if (!source) {
    throw DAGExecutionError(
        "FrameMapStage: input must be a VideoFrameRepresentation");
  }

  // Apply any runtime parameter overrides
  if (!parameters.empty()) {
    set_parameters(parameters);
  }

  // Pass-through when no ranges and no processing requested
  if (range_spec_.empty() && !remove_duplicates_ && !pad_gaps_) {
    cached_output_ = source;
    return {inputs[0]};
  }

  // Build initial frame mapping from range specification
  std::vector<FrameID> mapping;
  if (!range_spec_.empty() && !cached_ranges_.empty()) {
    mapping = build_frame_mapping(cached_ranges_, *source);
    if (mapping.empty()) {
      ORC_LOG_WARN("FrameMapStage: range spec produced empty mapping");
      cached_output_ = source;
      return {inputs[0]};
    }
  } else {
    // No range spec — use identity mapping over source range
    FrameIDRange rng = source->frame_range();
    for (FrameID fid = rng.first; fid <= rng.last; ++fid) {
      if (source->has_frame(fid)) {
        mapping.push_back(fid);
      }
    }
  }

  // Duplicate frame removal
  size_t removed_count = 0;
  if (remove_duplicates_) {
    removed_count = apply_remove_duplicates(mapping, *source);
    ORC_LOG_DEBUG("FrameMapStage: removed {} duplicate frame(s)",
                  removed_count);
  }

  // Gap padding
  std::vector<FrameMappedRepresentation::PaddingDescriptor> pads;
  size_t padded_count = 0;
  std::string gap_positions;
  if (pad_gaps_) {
    padded_count =
        apply_pad_gaps(mapping, pads, *source, pad_strategy_, gap_positions);
    ORC_LOG_DEBUG("FrameMapStage: inserted {} padding frame(s) into {} gap(s)",
                  padded_count, gap_positions.empty() ? 0 : 1);
  }

  // Emit aggregate observations on field 0 as a convention
  // (ObservationContext is still FieldID-keyed; use FieldID(0) for aggregates)
  if (removed_count > 0) {
    observation_context.set(FieldID(0), "frame_map", "frames_removed",
                            static_cast<int64_t>(removed_count));
  }
  if (padded_count > 0) {
    observation_context.set(FieldID(0), "frame_map", "frames_padded",
                            static_cast<int64_t>(padded_count));
    if (!gap_positions.empty()) {
      observation_context.set(FieldID(0), "frame_map", "gap_positions",
                              gap_positions);
    }
  }

  // Free-running tracks are remapped in the time domain by stitching
  // per-frame windows; joins at mapping discontinuities are not
  // phase-continuous, so tell the user which tracks are affected.
  const size_t discontinuities =
      FrameMappedRepresentation::count_free_running_discontinuities(mapping);
  if (discontinuities > 0) {
    std::string affected_tracks;
    for (size_t track = 0; track < source->audio_track_count(); ++track) {
      const auto desc = source->get_audio_track_descriptor(track);
      if (!desc || desc->locked) continue;
      if (!affected_tracks.empty()) affected_tracks += ", ";
      affected_tracks += std::to_string(track);
    }
    if (!affected_tracks.empty()) {
      const std::string message =
          "free-running track(s) " + affected_tracks + " remapped with " +
          std::to_string(discontinuities) +
          " discontinuities — audible clicks possible; lock audio at the "
          "source for gapless stacking/reordering";
      observation_context.set(FieldID(0), "frame_map", "free_running_audio",
                              message);
      observation_context.set(FieldID(0), "frame_map",
                              "free_running_discontinuities",
                              static_cast<int64_t>(discontinuities));
      ORC_LOG_INFO("FrameMapStage: {}", message);
    }
  }

  std::string tag = range_spec_ + "_rd" + std::to_string(remove_duplicates_) +
                    "_pg" + std::to_string(pad_gaps_);

  auto result = std::make_shared<FrameMappedRepresentation>(
      source, std::move(mapping), std::move(pads), tag);

  cached_output_ = result;
  return {result};
}

std::vector<ParameterDescriptor> FrameMapStage::get_parameter_descriptors(
    VideoSystem /*project_format*/, SourceType /*source_type*/) const {
  return {
      ParameterDescriptor{
          "ranges", "Frame Ranges",
          "Comma-separated frame ranges, 1-based as shown in the preview "
          "(e.g., '1-11,21-31,12-20'). Stored 0-based in the project file. "
          "Empty = pass-through.",
          ParameterType::STRING,
          ParameterConstraints{std::nullopt,
                               std::nullopt,
                               ParameterValue{std::string("")},
                               {},
                               false,
                               std::nullopt}},
      ParameterDescriptor{
          "remove_duplicates", "Remove Duplicates",
          "Remove consecutive frames with identical colour_frame_index",
          ParameterType::BOOL,
          ParameterConstraints{std::nullopt,
                               std::nullopt,
                               ParameterValue{false},
                               {},
                               false,
                               std::nullopt}},
      ParameterDescriptor{
          "pad_gaps", "Pad Gaps",
          "Fill colour-frame-index sequence gaps with synthetic padding frames",
          ParameterType::BOOL,
          ParameterConstraints{std::nullopt,
                               std::nullopt,
                               ParameterValue{false},
                               {},
                               false,
                               std::nullopt}},
      ParameterDescriptor{
          "pad_strategy", "Padding Strategy",
          "How to fill gaps: 'nearest' (nearest real frame) or 'black'",
          ParameterType::STRING,
          ParameterConstraints{std::nullopt,
                               std::nullopt,
                               ParameterValue{std::string("nearest")},
                               {"nearest", "black"},
                               false,
                               std::nullopt}},
  };
}

std::map<std::string, ParameterValue> FrameMapStage::get_parameters() const {
  return {{"ranges", ParameterValue{range_spec_}},
          {"remove_duplicates", ParameterValue{remove_duplicates_}},
          {"pad_gaps", ParameterValue{pad_gaps_}},
          {"pad_strategy", ParameterValue{pad_strategy_}}};
}

bool FrameMapStage::set_parameters(
    const std::map<std::string, ParameterValue>& params) {
  for (const auto& [key, value] : params) {
    if (key == "ranges") {
      if (auto* v = std::get_if<std::string>(&value)) {
        range_spec_ = *v;
        if (!range_spec_.empty()) {
          cached_ranges_ = parse_ranges(range_spec_);
          if (cached_ranges_.empty()) {
            ORC_LOG_ERROR("FrameMapStage: invalid range spec '{}'",
                          range_spec_);
            return false;
          }
        } else {
          cached_ranges_.clear();
        }
      } else {
        return false;
      }
    } else if (key == "remove_duplicates") {
      if (auto* v = std::get_if<bool>(&value)) {
        remove_duplicates_ = *v;
      } else {
        return false;
      }
    } else if (key == "pad_gaps") {
      if (auto* v = std::get_if<bool>(&value)) {
        pad_gaps_ = *v;
      } else {
        return false;
      }
    } else if (key == "pad_strategy") {
      if (auto* v = std::get_if<std::string>(&value)) {
        if (*v != "nearest" && *v != "black") {
          ORC_LOG_ERROR("FrameMapStage: unknown pad_strategy '{}'", *v);
          return false;
        }
        pad_strategy_ = *v;
      } else {
        return false;
      }
    } else {
      ORC_LOG_WARN("FrameMapStage: unknown parameter '{}'", key);
      return false;
    }
  }
  set_configuration_status(range_spec_.empty()
                               ? orc::ConfigurationStatus::Red
                               : orc::ConfigurationStatus::Green);
  return true;
}

StagePreviewCapability FrameMapStage::get_preview_capability() const {
  return PreviewHelpers::make_signal_preview_capability(cached_output_);
}

}  // namespace orc
