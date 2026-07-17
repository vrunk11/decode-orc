/*
 * File:        observation_cache.h
 * Module:      orc-core
 * Purpose:     Universal cache for observer data across video source
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2025-2026 Simon Inns
 */

#ifndef ORC_CORE_OBSERVATION_CACHE_H
#define ORC_CORE_OBSERVATION_CACHE_H

#include <orc/stage/field_id.h>
#include <orc/stage/frame_id.h>
#include <orc/stage/node_id.h>
#include <orc/support/lru_cache.h>

#include <memory>
#include <string>

namespace orc {

// Forward declarations
class DAG;
class DAGFrameRenderer;
class ObservationContext;

/**
 * @brief Universal cache for rendered frame observations across the video
 * source
 *
 * Provides centralized caching of frame renders (and their observer output) for
 * all frames at any node in the DAG. Analysis dialogs (VBI, dropout, etc.) can
 * share this cache, avoiding redundant frame rendering.
 *
 * Observer output (VBI data, quality metrics, etc.) is stored in the
 * ObservationContext of the underlying DAGFrameRenderer, keyed by derived
 * FieldIDs (frame_id * 2 + field_idx) as populated by each observer.
 */
class ObservationCache {
 public:
  /**
   * @brief Construct an observation cache
   * @param dag The DAG to extract observations from
   */
  explicit ObservationCache(std::shared_ptr<const DAG> dag);

  ~ObservationCache() = default;

  /**
   * @brief Ensure observations are populated for the field's parent frame
   *
   * If not cached, renders the frame containing field_id and runs all
   * observers. Returns true on success, false if the frame cannot be rendered.
   *
   * @param node_id The node to query
   * @param field_id The field whose parent frame to render
   * @return True if observations are now available, false on error
   */
  bool get_field(NodeID node_id, FieldID field_id);

  /**
   * @brief Get the total frame count at a node
   *
   * Renders frame 0 if necessary to determine count.
   *
   * @param node_id The node to query
   * @return Frame count, or 0 if node cannot be rendered
   */
  size_t get_frame_count(NodeID node_id);

  /**
   * @brief Update the DAG reference and clear cache
   * @param dag New DAG to use
   */
  void update_dag(std::shared_ptr<const DAG> dag);

  /**
   * @brief Clear all cached observations
   */
  void clear();

  /**
   * @brief Clear cached observations for a specific node
   * @param node_id The node to clear
   */
  void clear_node(NodeID node_id);

  /**
   * @brief Get the observation context from the renderer
   *
   * Returns the observation context populated during the most recent
   * frame rendering by this cache's renderer.
   *
   * @return Const reference to the observation context
   */
  const ObservationContext& get_observation_context() const;

 private:
  // Cache key: (node_id, frame_id)
  struct CacheKey {
    NodeID node_id;
    FrameID frame_id;

    bool operator==(const CacheKey& other) const {
      return node_id == other.node_id && frame_id == other.frame_id;
    }
  };

  struct CacheKeyHash {
    std::size_t operator()(const CacheKey& key) const {
      std::size_t h1 = std::hash<NodeID>{}(key.node_id);
      std::size_t h2 = std::hash<FrameID>{}(key.frame_id);
      return h1 ^ (h2 << 1);
    }
  };

  std::shared_ptr<const DAG> dag_;
  std::shared_ptr<DAGFrameRenderer> renderer_;

  // LRU cache of successfully rendered frame IDs (max 500 entries)
  mutable LRUCache<CacheKey, bool, CacheKeyHash> cache_;

  // LRU cache of frame counts per node (max 100 entries)
  mutable LRUCache<NodeID, size_t> frame_count_cache_;

  // Render a single frame and cache it
  bool render_and_cache(NodeID node_id, FrameID frame_id);
};

}  // namespace orc

#endif  // ORC_CORE_OBSERVATION_CACHE_H
