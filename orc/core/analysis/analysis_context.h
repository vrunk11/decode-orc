/*
 * File:        analysis_context.h
 * Module:      analysis
 * Purpose:     Context object passed to analysis tools during execution
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 decode-orc contributors
 */

#ifndef ORC_CORE_ANALYSIS_CONTEXT_H
#define ORC_CORE_ANALYSIS_CONTEXT_H

#if defined(ORC_GUI_BUILD)
#error \
    "GUI code cannot include core/analysis/analysis_context.h. Use AnalysisPresenter instead."
#endif

#include <orc/stage/node_id.h>
#include <orc/stage/params/stage_parameter.h>
#include <orc_analysis.h>  // For AnalysisSourceType

#include <map>
#include <memory>
#include <string>

namespace orc {

// Forward declarations
class DAG;
class Project;

/**
 * @brief Input context for running an analysis
 */
struct AnalysisContext {
  AnalysisSourceType source_type = AnalysisSourceType::LaserDisc;
  std::string source_file;  // Path to source file (legacy - prefer using
                            // dag/project)
  NodeID node_id;           // ID of node being analyzed
  std::map<std::string, ParameterValue>
      parameters;  // User-configured parameters

  // DAG execution context - preferred over source_file
  std::shared_ptr<DAG> dag;          // DAG to execute
  std::shared_ptr<Project> project;  // Project for node metadata
};

}  // namespace orc

#endif  // ORC_CORE_ANALYSIS_CONTEXT_H
