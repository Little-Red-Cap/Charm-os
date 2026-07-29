module;

#include <cstddef>
#include <cstdint>

module charm.core.soa_kernel:storage;

import :kernel_class;
import :types;
import :input;
import charm.core.style;
import charm.core.soa_payload;
import charm.core.soa_registry;

    SoaKernel::SoaKernel() noexcept {
        free_head_ = 0;
        for (std::uint16_t i = 0; i < kMaxNodes; ++i) {
            common_.storage_slot[i].set_free_next(
                (i + 1 < kMaxNodes) ? static_cast<std::uint16_t>(i + 1) : kInvalidIndex);
            common_.kind[i] = WidgetKind::None;
            common_.generation[i] = 1;
            common_.runtime_state[i].reset();
            common_.rects[i] = Rect{};
            common_.parent[i] = kInvalidIndex;
            common_.first_child[i] = kInvalidIndex;
            common_.last_child[i] = kInvalidIndex;
            common_.next_sibling[i] = kInvalidIndex;
            common_.prev_sibling[i] = kInvalidIndex;
            common_.child_count[i] = 0;
            common_.layout_text[i].reset();
            common_.style_patch_slot[i].reset();
            common_.style_class[i] = kStyleClassInvalid;
            common_.semantic_slot[i].reset();
#if CHARM_VIVID_DRAW_DETAIL_EVIDENCE
            common_.draw_scope[i] = 0;
#endif
        }
        semantics_.reset();
        style_patches_.reset();
        payloads_.reset();
    }

    WidgetHandle SoaKernel::create(WidgetKind kind) noexcept {
        if (!widget_kind_enabled(kind)) {
            unsupported_kind(kind);
            return {};
        }
        const auto desc = payload_descriptor(kind);
        if (!desc.supported) {
            unsupported_kind(kind);
            return {};
        }
        if (free_head_ == kInvalidIndex) return {};
        const std::uint16_t idx = free_head_;
        free_head_ = common_.storage_slot[idx].free_next();
        const SoaDefaults defaults = default_for_kind(kind);
        common_.kind[idx] = kind;
        common_.runtime_state[idx].reset();
        common_.runtime_state[idx].set(SoaNodeFlag::Visible, true);
        common_.runtime_state[idx].set(SoaNodeFlag::Enabled, true);
        common_.runtime_state[idx].set(SoaNodeFlag::HitTest, defaults.hit_test);
        common_.runtime_state[idx].set(SoaNodeFlag::Focusable, defaults.focusable);
        common_.runtime_state[idx].set(SoaNodeFlag::ClipChildren, defaults.clip_children);
        common_.rects[idx] = Rect{};
        common_.parent[idx] = kInvalidIndex;
        common_.first_child[idx] = kInvalidIndex;
        common_.last_child[idx] = kInvalidIndex;
        common_.next_sibling[idx] = kInvalidIndex;
        common_.prev_sibling[idx] = kInvalidIndex;
        common_.child_count[idx] = 0;
        common_.layout_text[idx].reset(defaults.layout_kind);
        common_.style_patch_slot[idx].reset();
        common_.style_class[idx] = kStyleClassInvalid;
        common_.semantic_slot[idx].reset();
#if CHARM_VIVID_DRAW_DETAIL_EVIDENCE
        common_.draw_scope[idx] = 0;
#endif
        const auto payload = payload_alloc(kind, idx);
        if (desc.payload != soa_detail::PayloadKind::None && !soa_detail::payload_slot_valid(payload)) {
            common_.kind[idx] = WidgetKind::None;
            common_.runtime_state[idx].reset();
            common_.rects[idx] = Rect{};
            common_.parent[idx] = kInvalidIndex;
            common_.first_child[idx] = kInvalidIndex;
            common_.last_child[idx] = kInvalidIndex;
            common_.next_sibling[idx] = kInvalidIndex;
            common_.prev_sibling[idx] = kInvalidIndex;
            common_.child_count[idx] = 0;
            common_.layout_text[idx].reset();
            common_.style_patch_slot[idx].reset();
            common_.style_class[idx] = kStyleClassInvalid;
            common_.semantic_slot[idx].reset();
#if CHARM_VIVID_DRAW_DETAIL_EVIDENCE
            common_.draw_scope[idx] = 0;
#endif
            common_.storage_slot[idx].set_free_next(free_head_);
            free_head_ = idx;
            return {};
        }
        common_.storage_slot[idx].set_payload_slot(payload);
        mark_layout_dirty();
        return WidgetHandle{kind, idx, common_.generation[idx]};
    }

    void SoaKernel::destroy(WidgetHandle h) noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return;
        input_on_destroy(h);
        clear_scrollbar_targets(h);
        const WidgetKind old_kind = common_.kind[idx];
        detach_from_parent(idx);
        detach_children(idx);
        common_.kind[idx] = WidgetKind::None;
        common_.runtime_state[idx].reset();
        common_.rects[idx] = Rect{};
        common_.layout_text[idx].reset();
        (void)style_patches_.clear(common_.style_patch_slot[idx]);
        common_.style_class[idx] = kStyleClassInvalid;
        (void)semantics_.clear(common_.semantic_slot[idx]);
