/*
 * File:        snr_analysis_sink_deps.cpp
 * Module:      orc-core
 * Purpose:     SNRAnalysisSinkStage dependency implementation
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 decode-orc contributors
 */

#include "snr_analysis_sink_deps.h"

#include <cmath>
#include <fstream>
#include <utility>
#include <variant>

#include "field_id.h"

namespace orc {
void SNRAnalysisSinkStageDeps::init(TriggerProgressCallback progress_callback,
                                    std::atomic<bool>* cancel_requested) {
  progress_callback_ = std::move(progress_callback);
  cancel_requested_ = cancel_requested;
}

SNRAnalysisComputeResult SNRAnalysisSinkStageDeps::compute_and_analyze(
    VideoFrameRepresentation* representation,
    IObservationContext& observation_context,
    SNRAnalysisComputeOptions options) {
  if (!representation) {
    return {false, "Input representation is null", {}, 0};
  }

  (void)options.output_path;
  (void)options.write_csv;

  SNRAnalysisComputeResult result;
  result.success = true;
  result.message = "SNR analysis complete";

  const auto frame_rng = representation->frame_range();
  const uint64_t total_frames = frame_rng.count();

  if (total_frames == 0) {
    logger_.warn("SNRAnalysisSinkDeps: No frames available");
    result.total_frames = 0;
    return result;
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
      "SNRAnalysisSinkDeps: {} frames → {} buckets (~{} samples/bucket)",
      total_frames, bucket_count, kSamplesPerBucket);

  result.frame_stats.reserve(static_cast<size_t>(bucket_count));

  for (uint64_t b = 0; b < bucket_count; ++b) {
    if (cancel_requested_ && cancel_requested_->load()) {
      logger_.warn("SNRAnalysisSinkDeps: Cancel requested at bucket {}", b);
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

    double white_sum = 0.0;
    double black_sum = 0.0;
    size_t white_count = 0;
    size_t black_count = 0;

    for (uint64_t s = 0; s < n_samples; ++s) {
      // Evenly distribute sample frames across the bucket so the first and last
      // frames are always included.
      const FrameID fid =
          (n_samples == 1U)
              ? bucket_start
              : bucket_start + (s * (bucket_size - 1U)) / (n_samples - 1U);

      if (options.snr_mode == SNRAnalysisMode::WHITE ||
          options.snr_mode == SNRAnalysisMode::BOTH) {
        white_snr_observer_.process_frame(*representation, fid,
                                          observation_context);
      }
      if (options.snr_mode == SNRAnalysisMode::BLACK ||
          options.snr_mode == SNRAnalysisMode::BOTH) {
        black_psnr_observer_.process_frame(*representation, fid,
                                           observation_context);
      }

      const FieldID frame_fid(fid * 2U);

      auto white_val =
          observation_context.get(frame_fid, "white_snr", "snr_db");
      if (white_val && std::holds_alternative<double>(*white_val)) {
        white_sum += std::get<double>(*white_val);
        ++white_count;
      }

      auto black_val =
          observation_context.get(frame_fid, "black_psnr", "psnr_db");
      if (black_val && std::holds_alternative<double>(*black_val)) {
        black_sum += std::get<double>(*black_val);
        ++black_count;
      }

      observation_context.clear_field(frame_fid);
    }

    FrameSNRStats frame_stat;
    // Use the center frame of the bucket as the representative frame number
    // (1-based for display).
    frame_stat.frame_number =
        static_cast<int32_t>(bucket_start + (bucket_end - bucket_start) / 2U) +
        1;

    if (white_count > 0) {
      frame_stat.white_snr = white_sum / static_cast<double>(white_count);
      frame_stat.white_snr_count = white_count;
      frame_stat.has_white_snr = true;
    }
    if (black_count > 0) {
      frame_stat.black_psnr = black_sum / static_cast<double>(black_count);
      frame_stat.black_psnr_count = black_count;
      frame_stat.has_black_psnr = true;
    }
    frame_stat.has_data = frame_stat.has_white_snr || frame_stat.has_black_psnr;
    frame_stat.field_count = std::max(white_count, black_count);

    result.frame_stats.push_back(frame_stat);

    if (progress_callback_ && (b % 50 == 0 || b + 1 == bucket_count)) {
      progress_callback_(b + 1, bucket_count,
                         "Analysing bucket " + std::to_string(b + 1) + "/" +
                             std::to_string(bucket_count));
    }
  }

  result.total_frames = static_cast<int32_t>(total_frames);
  logger_.debug("SNRAnalysisSinkDeps: Complete — {} buckets from {} frames",
                result.frame_stats.size(), total_frames);

  return result;
}

bool SNRAnalysisSinkStageDeps::write_csv(
    const std::string& path, const std::vector<FrameSNRStats>& frame_stats) {
  if (frame_stats.empty()) {
    logger_.warn("SNRAnalysisSinkDeps: No data to write");
    return false;
  }

  logger_.debug("SNRAnalysisSinkDeps: Writing CSV to: {}", path);

  std::ofstream csv(path, std::ios::out | std::ios::trunc);
  if (!csv.is_open()) {
    logger_.error("SNRAnalysisSinkDeps: Failed to open file for writing: {}",
                  path);
    return false;
  }

  csv << "frame_number,white_snr_db,black_psnr_db\n";
  size_t rows_written = 0;
  for (const auto& fs : frame_stats) {
    if (fs.has_data) {
      csv << fs.frame_number << ','
          << (fs.has_white_snr ? fs.white_snr : std::nan("")) << ','
          << (fs.has_black_psnr ? fs.black_psnr : std::nan("")) << '\n';
      rows_written++;
    }
  }

  logger_.debug("SNRAnalysisSinkDeps: Successfully wrote {} data rows to: {}",
                rows_written, path);
  return true;
}
}  // namespace orc
