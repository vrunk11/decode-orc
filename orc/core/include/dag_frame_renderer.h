/*
 * File:        dag_frame_renderer.h
 * Module:      orc-core
 * Purpose:     Frame rendering at DAG nodes using VideoFrameRepresentation
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#pragma once

#include <frame_id.h>
#include <node_id.h>

#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include "dag_executor.h"
#include "lru_cache.h"
#include "observation_context.h"
#include "video_frame_representation.h"

namespace orc {

// Exception thrown during DAG frame rendering.
class DAGFrameRenderError : public std::runtime_error {
 public:
  explicit DAGFrameRenderError(const std::string& msg)
      : std::runtime_error(msg) {}
};

// Result of rendering a specific frame at a DAG node.
struct FrameRenderResult {
  // The VideoFrameRepresentation produced at the specified node.
  // Contains all frames from the source; callers address individual frames by
  // FrameID through the VFR API.
  VideoFrameRepresentationPtr representation;

  // True if the result is valid and can be used.
  bool is_valid = false;

  // Error message when is_valid is false (empty when valid).
  std::string error_message;

  // The node that was rendered.
  NodeID node_id;

  // The frame that was requested.
  FrameID frame_id = 0;

  // True if this result was returned from the LRU cache.
  bool from_cache = false;
};

// DAGFrameRenderer — on-demand frame access at arbitrary DAG nodes.
//
// Executes the DAG up to a specified node and returns the
// VideoFrameRepresentation produced there, keyed by FrameID in an LRU cache.
// Consumers address individual frames through the VFR API.
//
// Replaces DAGFieldRenderer for the CVBS_U10_4FSC pipeline.
//
// Thread safety: not thread-safe. Use from a single thread only.
class DAGFrameRenderer {
 public:
  // Construct a renderer for the given DAG.  Throws DAGFrameRenderError if dag
  // is null or invalid.
  explicit DAGFrameRenderer(std::shared_ptr<const DAG> dag);

  ~DAGFrameRenderer() = default;

  // Non-copyable, non-movable (contains stateful executor and cache).
  DAGFrameRenderer(const DAGFrameRenderer&) = delete;
  DAGFrameRenderer& operator=(const DAGFrameRenderer&) = delete;
  DAGFrameRenderer(DAGFrameRenderer&&) = delete;
  DAGFrameRenderer& operator=(DAGFrameRenderer&&) = delete;

  // -------------------------------------------------------------------------
  // Frame Rendering API
  // -------------------------------------------------------------------------

  // Render the representation at node_id and return it, verifying that
  // frame_id is present.  Results are LRU-cached; calling update_dag()
  // invalidates all cached entries.
  //
  // The representation is NOT executed per-frame: the whole source runs once
  // and the VFR is cached.  Individual frames are accessed by FrameID through
  // the representation's get_frame() / get_line() API.
  FrameRenderResult render_frame_at_node(NodeID node_id, FrameID frame_id);

  // True if node_id exists in the current DAG.
  bool has_node(NodeID node_id) const;

  // Returns node IDs in DAG order (source → sink).
  std::vector<NodeID> get_renderable_nodes() const;

  // Returns the DAG used by this renderer.
  std::shared_ptr<const DAG> get_dag() const { return dag_; }

  // -------------------------------------------------------------------------
  // DAG Change Tracking
  // -------------------------------------------------------------------------

  // Replace the DAG and clear all cached render results.  Increments the
  // internal DAG version so stale FrameRenderResult objects are detectable.
  void update_dag(std::shared_ptr<const DAG> new_dag);

  // Monotonically increasing version number; increments on every update_dag()
  // call.  Callers may store this to detect whether their cached results are
  // still valid.
  uint64_t get_dag_version() const { return dag_version_; }

  // Returns the observation context populated during the most recent
  // render_frame_at_node() execution.
  const ObservationContext& get_observation_context() const;

  // -------------------------------------------------------------------------
  // Cache Management
  // -------------------------------------------------------------------------

  // Discard all cached render results without incrementing the DAG version.
  void clear_cache();

  // Current number of (node_id, frame_id) pairs in the cache.
  size_t cache_size() const { return render_cache_.size(); }

  // Enable or disable result caching.  When disabled every call re-executes
  // the DAG.
  void set_cache_enabled(bool enabled) { cache_enabled_ = enabled; }
  bool is_cache_enabled() const { return cache_enabled_; }

 private:
  std::shared_ptr<const DAG> dag_;
  uint64_t dag_version_;
  bool cache_enabled_;

  // Cache key: (node_id, frame_id value, dag_version)
  struct CacheKey {
    NodeID node_id;
    uint64_t frame_id_value;
    uint64_t dag_version;

    bool operator==(const CacheKey& o) const noexcept {
      return dag_version == o.dag_version && node_id == o.node_id &&
             frame_id_value == o.frame_id_value;
    }
  };

  struct CacheKeyHash {
    std::size_t operator()(const CacheKey& k) const noexcept {
      std::size_t h1 = std::hash<NodeID>{}(k.node_id);
      std::size_t h2 = std::hash<uint64_t>{}(k.frame_id_value);
      std::size_t h3 = std::hash<uint64_t>{}(k.dag_version);
      return h1 ^ (h2 << 1) ^ (h3 << 2);
    }
  };

  static constexpr size_t kMaxCachedRenders = 500;
  LRUCache<CacheKey, FrameRenderResult, CacheKeyHash> render_cache_;

  std::unique_ptr<DAGExecutor> executor_;

  mutable std::map<NodeID, size_t> node_index_;
  mutable bool node_index_valid_;

  void ensure_node_index() const;

  FrameRenderResult execute_to_node(NodeID node_id, FrameID frame_id);
};

}  // namespace orc