#if CHARM_VIVID_DRAW_DETAIL_EVIDENCE
        common_.draw_scope[idx] = 0;
#endif
        payload_free(old_kind, common_.storage_slot[idx].payload_slot(), idx);
        mark_layout_dirty();
        common_.generation[idx] = static_cast<std::uint16_t>(common_.generation[idx] + 1);
        common_.storage_slot[idx].set_free_next(free_head_);
        free_head_ = idx;
    }

    bool SoaKernel::valid(WidgetHandle h) const noexcept {
        return index_of(h) != kInvalidIndex;
    }

    WidgetKind SoaKernel::kind(WidgetHandle h) const noexcept {
        const std::uint16_t idx = index_of(h);
        return (idx == kInvalidIndex) ? WidgetKind::None : common_.kind[idx];
    }

    bool SoaKernel::link(WidgetHandle parent, WidgetHandle child) noexcept {
        const std::uint16_t p = index_of(parent);
        const std::uint16_t c = index_of(child);
        if (p == kInvalidIndex || c == kInvalidIndex) return false;
        if (p == c) return false;
        if (creates_cycle(p, c)) return false;
        detach_from_parent(c);
        common_.parent[c] = p;
        if (common_.last_child[p] != kInvalidIndex) {
            const std::uint16_t last = common_.last_child[p];
            common_.next_sibling[last] = c;
            common_.prev_sibling[c] = last;
            common_.last_child[p] = c;
        } else {
            common_.first_child[p] = c;
            common_.last_child[p] = c;
            common_.prev_sibling[c] = kInvalidIndex;
        }
        common_.next_sibling[c] = kInvalidIndex;
        common_.child_count[p] = static_cast<std::uint16_t>(common_.child_count[p] + 1);
        mark_layout_dirty();
        return true;
    }

    bool SoaKernel::unlink(WidgetHandle parent, WidgetHandle child) noexcept {
        const std::uint16_t p = index_of(parent);
        const std::uint16_t c = index_of(child);
        if (p == kInvalidIndex || c == kInvalidIndex) return false;
        if (common_.parent[c] != p) return false;
        detach_from_parent(c);
        mark_layout_dirty();
        return true;
    }

    WidgetHandle SoaKernel::parent(WidgetHandle h) const noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return {};
        return handle_from_index(common_.parent[idx]);
    }

    WidgetHandle SoaKernel::first_child(WidgetHandle h) const noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return {};
        return handle_from_index(common_.first_child[idx]);
    }

    WidgetHandle SoaKernel::last_child(WidgetHandle h) const noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return {};
        return handle_from_index(common_.last_child[idx]);
    }

    WidgetHandle SoaKernel::next_sibling(WidgetHandle h) const noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return {};
        return handle_from_index(common_.next_sibling[idx]);
    }

    WidgetHandle SoaKernel::prev_sibling(WidgetHandle h) const noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return {};
        return handle_from_index(common_.prev_sibling[idx]);
    }

    std::size_t SoaKernel::child_count(WidgetHandle h) const noexcept {
        const std::uint16_t idx = index_of(h);
        return (idx == kInvalidIndex) ? std::size_t{0} : static_cast<std::size_t>(common_.child_count[idx]);
    }

    std::uint16_t SoaKernel::index_of(WidgetHandle h) const noexcept {
        const std::uint16_t idx = h.index;
        if (idx >= kMaxNodes) return kInvalidIndex;
        const WidgetKind kind = common_.kind[idx];
        if (kind == WidgetKind::None || kind != h.kind) return kInvalidIndex;
        if (common_.generation[idx] != h.generation) return kInvalidIndex;
        return idx;
    }

    WidgetHandle SoaKernel::handle_from_index(std::uint16_t idx) const noexcept {
        if (idx == kInvalidIndex || idx >= kMaxNodes) return {};
        if (common_.kind[idx] == WidgetKind::None) return {};
        return WidgetHandle{common_.kind[idx], idx, common_.generation[idx]};
    }

    void SoaKernel::detach_from_parent(std::uint16_t idx) noexcept {
        const std::uint16_t p = common_.parent[idx];
        if (p == kInvalidIndex) return;
        const std::uint16_t prev = common_.prev_sibling[idx];
        const std::uint16_t next = common_.next_sibling[idx];
        if (prev != kInvalidIndex) {
            common_.next_sibling[prev] = next;
        } else {
            common_.first_child[p] = next;
        }
        if (next != kInvalidIndex) {
            common_.prev_sibling[next] = prev;
        } else {
            common_.last_child[p] = prev;
        }
        common_.parent[idx] = kInvalidIndex;
        common_.prev_sibling[idx] = kInvalidIndex;
        common_.next_sibling[idx] = kInvalidIndex;
        if (common_.child_count[p] > 0) {
            common_.child_count[p] = static_cast<std::uint16_t>(common_.child_count[p] - 1);
        }
    }

    void SoaKernel::detach_children(std::uint16_t idx) noexcept {
        std::uint16_t child = common_.first_child[idx];
        while (child != kInvalidIndex) {
            common_.parent[child] = kInvalidIndex;
            const std::uint16_t next = common_.next_sibling[child];
            common_.prev_sibling[child] = kInvalidIndex;
            common_.next_sibling[child] = kInvalidIndex;
            child = next;
        }
        common_.first_child[idx] = kInvalidIndex;
        common_.last_child[idx] = kInvalidIndex;
        common_.child_count[idx] = 0;
    }

    bool SoaKernel::creates_cycle(std::uint16_t parent, std::uint16_t child) const noexcept {
        std::uint16_t p = parent;
        while (p != kInvalidIndex) {
            if (p == child) return true;
            p = common_.parent[p];
        }
        return false;
    }

    bool SoaKernel::flag_raw(std::uint16_t idx, SoaNodeFlag flag) const noexcept {
        return common_.runtime_state[idx].get(flag);
    }

    bool SoaKernel::get_flag(WidgetHandle h, SoaNodeFlag flag) const noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return false;
        return flag_raw(idx, flag);
    }

    void SoaKernel::set_flag(WidgetHandle h, SoaNodeFlag flag, bool on) noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return;
        common_.runtime_state[idx].set(flag, on);
    }

    bool SoaKernel::get_state_flag(WidgetHandle h, SoaStateFlag flag) const noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return false;
        return common_.runtime_state[idx].get(flag);
    }

    void SoaKernel::set_state_flag(WidgetHandle h, SoaStateFlag flag, bool on) noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return;
        common_.runtime_state[idx].set(flag, on);
    }
