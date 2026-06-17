/*
 * File:        vectorscope_analysis.h
 * Module:      orc-core
 * Purpose:     Vectorscope analysis tool for chroma decoder
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2025-2026 Simon Inns
 */

#ifndef ORC_CORE_ANALYSIS_VECTORSCOPE_ANALYSIS_H
#define ORC_CORE_ANALYSIS_VECTORSCOPE_ANALYSIS_H

#include <frame_id.h>
#include <orc_preview_carriers.h>
#include <orc_source_parameters.h>

#include <memory>
#include <optional>

#include "../analysis_tool.h"
#include "vectorscope_data.h"

// Forward declaration (ComponentFrame is in global namespace, not orc::)
class ComponentFrame;

namespace orc {

/**
 * @brief Vectorscope visualization tool for chroma decoder output
 *
 * Extracts U/V chroma samples from the decoded output of the comb decoder for
 * real-time vectorscope display.
 */
class VectorscopeAnalysisTool : public AnalysisTool {
 public:
  std::string id() const override;
  std::string name() const override;
  std::string description() const override;
  std::string category() const override;

  std::vector<ParameterDescriptor> parameters() const override;
  bool canAnalyze(AnalysisSourceType source_type) const override;
  bool isApplicableToStage(const std::string& stage_name) const override;

  AnalysisResult analyze(const AnalysisContext& ctx,
                         AnalysisProgress* progress) override;

  bool canApplyToGraph() const override;
  bool applyToGraph(AnalysisResult& result, const Project& project,
                    NodeID node_id) override;

  int estimateDurationSeconds(const AnalysisContext& ctx) const override;

  /**
   * @brief Extract vectorscope data directly from ComponentFrame U/V channels.
   *
   * Uses the native U/V chroma values written by the comb decoder into
   * ComponentFrame, normalised to ±32767 by the blanking-to-white range.
   */
  static VectorscopeData extractFromComponentFrame(
      const ::ComponentFrame& frame,
      const ::orc::SourceParameters& video_parameters, uint64_t field_number,
      uint32_t subsample = 1);

  /**
   * @brief Extract vectorscope data from a colour preview carrier.
   *
   * Uses the decoded U/V planes already present in the carrier and can either
   * limit sampling to the active picture window or include the entire frame.
   */
  static VectorscopeData extractFromColourFrameCarrier(
      const ColourFrameCarrier& carrier, uint64_t field_number,
      uint32_t subsample = 1, bool active_area_only = true);
};

}  // namespace orc

#endif  // ORC_CORE_ANALYSIS_VECTORSCOPE_ANALYSIS_H
