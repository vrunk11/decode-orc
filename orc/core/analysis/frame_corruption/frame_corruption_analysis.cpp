/*
 * File:        frame_corruption_analysis.cpp
 * Module:      orc-core/analysis
 * Purpose:     Frame corruption analysis tool implementation
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2025-2026 Simon Inns
 */

#include "frame_corruption_analysis.h"

#include <frame_numbering.h>
#include <orc/stage/video_frame_representation.h>
#include <orc/support/logging.h>

#include <algorithm>
#include <cstdint>
#include <ctime>
#include <sstream>

#include "../../include/dag_executor.h"
#include "../../include/project.h"
#include "../analysis_registry.h"
#include "frame_corruption_analyzer.h"

namespace orc {

// Force linker to include this object file (for static registration)
void force_link_FrameCorruptionAnalysisTool() {}

std::string FrameCorruptionAnalysisTool::id() const {
  return "frame_corruption";
}

std::string FrameCorruptionAnalysisTool::name() const {
  return "Frame Corruption Generator";
}

std::string FrameCorruptionAnalysisTool::description() const {
  return "Generate corruption patterns (skips, repeats, gaps) for testing disc "
         "mapper";
}

std::string FrameCorruptionAnalysisTool::category() const { return "Testing"; }

std::vector<ParameterDescriptor> FrameCorruptionAnalysisTool::parameters()
    const {
  std::vector<ParameterDescriptor> params;

  // Pattern parameter with dropdown
  {
    ParameterDescriptor desc;
    desc.name = "pattern";
    desc.display_name = "Corruption Pattern";
    desc.description = "Type of corruption pattern to generate";
    desc.type = ParameterType::STRING;
    desc.constraints.allowed_strings = {
        "simple-skip",  "simple-repeat", "skip-with-gap", "heavy-skip",
        "heavy-repeat", "mixed-light",   "mixed-heavy"};
    desc.constraints.default_value = std::string("simple-skip");
    desc.constraints.required = true;
    params.push_back(desc);
  }

  return params;
}

std::vector<ParameterDescriptor>
FrameCorruptionAnalysisTool::parametersForContext(
    const AnalysisContext& ctx) const {
  auto params = parameters();

  // Check if a seed already exists in the node parameters
  bool has_existing_seed = false;
  if (ctx.dag && ctx.node_id.is_valid()) {
    const auto& dag_nodes = ctx.dag->nodes();
    auto node_it = std::find_if(
        dag_nodes.begin(), dag_nodes.end(),
        [&ctx](const DAGNode& node) { return node.node_id == ctx.node_id; });

    if (node_it != dag_nodes.end()) {
      auto seed_it = node_it->parameters.find("seed");
      if (seed_it != node_it->parameters.end()) {
        if (auto* val = std::get_if<int32_t>(&seed_it->second)) {
          has_existing_seed = (*val != 0);
        }
      }
    }
  }

  // Only show regenerate_seed checkbox if a seed already exists
  if (has_existing_seed) {
    ParameterDescriptor desc;
    desc.name = "regenerate_seed";
    desc.display_name = "Regenerate Seed";
    desc.description =
        "Generate new random seed (unchecked = reuse previous seed for "
        "reproducibility)";
    desc.type = ParameterType::BOOL;
    desc.constraints.default_value = false;  // Default to reusing existing seed
    desc.constraints.required = false;
    params.push_back(desc);
  }

  return params;
}

bool FrameCorruptionAnalysisTool::canAnalyze(
    AnalysisSourceType source_type) const {
  // This is a generator tool - works with any source type (doesn't actually use
  // the source)
  (void)source_type;
  return true;
}

bool FrameCorruptionAnalysisTool::isApplicableToStage(
    const std::string& stage_name) const {
  return stage_name == "frame_map";
}

AnalysisResult FrameCorruptionAnalysisTool::analyze(
    const AnalysisContext& ctx, AnalysisProgress* progress) {
  AnalysisResult result;
  result.status = AnalysisResult::Success;

  if (progress) {
    progress->setStatus("Initializing corruption generator...");
    progress->setProgress(0);
  }

  // Get existing seed from node parameters (if any)
  int32_t existing_seed = 0;
  if (ctx.dag && ctx.node_id.is_valid()) {
    const auto& dag_nodes = ctx.dag->nodes();
    auto node_it = std::find_if(
        dag_nodes.begin(), dag_nodes.end(),
        [&ctx](const DAGNode& node) { return node.node_id == ctx.node_id; });

    if (node_it != dag_nodes.end()) {
      auto seed_it = node_it->parameters.find("seed");
      if (seed_it != node_it->parameters.end()) {
        if (auto* val = std::get_if<int32_t>(&seed_it->second)) {
          existing_seed = *val;
        }
      }
    }
  }

  // Get input frame count by executing the input node
  uint64_t frame_count = 0;

  if (ctx.dag && ctx.node_id.is_valid()) {
    const auto& dag_nodes = ctx.dag->nodes();
    auto node_it = std::find_if(
        dag_nodes.begin(), dag_nodes.end(),
        [&ctx](const DAGNode& node) { return node.node_id == ctx.node_id; });

    if (node_it != dag_nodes.end() && !node_it->input_node_ids.empty()) {
      try {
        orc::NodeID input_node_id = node_it->input_node_ids[0];
        orc::DAGExecutor executor;
        auto all_outputs = executor.execute_to_node(*ctx.dag, input_node_id);
        auto output_it = all_outputs.find(input_node_id);

        if (output_it != all_outputs.end() && !output_it->second.empty()) {
          for (const auto& artifact : output_it->second) {
            auto vfr =
                std::dynamic_pointer_cast<const orc::VideoFrameRepresentation>(
                    artifact);
            if (vfr) {
              frame_count = vfr->frame_count();
              ORC_LOG_DEBUG(
                  "Frame Corruption Generator: Got frame count {} from input",
                  frame_count);
              break;
            }
          }
        }
      } catch (const std::exception& e) {
        ORC_LOG_WARN(
            "Frame Corruption Generator: Failed to execute input node: {}",
            e.what());
      }
    }
  }

  if (frame_count == 0) {
    result.status = AnalysisResult::Failed;
    result.summary =
        "Cannot determine input frame count.\n\n"
        "Frame Corruption Generator requires a VideoFrameRepresentation "
        "input.\n"
        "Ensure this frame_map stage has an input connection.";
    return result;
  }

  // Extract pattern parameter
  std::string pattern_name = "simple-skip";
  auto it_pattern = ctx.parameters.find("pattern");
  if (it_pattern != ctx.parameters.end()) {
    if (auto* val = std::get_if<std::string>(&it_pattern->second)) {
      pattern_name = *val;
    }
  }

  // Check regenerate_seed parameter (only exists if seed was already set)
  bool regenerate_seed = false;  // Default to reusing existing seed
  auto it_regen = ctx.parameters.find("regenerate_seed");
  if (it_regen != ctx.parameters.end()) {
    if (auto* val = std::get_if<bool>(&it_regen->second)) {
      regenerate_seed = *val;
    }
  }

  // Generate seed based on regenerate_seed setting and existing seed
  uint32_t seed;
  if (existing_seed != 0 && !regenerate_seed) {
    // Reuse existing seed
    seed = static_cast<uint32_t>(existing_seed);
  } else {
    // Generate new seed
    seed = static_cast<uint32_t>(std::time(nullptr));
  }

  // Find pattern config
  auto patterns = FrameCorruptionAnalyzer::get_all_patterns();
  auto pattern_it =
      std::find_if(patterns.begin(), patterns.end(),
                   [&](const FrameCorruptionAnalyzer::PatternConfig& p) {
                     return p.name == pattern_name;
                   });

  if (pattern_it == patterns.end()) {
    result.status = AnalysisResult::Failed;
    result.summary = "Unknown corruption pattern: " + pattern_name;
    return result;
  }

  if (progress) {
    progress->setProgress(10);
    progress->setStatus("Generating corruption pattern...");
  }

  // Create analyzer and generate
  FrameCorruptionAnalyzer analyzer(frame_count, *pattern_it, seed);
  auto analysis_result = analyzer.analyze();

  if (progress) {
    progress->setProgress(90);
    progress->setStatus("Formatting results...");
  }

  // Build result
  if (!analysis_result.success) {
    result.status = AnalysisResult::Failed;
  }

  // Build summary
  std::ostringstream summary;
  summary << "Pattern: " << pattern_it->name << "\n";
  summary << "Description: " << pattern_it->description << "\n";
  summary << "Input frames: " << frame_count << "\n";
  summary << "Seed: " << seed;
  if (existing_seed != 0) {
    summary << (regenerate_seed ? " (regenerated)" : " (reused)");
  } else {
    summary << " (new)";
  }
  summary << "\n\n";

  // Add frame mapping specification preview. Shown 1-based to match the
  // Frame Map parameter dialog; the applied spec stays 0-based internally.
  const std::string display_spec =
      range_spec_to_presentation(analysis_result.mapping_spec);
  summary << "==================================================\n";
  summary << "Generated Frame Mapping Specification:\n";
  summary << "==================================================\n";
  if (display_spec.length() > 500) {
    summary << display_spec.substr(0, 500) << "...\n\n";
    summary << "(Full specification: " << display_spec.length()
            << " characters)\n";
    summary << "This will be applied to the 'ranges' parameter when you click "
               "'Apply to Node'\n";
  } else {
    summary << display_spec << "\n";
  }

  result.summary = summary.str();

  // Store mapping spec and seed for graph application
  result.graphData["ranges"] = analysis_result.mapping_spec;
  result.graphData["seed"] = std::to_string(seed);
  result.graphData["rationale"] = analysis_result.rationale;

  // Statistics
  result.statistics["normalFrames"] =
      static_cast<int64_t>(analysis_result.stats.normal_frames);
  result.statistics["repeatedFrames"] =
      static_cast<int64_t>(analysis_result.stats.repeated_frames);
  result.statistics["skippedFrames"] =
      static_cast<int64_t>(analysis_result.stats.skipped_frames);
  result.statistics["gapMarkers"] =
      static_cast<int64_t>(analysis_result.stats.gap_markers);
  result.statistics["totalOutputFrames"] =
      static_cast<int64_t>(analysis_result.stats.total_output_frames);
  result.statistics["patternName"] = pattern_it->name;
  result.statistics["seed"] = static_cast<int64_t>(seed);

  // Add mapping spec as info item FIRST so it's visible at the top
  AnalysisResult::ResultItem spec_item;
  spec_item.type = "info";
  std::ostringstream spec_msg;
  spec_msg << "==================================================\n";
  spec_msg << "Generated Frame Mapping Specification\n";
  spec_msg << "==================================================\n\n";

  // Show a preview if it's long (1-based display, like the summary above)
  if (display_spec.length() > 500) {
    spec_msg << display_spec.substr(0, 500) << "...\n\n";
    spec_msg << "(Full specification: " << display_spec.length()
             << " characters)\n";
    spec_msg << "Click 'Apply to Node' to use this specification\n";
  } else {
    spec_msg << display_spec << "\n";
  }
  spec_item.message = spec_msg.str();
  result.items.push_back(spec_item);

  // Add corruption events as result items
  if (!analysis_result.events.empty()) {
    AnalysisResult::ResultItem separator_item;
    separator_item.type = "info";
    separator_item.message =
        "\n==================================================\nCorruption "
        "Events Applied\n==================================================\n";
    result.items.push_back(separator_item);
  }

  for (const auto& event : analysis_result.events) {
    AnalysisResult::ResultItem item;
    item.type =
        (event.type == FrameCorruptionAnalyzer::CorruptionEvent::SKIP) ? "skip"
        : (event.type == FrameCorruptionAnalyzer::CorruptionEvent::REPEAT)
            ? "repeat"
            : "gap";
    item.message = event.to_string();
    item.startFrame = static_cast<int>(event.start_frame);
    item.endFrame = static_cast<int>(event.end_frame);
    item.metadata["count"] = static_cast<int>(event.count);
    result.items.push_back(item);

    if (progress) {
      progress->reportPartialResult(item);
    }
  }

  if (progress) {
    progress->setProgress(100);
    progress->setStatus("Complete");
  }

  ORC_LOG_DEBUG(
      "Frame corruption analysis complete: {} events, {} output frames",
      analysis_result.events.size(), analysis_result.stats.total_output_frames);

  return result;
}

bool FrameCorruptionAnalysisTool::canApplyToGraph() const { return true; }

bool FrameCorruptionAnalysisTool::applyToGraph(AnalysisResult& result,
                                               const Project& project,
                                               NodeID node_id) {
  // Find the ranges in graphData
  auto it_ranges = result.graphData.find("ranges");

  if (it_ranges == result.graphData.end()) {
    ORC_LOG_ERROR("No ranges data found in corruption analysis result");
    return false;
  }

  // Find the seed in graphData
  auto it_seed = result.graphData.find("seed");
  if (it_seed == result.graphData.end()) {
    ORC_LOG_ERROR("No seed data found in corruption analysis result");
    return false;
  }

  // Get the node (using Project's public API via project_io)
  auto nodes = project.get_nodes();
  auto node_it = std::find_if(
      nodes.begin(), nodes.end(),
      [&](const ProjectDAGNode& node) { return node.node_id == node_id; });

  if (node_it == nodes.end()) {
    ORC_LOG_ERROR("Node {} not found in project", node_id);
    return false;
  }

  if (node_it->stage_name != "frame_map") {
    ORC_LOG_ERROR("Node {} is not a frame_map stage (type: {})", node_id,
                  node_it->stage_name);
    return false;
  }

  // Parse seed string to int32
  int32_t seed_value;
  try {
    seed_value = std::stoi(it_seed->second);
  } catch (...) {
    ORC_LOG_ERROR("Failed to parse seed value: {}", it_seed->second);
    return false;
  }

  // Populate parameterChanges instead of modifying project directly
  result.parameterChanges["ranges"] = it_ranges->second;
  result.parameterChanges["seed"] = seed_value;

  ORC_LOG_DEBUG("Prepared corruption pattern for node {}: ranges={}, seed={}",
                node_id, it_ranges->second, it_seed->second);

  return true;
}

int FrameCorruptionAnalysisTool::estimateDurationSeconds(
    const AnalysisContext& /*ctx*/) const {
  // Very fast - just generates a string
  return 1;
}

// Register the tool
REGISTER_ANALYSIS_TOOL(FrameCorruptionAnalysisTool);

}  // namespace orc
