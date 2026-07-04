/*
 * File:        frame_map_range_analysis.h
 * Module:      analysis
 * Purpose:     Frame map range locator: finds frames by picture number or CLV
 * timecode
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 decode-orc contributors
 */

#ifndef ORC_CORE_ANALYSIS_FRAME_MAP_RANGE_ANALYSIS_H
#define ORC_CORE_ANALYSIS_FRAME_MAP_RANGE_ANALYSIS_H

#include "../analysis_tool.h"

namespace orc {

/**
 * @brief Frame map range analysis tool
 *
 * Locates a frame range based on user-specified start/end addresses
 * (picture number or CLV timecode) and prepares a FrameMapStage
 * range specification.
 */
class FrameMapRangeAnalysisTool : public AnalysisTool {
 public:
  std::string id() const override;
  std::string name() const override;
  std::string description() const override;
  std::string category() const override;

  std::vector<ParameterDescriptor> parameters() const override;
  bool canAnalyze(AnalysisSourceType source_type) const override;
  bool isApplicableToStage(const std::string& stage_name) const override;
  int priority() const override { return 1; }

  AnalysisResult analyze(const AnalysisContext& ctx,
                         AnalysisProgress* progress) override;

  bool canApplyToGraph() const override;
  bool applyToGraph(AnalysisResult& result, const Project& project,
                    NodeID node_id) override;

  int estimateDurationSeconds(const AnalysisContext& ctx) const override;
};

}  // namespace orc

#endif  // ORC_CORE_ANALYSIS_FRAME_MAP_RANGE_ANALYSIS_H
