/*
 * File:        observation_cache.cpp
 * Module:      orc-core
 * Purpose:     Universal cache for observer data implementation
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2025-2026 Simon Inns
 */

#include "include/observation_cache.h"

#include <orc/stage/logging.h>

#include <algorithm>

#include "include/dag_frame_renderer.h"

namespace orc {

ObservationCache::ObservationCache(std::shared_ptr<const DAG> dag)
    : dag_(dag), cache_(500), frame_count_cache_(100) {
  if (!dag_) {
    throw std::invalid_argument("ObservationCache: DAG cannot be null");
  }

  renderer_ = std::make_shared<DAGFrameRenderer>(dag_);
}

void ObservationCache::update_dag(std::shared_ptr<const DAG> dag) {
  if (!dag) {
    throw std::invalid_argument("ObservationCache: DAG cannot be null");
  }

  dag_ = dag;
  renderer_ = std::make_shared<DAGFrameRenderer>(dag_);
  clear();
}

void ObservationCache::clear() {
  cache_.clear();
  frame_count_cache_.clear();
  ORC_LOG_DEBUG("ObservationCache: All cached observations cleared");
}

void ObservationCache::clear_node([[maybe_unused]] NodeID node_id) {
  cache_.clear();
  frame_count_cache_.clear();
  ORC_LOG_DEBUG(
      "ObservationCache: Cleared all observations (requested for node '{}')",
      node_id.to_string());
}

size_t ObservationCache::get_frame_count(NodeID node_id) {
  auto cached_count = frame_count_cache_.get(node_id);
  if (cached_count.has_value()) {
    return cached_count.value();
  }

  // Render frame 0 to get the representation and read frame_count() from it
  if (!render_and_cache(node_id, FrameID{0})) {
    ORC_LOG_WARN("ObservationCache: Failed to get frame count for node '{}'",
                 node_id.to_string());
    return 0;
  }

  // Ask the renderer for the cached representation to read frame_count
  auto result = renderer_->render_frame_at_node(node_id, FrameID{0});
  if (!result.is_valid || !result.representation) {
    return 0;
  }

  size_t count = result.representation->frame_count();
  frame_count_cache_.put(node_id, count);
  return count;
}

bool ObservationCache::render_and_cache(NodeID node_id, FrameID frame_id) {
  try {
    auto result = renderer_->render_frame_at_node(node_id, frame_id);

    if (!result.is_valid || !result.representation) {
      bool is_sink = false;
      if (dag_) {
        const auto& dag_nodes = dag_->nodes();
        auto dag_node_it = std::find_if(
            dag_nodes.begin(), dag_nodes.end(),
            [&node_id](const auto& n) { return n.node_id == node_id; });

        if (dag_node_it != dag_nodes.end() && dag_node_it->stage) {
          auto node_type = dag_node_it->stage->get_node_type_info().type;
          is_sink = (node_type == NodeType::SINK ||
                     node_type == NodeType::ANALYSIS_SINK);
        }
      }

      if (is_sink) {
        ORC_LOG_DEBUG(
            "ObservationCache: Sink node '{}' has no frame representation "
            "(expected)",
            node_id.to_string());
      } else {
        ORC_LOG_WARN(
            "ObservationCache: Failed to render frame {} at node '{}': {}",
            frame_id, node_id.to_string(), result.error_message);
      }
      return false;
    }

    CacheKey key{node_id, frame_id};
    cache_.put(key, true);
    return true;

  } catch (const std::exception& e) {
    ORC_LOG_ERROR(
        "ObservationCache: Exception rendering frame {} at node '{}': {}",
        frame_id, node_id.to_string(), e.what());
    return false;
  }
}

bool ObservationCache::get_field(NodeID node_id, FieldID field_id) {
  // Derive the parent FrameID from the FieldID
  FrameID frame_id = static_cast<FrameID>(field_id.value() / 2);

  CacheKey key{node_id, frame_id};
  auto cached = cache_.get(key);
  if (cached.has_value()) {
    return cached.value();
  }

  return render_and_cache(node_id, frame_id);
}

const ObservationContext& ObservationCache::get_observation_context() const {
  if (!renderer_) {
    static ObservationContext empty_context;
    return empty_context;
  }
  return renderer_->get_observation_context();
}

}  // namespace orc
