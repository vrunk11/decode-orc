/*
 * File:        filtergraph_export.h
 * Module:      orc-presenters
 * Purpose:     Serialize a project's DAG into an ffmpeg-style filtergraph
 *              string equivalent to the one --filter accepts, so a project
 *              built in the GUI (or loaded from a .orcprj file) can be
 *              copied out as a ready-to-run CLI command.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2025-2026 Simon Inns
 *
 * Shared between orc-cli and orc-gui (both link orc-presenters): this is
 * the reverse of filtergraph_parser.h, which turns such a string back into
 * a project.
 */

#pragma once

#include <string>

namespace orc {
namespace presenters {

class IProjectPresenter;

/**
 * @brief Serialize the current project into an ffmpeg-style filtergraph
 * string equivalent to --filter's argument.
 *
 * Every node becomes its own labelled filterchain (`[in1][in2] stage=k=v
 * [out]`), connected via explicit link labels rather than relying on comma
 * auto-chaining — this makes the output correct for any topology (linear
 * chains, multi-input mergers, fan-out) regardless of how the nodes were
 * created, at the cost of being slightly more verbose than a hand-written
 * linear chain would be. The result is always accepted by
 * `parse_filtergraph()` (round-trip safe), with parameter values quoted or
 * escaped as needed.
 *
 * Nodes with no incoming edges (sources) are listed first, followed by the
 * rest in their existing order, purely for readability — the grammar does
 * not require any particular order since labels resolve globally.
 *
 * A node's user-assigned display label (as set in the GUI) is not part of
 * the output: only the stage name, its parameters, and the DAG topology are
 * preserved, since that's all `--filter`/`--input`/`--filters`/`--output`
 * can express.
 *
 * @param presenter The project to serialize (its current node/edge/parameter
 * state, via getNodes()/getEdges()/getNodeParameters()). Non-const because
 * IProjectPresenter::getNodeParameters() is itself non-const.
 * @return A filtergraph string usable directly as `--filter`'s argument, or
 * an empty string if the project has no nodes.
 */
std::string export_project_as_filtergraph(IProjectPresenter& presenter);

}  // namespace presenters
}  // namespace orc
