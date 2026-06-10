/*
 * File:        analysis_result.h
 * Module:      analysis
 * Purpose:     Result type alias for analysis tool output
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 decode-orc contributors
 */

#ifndef ORC_CORE_ANALYSIS_RESULT_H
#define ORC_CORE_ANALYSIS_RESULT_H

#if defined(ORC_GUI_BUILD)
#error \
    "GUI code cannot include core/analysis/analysis_result.h. Use AnalysisPresenter instead."
#endif

// Use view-type definitions for analysis results
#include <orc_analysis.h>

// Core headers simply include the view-type and use it directly

#endif  // ORC_CORE_ANALYSIS_RESULT_H
