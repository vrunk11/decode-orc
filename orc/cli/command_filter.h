/*
 * File:        command_filter.h
 * Module:      orc-cli
 * Purpose:     Build and process a DAG from an ffmpeg-style filtergraph
 *              string (or an input/filters/output triad) instead of a
 *              .orcprj project file.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2025-2026 Simon Inns
 */

#pragma once

#include <string>

namespace orc {
namespace cli {

/**
 * @brief Options for the filter command.
 *
 * Exactly one of the two modes below is used, chosen by which fields are
 * non-empty:
 *
 * - Graph-string mode: `filtergraph` holds a full ffmpeg-style filtergraph
 *   (sources, transforms, sinks, labels — anything the grammar supports).
 * - Triad mode: `input_stages` / `filters_stages` / `output_stages` each hold
 *   a (possibly comma-chained) fragment restricted to one stage category —
 *   source stages, processing stages, and sink stages respectively. This is
 *   friendlier for the common linear pipeline and is validated per category
 *   (see filter_command()).
 *
 * There is no video/source-format option: video format and source type are
 * inherent to the stage modules used (e.g. an NTSC-only source, or the
 * y_path/c_path/input_path parameters actually supplied) and are detected
 * automatically. There is likewise no project-name option: the in-memory
 * project name is an internal placeholder unless `export_project_path` is
 * used, in which case it becomes the saved project's name.
 */
struct FilterOptions {
  std::string filtergraph;     ///< Graph-string mode: full filtergraph.
  std::string input_stages;    ///< Triad mode: input (source) stage(s).
  std::string filters_stages;  ///< Triad mode: processing stage(s).
  std::string output_stages;   ///< Triad mode: output (sink) stage(s).

  /// When non-empty, save the assembled project to this .orcprj path
  /// instead of running it (e.g. for later editing in the GUI, or reuse
  /// with --process). No sinks are triggered when this is set.
  std::string export_project_path;

  /// When true, print the equivalent --filter graph string for the
  /// assembled project to stdout instead of running it. Mutually exclusive
  /// with export_project_path (only one "instead of running" mode at a
  /// time).
  bool print_as_filter = false;
};

/**
 * @brief Execute a filtergraph decode run, or export it instead of running.
 *
 * Parses the filtergraph (or composes and validates the input/filters/output
 * triad), auto-detects the project's video format and source type from the
 * stages actually used, and builds an in-memory project via the project
 * presenter. What happens next depends on the options:
 *
 * - Normally, triggers all sink nodes and returns their combined result.
 * - If `export_project_path` is set, saves the assembled project as a
 *   .orcprj file instead (for opening in the GUI, or later use with
 *   `--process`) and does not run anything.
 * - If `print_as_filter` is set, prints the equivalent `--filter` graph
 *   string for the assembled project to stdout instead of running it —
 *   useful as a canonical, re-parseable form, or to sanity-check how the
 *   input/filters/output triad composed.
 *
 * @param options Filtergraph or triad options.
 * @return Exit code (0 = success, non-zero = error).
 */
int filter_command(const FilterOptions& options);

}  // namespace cli
}  // namespace orc
