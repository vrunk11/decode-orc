/*
 * File:        burst_level_analysis_sink_deps.cpp
 * Module:      orc-core
 * Purpose:     BurstLevelAnalysisSinkStage dependency implementation
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 decode-orc contributors
 */

#include "burst_level_analysis_sink_deps.h"

#include <orc/stage/field_id.h>
#include <orc/support/logging.h>

#include <cmath>
#include <fstream>
#include <memory>
#include <utility>
#include <variant>

namespace orc {
void BurstLevelAnalysisSinkStageDeps::init(
    TriggerProgressCallback progress_callback,
    std::atomic<bool>* cancel_requested) {
  progress_callback_ = std::move(progress_callback);
  cancel_requested_ = cancel_requested;
}

BurstAnalysisComputeResult BurstLevelAnalysisSinkStageDeps::compute_and_analyze(
    VideoFrameRepresentation* representation,
    IObservationContext& observation_context,
    BurstAnalysisComputeOptions options) {
  if (!representation) {
    return {false, "Input representation is null", {}, 0};
  }

  BurstAnalysisComputeResult result;
  result.success = true;
  result.message = "Burst level analysis complete";

  const auto frame_rng = representation->frame_range();
  const uint64_t total_frames = frame_rng.count();

  if (total_frames == 0) {
    logger_.warn("BurstLevelAnalysisSinkDeps: No frames available");
    result.total_frames = 0;
    return result;
  }

  // A single observer session is reused across every sampled frame (the burst
  // level observer holds no cross-frame state; its observation field is read
  // and cleared each iteration). A null service — e.g. an older host — leaves
  // the handle null and the per-frame observation is skipped.
  std::unique_ptr<IObserverHandle> burst_level_handle;
  if (observation_service_) {
    burst_level_handle = observation_service_->create_observer("burst_level");
  } else {
    logger_.warn(
        "BurstLevelAnalysisSinkDeps: observation service unavailable; burst "
        "level observations skipped");
  }

  // Bucket-sampled analysis: divide the recording into at most kDefaultBuckets
  // display points and, within each bucket, analyze at most kSamplesPerBucket
  // evenly-spaced frames.  For small sources (bucket size <= kSamplesPerBucket)
  // every frame in the bucket is analyzed.  This keeps wall-clock time near-
  // constant regardless of recording length.
  constexpr uint64_t kDefaultBuckets = 1000;
  constexpr uint64_t kSamplesPerBucket = 1;
  const uint64_t bucket_count =
      (total_frames < kDefaultBuckets) ? total_frames : kDefaultBuckets;

  logger_.debug(
      "BurstLevelAnalysisSinkDeps: {} frames → {} buckets (~{} samples/bucket)",
      total_frames, bucket_count, kSamplesPerBucket);

  result.frame_stats.reserve(static_cast<size_t>(bucket_count));

  for (uint64_t b = 0; b < bucket_count; ++b) {
    if (cancel_requested_ && cancel_requested_->load()) {
      logger_.warn("BurstLevelAnalysisSinkDeps: Cancel requested at bucket {}",
                   b);
      result.success = false;
      result.message = "Cancelled by user";
      result.frame_stats.clear();
      result.total_frames = 0;
      return result;
    }

    // Inclusive frame range for this bucket (no frame is missed or counted
    // twice across adjacent buckets).
    const FrameID bucket_start =
        frame_rng.first + (b * total_frames) / bucket_count;
    const FrameID bucket_end =
        frame_rng.first + ((b + 1) * total_frames) / bucket_count - 1;
    const uint64_t bucket_size = bucket_end - bucket_start + 1;
    const uint64_t n_samples =
        (kSamplesPerBucket < bucket_size) ? kSamplesPerBucket : bucket_size;

    double sum = 0.0;
    size_t count = 0;

    for (uint64_t s = 0; s < n_samples; ++s) {
      // Evenly distribute sample frames across the bucket so the first and last
      // frames are always included.
      const FrameID fid =
          (n_samples == 1U)
              ? bucket_start
              : bucket_start + (s * (bucket_size - 1U)) / (n_samples - 1U);

      if (burst_level_handle) {
        burst_level_handle->process_frame(*representation, fid,
                                          observation_context);
      }

      const FieldID frame_fid(fid * 2U);
      auto val = observation_context.get(frame_fid, "burst_level",
                                         "median_burst_10bit");
      logger_.debug(
          "BurstLevelAnalysisSinkDeps: fid={} field_id={} val_present={} "
          "type_ok={}",
          fid, frame_fid.value(), val.has_value(),
          val.has_value() && std::holds_alternative<double>(*val));
      if (val && std::holds_alternative<double>(*val)) {
        sum += std::get<double>(*val);
        ++count;
      }
      observation_context.clear_field(frame_fid);
    }

    FrameBurstLevelStats frame_stat;
    // Use the center frame of the bucket as the representative frame number
    // (1-based for display).
    frame_stat.frame_number =
        static_cast<int32_t>(bucket_start + (bucket_end - bucket_start) / 2U) +
        1;

    if (count > 0) {
      frame_stat.median_burst_10bit = sum / static_cast<double>(count);
      frame_stat.has_data = true;
      frame_stat.field_count = count;
    }

    result.frame_stats.push_back(frame_stat);

    if (progress_callback_ && (b % 50 == 0 || b + 1 == bucket_count)) {
      progress_callback_(b + 1, bucket_count,
                         "Analysing bucket " + std::to_string(b + 1) + "/" +
                             std::to_string(bucket_count));
    }
  }

  result.total_frames = static_cast<int32_t>(total_frames);
  logger_.debug(
      "BurstLevelAnalysisSinkDeps: Complete — {} buckets from {} frames",
      result.frame_stats.size(), total_frames);

  return result;
}

bool BurstLevelAnalysisSinkStageDeps::write_csv(
    const std::string& path,
    const std::vector<FrameBurstLevelStats>& frame_stats) {
  if (frame_stats.empty()) {
    logger_.warn("BurstLevelAnalysisSinkDeps: No data to write");
    return false;
  }

  logger_.debug("BurstLevelAnalysisSinkDeps: Writing CSV to: {}", path);

  std::ofstream csv(path, std::ios::out | std::ios::trunc);
  if (!csv.is_open()) {
    logger_.error(
        "BurstLevelAnalysisSinkDeps: Failed to open file for writing: {}",
        path);
    return false;
  }

  csv << "frame_number,median_burst_10bit\n";
  size_t rows_written = 0;
  for (const auto& fs : frame_stats) {
    csv << fs.frame_number << ','
        << (fs.has_data ? fs.median_burst_10bit : std::nan("")) << '\n';
    rows_written++;
  }

  logger_.debug(
      "BurstLevelAnalysisSinkDeps: Successfully wrote {} data rows to: {}",
      rows_written, path);
  return true;
}
}  // namespace orc
