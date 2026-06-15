/*
 * File:        hints_presenter.cpp
 * Module:      orc-presenters
 * Purpose:     Hints presenter implementation
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: 2026 Simon Inns
 */

#include "hints_presenter.h"

#include <algorithm>
#include <stdexcept>
#include <utility>

#include "../core/include/dag_frame_renderer.h"
#include "hints_view_models.h"

namespace orc::presenters {

class HintsPresenter::Impl {
 public:
  explicit Impl(std::function<std::shared_ptr<void>()> dag_provider)
      : dag_provider_(std::move(dag_provider)), next_hint_id_(1) {}

  std::function<std::shared_ptr<void>()> dag_provider_;
  int next_hint_id_;

  // In-memory hint store keyed by ID and node
  std::unordered_map<int, Hint> hints_by_id_;
  std::unordered_map<NodeID, std::vector<int>> node_to_hint_ids_;
};

HintsPresenter::HintsPresenter(
    std::function<std::shared_ptr<void>()> dag_provider)
    : impl_(std::make_unique<Impl>(std::move(dag_provider))) {}

HintsPresenter::~HintsPresenter() = default;

HintsPresenter::HintsPresenter(HintsPresenter&&) noexcept = default;
HintsPresenter& HintsPresenter::operator=(HintsPresenter&&) noexcept = default;

int HintsPresenter::addActiveLineHint(NodeID node_id,
                                      const ActiveLineHint& hint) {
  Hint h{};
  h.id = impl_->next_hint_id_++;
  h.type = HintType::ActiveLine;
  h.description = "Active line hint";
  h.active_line = hint;
  impl_->hints_by_id_.emplace(h.id, h);
  impl_->node_to_hint_ids_[node_id].push_back(h.id);
  return h.id;
}

int HintsPresenter::addVBIHint(NodeID node_id, const VBIHint& hint) {
  Hint h{};
  h.id = impl_->next_hint_id_++;
  h.type = HintType::VBI;
  h.description = "VBI hint";
  h.vbi = hint;
  impl_->hints_by_id_.emplace(h.id, h);
  impl_->node_to_hint_ids_[node_id].push_back(h.id);
  return h.id;
}

int HintsPresenter::addDropoutHint(NodeID node_id, const DropoutHint& hint) {
  Hint h{};
  h.id = impl_->next_hint_id_++;
  h.type = HintType::Dropout;
  h.description = "Dropout hint";
  h.dropout = hint;
  impl_->hints_by_id_.emplace(h.id, h);
  impl_->node_to_hint_ids_[node_id].push_back(h.id);
  return h.id;
}

int HintsPresenter::addBurstHint(NodeID node_id, const BurstHint& hint) {
  Hint h{};
  h.id = impl_->next_hint_id_++;
  h.type = HintType::Burst;
  h.description = "Burst hint";
  h.burst = hint;
  impl_->hints_by_id_.emplace(h.id, h);
  impl_->node_to_hint_ids_[node_id].push_back(h.id);
  return h.id;
}

bool HintsPresenter::updateHint(int hint_id, const Hint& hint) {
  auto it = impl_->hints_by_id_.find(hint_id);
  if (it == impl_->hints_by_id_.end()) return false;
  if (hint.id != hint_id) return false;
  it->second = hint;
  return true;
}

bool HintsPresenter::removeHint(int hint_id) {
  auto it = impl_->hints_by_id_.find(hint_id);
  if (it == impl_->hints_by_id_.end()) return false;

  // Remove from node map
  for (auto& entry : impl_->node_to_hint_ids_) {
    auto& vec = entry.second;
    vec.erase(std::remove(vec.begin(), vec.end(), hint_id), vec.end());
  }

  impl_->hints_by_id_.erase(it);
  return true;
}

void HintsPresenter::setHintEnabled(int hint_id, bool enabled) {
  auto it = impl_->hints_by_id_.find(hint_id);
  if (it != impl_->hints_by_id_.end()) {
    it->second.enabled = enabled;
  }
}

std::vector<Hint> HintsPresenter::getHints(NodeID node_id) const {
  std::vector<Hint> out;
  auto it = impl_->node_to_hint_ids_.find(node_id);
  if (it == impl_->node_to_hint_ids_.end()) return out;
  out.reserve(it->second.size());
  for (auto id : it->second) {
    auto hit = impl_->hints_by_id_.find(id);
    if (hit != impl_->hints_by_id_.end()) {
      out.push_back(hit->second);
    }
  }
  return out;
}

std::vector<Hint> HintsPresenter::getHintsByType(NodeID node_id,
                                                 HintType type) const {
  std::vector<Hint> out;
  auto it = impl_->node_to_hint_ids_.find(node_id);
  if (it == impl_->node_to_hint_ids_.end()) return out;
  for (auto id : it->second) {
    auto hit = impl_->hints_by_id_.find(id);
    if (hit != impl_->hints_by_id_.end() && hit->second.type == type) {
      out.push_back(hit->second);
    }
  }
  return out;
}

Hint HintsPresenter::getHint(int hint_id) const {
  auto it = impl_->hints_by_id_.find(hint_id);
  if (it == impl_->hints_by_id_.end()) {
    throw std::out_of_range("Hint not found");
  }
  return it->second;
}

bool HintsPresenter::hasHints(NodeID node_id) const {
  auto it = impl_->node_to_hint_ids_.find(node_id);
  return it != impl_->node_to_hint_ids_.end() && !it->second.empty();
}

std::vector<DropoutHint> HintsPresenter::getDropoutHintsForField(
    NodeID node_id, FieldID field_id) const {
  std::vector<DropoutHint> out;
  auto it = impl_->node_to_hint_ids_.find(node_id);
  if (it == impl_->node_to_hint_ids_.end()) return out;
  for (auto id : it->second) {
    auto hit = impl_->hints_by_id_.find(id);
    if (hit != impl_->hints_by_id_.end() &&
        hit->second.type == HintType::Dropout) {
      if (hit->second.dropout.field_id == field_id) {
        out.push_back(hit->second.dropout);
      }
    }
  }
  return out;
}

bool HintsPresenter::validateHint(const Hint& hint,
                                  std::string* error_message) const {
  auto fail = [&](const std::string& msg) {
    if (error_message) *error_message = msg;
    return false;
  };

  switch (hint.type) {
    case HintType::ActiveLine:
      if (hint.active_line.first_line < 0 || hint.active_line.last_line < 0) {
        return fail("Active line range must be non-negative");
      }
      if (hint.active_line.last_line < hint.active_line.first_line) {
        return fail("Active line range is invalid");
      }
      return true;
    case HintType::VBI:
      if (hint.vbi.lines.empty()) return fail("VBI lines cannot be empty");
      return true;
    case HintType::Dropout:
      if (hint.dropout.line_start < 0 ||
          hint.dropout.line_end < hint.dropout.line_start) {
        return fail("Dropout line range is invalid");
      }
      if (hint.dropout.pixel_start < 0 ||
          hint.dropout.pixel_end < hint.dropout.pixel_start) {
        return fail("Dropout pixel range is invalid");
      }
      return true;
    case HintType::Burst:
      if (hint.burst.burst_start < 0 || hint.burst.burst_length <= 0) {
        return fail("Burst parameters are invalid");
      }
      return true;
    case HintType::Custom:
      return true;
    default:
      return fail("Unknown hint type");
  }
}

void HintsPresenter::clearHints(NodeID node_id) {
  auto it = impl_->node_to_hint_ids_.find(node_id);
  if (it == impl_->node_to_hint_ids_.end()) return;
  for (auto id : it->second) {
    impl_->hints_by_id_.erase(id);
  }
  impl_->node_to_hint_ids_.erase(it);
}

bool HintsPresenter::importHints(NodeID node_id, const std::string& file_path) {
  return false;
}

bool HintsPresenter::exportHints(NodeID node_id,
                                 const std::string& file_path) const {
  return false;
}

HintsPresenter::FieldHintsView HintsPresenter::getHintsForField(
    NodeID node_id, FieldID field_id) const {
  FieldHintsView out{};

  auto dag_void = impl_->dag_provider_ ? impl_->dag_provider_() : nullptr;
  if (!dag_void) {
    return out;
  }
  auto dag = std::static_pointer_cast<const orc::DAG>(dag_void);

  try {
    orc::FrameID frame_id = static_cast<orc::FrameID>(field_id.value() / 2);
    bool is_first_field = (field_id.value() % 2 == 0);

    DAGFrameRenderer renderer(dag);
    auto result = renderer.render_frame_at_node(node_id, frame_id);
    if (!result.is_valid || !result.representation) {
      return out;
    }

    // Field parity: derived deterministically from FieldID (no VFR hint needed)
    {
      FieldParityHintView v{};
      v.is_first_field = is_first_field;
      v.source = HintSourceView::METADATA;
      v.confidence_pct = 100;
      out.parity = v;
    }

    // Field phase: VFR exposes colour frame index at frame granularity
    if (auto colour_idx =
            result.representation->get_frame_phase_hint(frame_id)) {
      FieldPhaseHintView v{};
      v.field_phase_id = *colour_idx;
      v.source = HintSourceView::METADATA;
      v.confidence_pct = 100;
      out.phase = v;
    }

    if (auto active = result.representation->get_active_line_hint()) {
      ActiveLineHintView v{};
      v.first_active_frame_line = active->first_active_frame_line;
      v.last_active_frame_line = active->last_active_frame_line;
      v.source = HintSourceView::METADATA;
      v.confidence_pct = 100;
      if (v.is_valid()) {
        out.active_line = v;
      }
    }

    if (auto params = result.representation->get_video_parameters()) {
      out.video_params = toVideoParametersView(*params);
    }
  } catch (const std::exception&) {  // NOLINT(bugprone-empty-catch)
    // Swallow and return empty hints; GUI will clear
  }

  return out;
}

}  // namespace orc::presenters
