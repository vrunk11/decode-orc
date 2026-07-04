/*
 * File:        node_type_helper_test.cpp
 * Module:      orc-tests/gui/unit
 * Purpose:     Unit tests for NodeTypeHelper namespace functions
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#include <gtest/gtest.h>

#include <QPointF>
#include <QString>

// Include headers from their installed locations
#include <orc/stage/node_type.h>

#include "node_type_helper.h"

namespace gui_unit_test {

// =============================================================================
// NodeVisualInfo Structure Tests
// =============================================================================

TEST(NodeTypeHelperTest, GetVisualInfo_ReturnsDefaultForUnknownStage) {
  // Verify that unknown stages return a sensible default (no crash)
  auto info = NodeTypeHelper::getVisualInfo("UnknownStageName");

  // Should return some default info rather than crash
  // Exact behavior depends on implementation, but should be safe
  EXPECT_FALSE(info.input_is_many);  // Most stages don't need many inputs
  EXPECT_FALSE(
      info.output_is_many);  // Most stages don't produce multiple outputs
}

TEST(NodeTypeHelperTest, GetVisualInfoSourceStage_HasOutput) {
  // Source stages should have output but no input
  auto info = NodeTypeHelper::getVisualInfo("PALYCSource");

  EXPECT_FALSE(info.has_input);  // Sources have no input
  EXPECT_TRUE(info.has_output);  // Sources produce output
}

TEST(NodeTypeHelperTest, GetVisualInfoTransformStage_HasInputAndOutput) {
  // Transform stages should have both input and output
  auto info = NodeTypeHelper::getVisualInfo("FieldInvert");

  EXPECT_TRUE(info.has_input);       // Transforms have input
  EXPECT_TRUE(info.has_output);      // Transforms produce output
  EXPECT_FALSE(info.input_is_many);  // Transforms typically have single input
}

TEST(NodeTypeHelperTest, GetVisualInfoSinkStage_HasInputNoOutput) {
  // Sink stages should have input but no output
  auto info = NodeTypeHelper::getVisualInfo("RawVideoSink");

  EXPECT_TRUE(info.has_input);    // Sinks consume input
  EXPECT_FALSE(info.has_output);  // Sinks don't produce output
}

// =============================================================================
// Port Position Tests
// =============================================================================

TEST(NodeTypeHelperTest, GetInputPortPosition_IsOnLeftEdge) {
  const double node_width = 100.0;
  const double node_height = 60.0;

  auto pos = NodeTypeHelper::getInputPortPosition(node_width, node_height);

  EXPECT_EQ(pos.x(), 0.0);                // Left edge
  EXPECT_EQ(pos.y(), node_height / 2.0);  // Middle height
}

TEST(NodeTypeHelperTest, Get_InputPortPositionMultipleHeights) {
  // Test with different node heights to ensure vertical centering
  std::vector<double> heights = {40.0, 60.0, 100.0};

  for (double h : heights) {
    auto pos = NodeTypeHelper::getInputPortPosition(100.0, h);
    EXPECT_EQ(pos.x(), 0.0);
    EXPECT_EQ(pos.y(), h / 2.0);
  }
}

TEST(NodeTypeHelperTest, GetOutputPortPosition_IsOnRightEdge) {
  const double node_width = 100.0;
  const double node_height = 60.0;

  auto pos = NodeTypeHelper::getOutputPortPosition(node_width, node_height);

  EXPECT_EQ(pos.x(), node_width);         // Right edge
  EXPECT_EQ(pos.y(), node_height / 2.0);  // Middle height
}

TEST(NodeTypeHelperTest, Get_OutputPortPositionMultipleWidths) {
  // Test with different node widths
  std::vector<double> widths = {80.0, 100.0, 150.0};

  for (double w : widths) {
    auto pos = NodeTypeHelper::getOutputPortPosition(w, 60.0);
    EXPECT_EQ(pos.x(), w);
    EXPECT_EQ(pos.y(), 60.0 / 2.0);
  }
}

TEST(NodeTypeHelperTest, Port_PositionsAlignOnYAxis) {
  // Input and output ports of the same node should align vertically
  const double width = 100.0;
  const double height = 60.0;

  auto input_pos = NodeTypeHelper::getInputPortPosition(width, height);
  auto output_pos = NodeTypeHelper::getOutputPortPosition(width, height);

  EXPECT_EQ(input_pos.y(), output_pos.y());
}

// =============================================================================
// Connection Validity Tests
// =============================================================================

TEST(NodeTypeHelperTest, Can_ConnectSourceToTransform) {
  // A source can typically connect to a transform
  bool can = NodeTypeHelper::canConnect("PALYCSource",  // source_stage
                                        "FieldInvert",  // target_stage
                                        0,  // existing_input_count on target
                                        0   // existing_output_count on source
  );

  EXPECT_TRUE(can);
}

TEST(NodeTypeHelperTest, Can_ConnectTransformToSink) {
  // A transform can typically connect to a sink
  bool can = NodeTypeHelper::canConnect("FieldInvert",   // source_stage
                                        "RawVideoSink",  // target_stage
                                        0,  // existing_input_count on target
                                        0   // existing_output_count on source
  );

  EXPECT_TRUE(can);
}

TEST(NodeTypeHelperTest, CanConnectSourceToSource_Fails) {
  // Source to source should not be allowed (sources produce output, don't
  // accept input)
  bool can = NodeTypeHelper::canConnect(
      "PALYCSource",     // source_stage
      "NTSCCompSource",  // target_stage (also a source)
      0,                 // existing_input_count
      0                  // existing_output_count
  );

  EXPECT_FALSE(can);
}

TEST(NodeTypeHelperTest, CanConnectSinkToSink_Fails) {
  // Sink to sink should not be allowed (sinks consume input, don't produce
  // output)
  bool can = NodeTypeHelper::canConnect(
      "RawVideoSink",     // source_stage (a sink)
      "FFmpegVideoSink",  // target_stage (also a sink)
      0,                  // existing_input_count
      0                   // existing_output_count
  );

  EXPECT_FALSE(can);
}

TEST(NodeTypeHelperTest, CanConnectExceedsTargetInputLimit_Fails) {
  // If target already has max inputs reached, should not allow new connection
  bool can =
      NodeTypeHelper::canConnect("FieldInvert",   // source_stage
                                 "RawVideoSink",  // target_stage (1 input max)
                                 1,  // existing_input_count on target = at max
                                 0   // existing_output_count on source
      );

  EXPECT_FALSE(can);
}

TEST(NodeTypeHelperTest, CanConnectExceedsSourceOutputLimit_Fails) {
  // If source already has max outputs reached, should not allow new connection
  bool can =
      NodeTypeHelper::canConnect("FieldInvert",  // source_stage (1 output max)
                                 "DropoutCorrect",  // target_stage
                                 0,  // existing_input_count on target
                                 1   // existing_output_count on source = at max
      );

  EXPECT_FALSE(can);
}

TEST(NodeTypeHelperTest, CanConnectUnknownSourceStage_Fails) {
  // Unknown stages should fail connection validation
  bool can = NodeTypeHelper::canConnect("UnknownSource",  // source_stage
                                        "FieldInvert",    // target_stage
                                        0,  // existing_input_count
                                        0   // existing_output_count
  );

  EXPECT_FALSE(can);
}

TEST(NodeTypeHelperTest, CanConnectUnknownTargetStage_Fails) {
  // Unknown target stages should fail connection validation
  bool can = NodeTypeHelper::canConnect("FieldInvert",    // source_stage
                                        "UnknownTarget",  // target_stage
                                        0,  // existing_input_count
                                        0   // existing_output_count
  );

  EXPECT_FALSE(can);
}

}  // namespace gui_unit_test
