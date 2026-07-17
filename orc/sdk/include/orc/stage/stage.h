/*
 * File:        stage.h
 * Module:      decode-orc Plugin SDK (stage contract)
 * Purpose:     Base interface for all stage types
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2025-2026 Simon Inns
 */

#pragma once

#include <orc/stage/artifact.h>
#include <orc/stage/node_type.h>
#include <orc/stage/observation/observation_context.h>
#include <orc/stage/observation/observation_schema.h>
#include <orc/stage/params/stage_parameter.h>

#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>

namespace orc {

/**
 * @brief Error thrown when stage/DAG execution fails.
 *
 * Stages throw this from process() to signal unrecoverable execution
 * failures; the host DAG executor propagates it.
 */
class DAGExecutionError : public std::runtime_error {
 public:
  explicit DAGExecutionError(const std::string& msg)
      : std::runtime_error(msg) {}
};

/**
 * @brief Base interface for all processing stages
 *
 * Stages transform input artifacts into output artifacts.
 * They are pure functions of their inputs and parameters.
 *
 * All stage implementations should inherit from this interface
 * and implement the required methods.
 *
 * Design Philosophy:
 * - Stages are stateless transformations
 * - All state is in artifacts (inputs/outputs)
 * - Parameters are declarative configuration
 * - Execution is deterministic and repeatable
 */
class DAGStage {
 public:
  virtual ~DAGStage() = default;

  /**
   * @brief Get stage version string
   *
   * Used for provenance tracking and compatibility checking.
   * Should follow semantic versioning (e.g., "1.2.3").
   */
  virtual std::string version() const = 0;

  /**
   * @brief Get node type information for GUI and validation
   *
   * Describes the stage's capabilities, inputs, outputs, and parameters
   * for use in the visual DAG editor and runtime validation.
   */
  virtual NodeTypeInfo get_node_type_info() const = 0;

  /**
   * @brief Execute the stage transformation
   *
   * @param inputs Input artifacts (must match required_input_count)
   * @param parameters Configuration parameters (validated against NodeTypeInfo)
   * @param observation_context Shared observation context for pipeline
   * @return Output artifacts (count matches output_count)
   *
   * This method should be pure - same inputs and parameters always
   * produce the same outputs. No side effects except through returned artifacts
   * and observations written to the context.
   */
  virtual std::vector<ArtifactPtr> execute(
      const std::vector<ArtifactPtr>& inputs,
      const std::map<std::string, ParameterValue>& parameters,
      ObservationContext& observation_context) = 0;

  /**
   * @brief Number of required input artifacts
   *
   * The DAG executor validates that this many inputs are provided.
   * Return 0 for source stages (no inputs required).
   */
  virtual size_t required_input_count() const = 0;

  /**
   * @brief Number of output artifacts produced
   *
   * The DAG executor validates that execute() returns this many outputs.
   * Most stages return 1, but mergers and complex stages may return multiple
   * outputs.
   */
  virtual size_t output_count() const = 0;

  /**
   * @brief Declare observations required for this stage to operate
   *
   * Pipeline validation will ensure these observations are available
   * before execution begins. Most stages don't require observations.
   *
   * @return List of required observation keys
   */
  virtual std::vector<ObservationKey> get_required_observations() const {
    return {};  // Default: no required observations
  }

  /**
   * @brief Declare observations provided by this stage
   *
   * Stages may own observers and populate the context. This allows
   * pipeline validation to know what observations will be available.
   *
   * @return List of provided observation keys
   */
  virtual std::vector<ObservationKey> get_provided_observations() const {
    return {};  // Default: no provided observations
  }

  /**
   * @brief Report the stage's current configuration status for GUI display.
   *
   * Stages that have configurable parameters must call
   * set_configuration_status() in their constructor and at the end of
   * set_parameters() so the node editor can render the traffic-light status dot
   * correctly.
   *
   * Stages that require no parameters should not override this — the default
   * Green is correct.
   */
  virtual ConfigurationStatus get_configuration_status() const {
    return configuration_status_;
  }

  /**
   * @brief Return stage help text in Markdown format.
   *
   * Override in each stage implementation to provide user-visible documentation
   * rendered by the GUI help dialog. The content should explain what the stage
   * does, when to use it, its parameters, and any tools it exposes.
   *
   * Third-party plugins implement this method to supply their own documentation
   * without depending on host-side resource files.
   *
   * @return Markdown string, or empty string if no documentation is available.
   */
  virtual std::string get_instructions() const { return ""; }

 protected:
  /**
   * @brief Set the configuration status.  Call this in the constructor and
   * whenever set_parameters() completes to reflect the current state.
   */
  void set_configuration_status(ConfigurationStatus status) {
    configuration_status_ = status;
  }

 private:
  ConfigurationStatus configuration_status_{ConfigurationStatus::Green};
};

/**
 * @brief Shared pointer to a stage
 *
 * Stages are shared across the DAG and should be managed via shared_ptr.
 */
using DAGStagePtr = std::shared_ptr<DAGStage>;

}  // namespace orc
