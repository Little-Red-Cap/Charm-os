module;
#include "vivid_features.generated.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

module charm.core.soa_kernel:payload_lists;

import :kernel_class;
import :types;
import charm.core.handle;
import charm.core.soa_payload;
import charm.core.soa_registry;
import charm.core.style;
import charm.core.style_sheet;
import alg_list_scroll;

    void SoaKernel::set_text_list_count(WidgetHandle h, std::uint16_t count) noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return;
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::TextList) {
            unsupported_kind(common_.kind[idx]);
            return;
        }
        if (count > soa_detail::kMaxTextListItems) {
            count = soa_detail::kMaxTextListItems;
        }
        auto* payload = payload_get<soa_detail::TextListPayload>(idx);
        if (!payload) return;
        payload->count = count;
        payload->start = 0;
        if (payload->selected >= static_cast<int>(count)) {
            payload->selected = (count > 0) ? static_cast<int>(count - 1) : -1;
        }
        mark_layout_dirty();
    }

    void SoaKernel::set_text_list_item(WidgetHandle h, std::uint16_t index, const char* text) noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return;
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::TextList) {
            unsupported_kind(common_.kind[idx]);
            return;
        }
        if (index >= soa_detail::kMaxTextListItems) return;
        auto* payload = payload_get<soa_detail::TextListPayload>(idx);
        if (!payload) return;
        const auto id = payloads_.store_text(text);
        const std::uint16_t slot =
            static_cast<std::uint16_t>((payload->start + index) % soa_detail::kMaxTextListItems);
        payload->items[slot] = id;
        if (index >= payload->count) {
            payload->count = static_cast<std::uint16_t>(index + 1);
        }
        if (payload->selected >= static_cast<int>(payload->count)) {
            payload->selected = payload->count ? static_cast<int>(payload->count - 1) : -1;
        }
        mark_layout_dirty();
    }

    std::uint16_t SoaKernel::text_list_count(WidgetHandle h) const noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return 0;
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::TextList) {
            unsupported_kind(common_.kind[idx]);
            return 0;
        }
        const auto* payload = payload_get<soa_detail::TextListPayload>(idx);
        return payload ? payload->count : 0;
    }

    int SoaKernel::text_list_selected(WidgetHandle h) const noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return -1;
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::TextList) {
            unsupported_kind(common_.kind[idx]);
            return -1;
        }
        const auto* payload = payload_get<soa_detail::TextListPayload>(idx);
        return payload ? payload->selected : -1;
    }

    void SoaKernel::set_text_list_selected(WidgetHandle h, int index) noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return;
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::TextList) {
            unsupported_kind(common_.kind[idx]);
            return;
        }
        auto* payload = payload_get<soa_detail::TextListPayload>(idx);
        if (!payload) return;
        if (payload->count == 0) return;
        if (index < 0) index = 0;
        if (index >= payload->count) index = payload->count - 1;
        if (payload->selected == index) return;
        payload->selected = index;
        const Rect r = rect(h);
        const int max_scroll_value = max_scroll(h);
        payload->scroll_y = alg::list_scroll::ensure_visible(
            index,
            payload->row_height,
            r.h,
            0,
            payload->scroll_y,
            max_scroll_value);
        mark_paint_dirty();
    }

    const char* SoaKernel::text_list_item(WidgetHandle h, std::uint16_t index) const noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return "";
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::TextList) {
            unsupported_kind(common_.kind[idx]);
            return "";
        }
        const auto* payload = payload_get<soa_detail::TextListPayload>(idx);
        if (!payload) return "";
        if (index >= payload->count) return "";
        const std::uint16_t slot =
            static_cast<std::uint16_t>((payload->start + index) % soa_detail::kMaxTextListItems);
        return payloads_.text_c_str(payload->items[slot]);
    }

    void SoaKernel::set_number_list_count(WidgetHandle h, std::uint16_t count) noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return;
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::NumberList) {
            unsupported_kind(common_.kind[idx]);
            return;
        }
        auto* payload = payload_get<soa_detail::NumberListPayload>(idx);
        if (!payload) return;
        payload->count = count;
        if (payload->selected >= static_cast<int>(count)) {
            payload->selected = (count > 0) ? static_cast<int>(count - 1) : 0;
        }
        payload->scroll_y = payload->selected * payload->row_height;
        mark_paint_dirty();
    }

    void SoaKernel::set_number_list_range(WidgetHandle h, int start, int delta) noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return;
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::NumberList) {
            unsupported_kind(common_.kind[idx]);
            return;
        }
        auto* payload = payload_get<soa_detail::NumberListPayload>(idx);
        if (!payload) return;
        payload->start = start;
        payload->delta = (delta != 0) ? delta : 1;
        mark_paint_dirty();
    }

    void SoaKernel::set_number_list_selected(WidgetHandle h, int index) noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return;
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::NumberList) {
            unsupported_kind(common_.kind[idx]);
            return;
        }
        auto* payload = payload_get<soa_detail::NumberListPayload>(idx);
        if (!payload || payload->count == 0) return;
        if (index < 0) index = 0;
        if (index >= payload->count) index = payload->count - 1;
        if (payload->selected == index) return;
        payload->selected = index;
        payload->scroll_y = payload->selected * payload->row_height;
        mark_paint_dirty();
    }

    std::uint16_t SoaKernel::number_list_count(WidgetHandle h) const noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return 0;
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::NumberList) {
            unsupported_kind(common_.kind[idx]);
            return 0;
        }
        const auto* payload = payload_get<soa_detail::NumberListPayload>(idx);
        return payload ? payload->count : 0;
    }

    int SoaKernel::number_list_selected(WidgetHandle h) const noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return 0;
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::NumberList) {
            unsupported_kind(common_.kind[idx]);
            return 0;
        }
        const auto* payload = payload_get<soa_detail::NumberListPayload>(idx);
        return payload ? payload->selected : 0;
    }

    int SoaKernel::number_list_value(WidgetHandle h, int index) const noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return 0;
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::NumberList) {
            unsupported_kind(common_.kind[idx]);
            return 0;
        }
        const auto* payload = payload_get<soa_detail::NumberListPayload>(idx);
        if (!payload) return 0;
        return payload->start + index * payload->delta;
    }

    void SoaKernel::set_number_list_row_height(WidgetHandle h, int row_h) noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return;
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::NumberList) {
            unsupported_kind(common_.kind[idx]);
            return;
        }
        auto* payload = payload_get<soa_detail::NumberListPayload>(idx);
        if (!payload) return;
        payload->row_height = (row_h > 0) ? row_h : 1;
        payload->scroll_y = payload->selected * payload->row_height;
        mark_paint_dirty();
    }

    int SoaKernel::number_list_row_height(WidgetHandle h) const noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return 24;
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::NumberList) {
            unsupported_kind(common_.kind[idx]);
            return 24;
        }
        const auto* payload = payload_get<soa_detail::NumberListPayload>(idx);
        return payload ? payload->row_height : 24;
    }

    void SoaKernel::set_number_list_wheel_step(WidgetHandle h, int step) noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return;
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::NumberList) {
            unsupported_kind(common_.kind[idx]);
            return;
        }
        auto* payload = payload_get<soa_detail::NumberListPayload>(idx);
        if (!payload) return;
        payload->wheel_step = (step > 0) ? step : 1;
    }

    int SoaKernel::number_list_wheel_step(WidgetHandle h) const noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return 24;
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::NumberList) {
            unsupported_kind(common_.kind[idx]);
            return 24;
        }
        const auto* payload = payload_get<soa_detail::NumberListPayload>(idx);
        return payload ? payload->wheel_step : 24;
    }

    void SoaKernel::set_roller_source(WidgetHandle h, std::uint16_t count,
                           const void* ctx, soa_detail::RollerTextFn fn) noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return;
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::Roller) {
            unsupported_kind(common_.kind[idx]);
            return;
        }
        auto* payload = payload_get<soa_detail::RollerPayload>(idx);
        if (!payload) return;
        payload->text_ctx = ctx;
        payload->text_fn = fn;
        payload->count = count;
        if (payload->selected >= static_cast<int>(count)) {
            payload->selected = (count > 0) ? static_cast<int>(count - 1) : 0;
        }
        payload->scroll_y = payload->selected * payload->row_height;
        mark_paint_dirty();
    }

    void SoaKernel::set_roller_selected(WidgetHandle h, int index) noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return;
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::Roller) {
            unsupported_kind(common_.kind[idx]);
            return;
        }
        auto* payload = payload_get<soa_detail::RollerPayload>(idx);
        if (!payload || payload->count == 0) return;
        const int count = payload->count;
        int wrapped = index % count;
        if (wrapped < 0) wrapped += count;
        if (payload->selected == wrapped) return;
        payload->selected = wrapped;
        payload->scroll_y = payload->selected * payload->row_height;
        mark_paint_dirty();
    }

    std::uint16_t SoaKernel::roller_count(WidgetHandle h) const noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return 0;
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::Roller) {
            unsupported_kind(common_.kind[idx]);
            return 0;
        }
        const auto* payload = payload_get<soa_detail::RollerPayload>(idx);
        return payload ? payload->count : 0;
    }

    int SoaKernel::roller_selected(WidgetHandle h) const noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return 0;
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::Roller) {
            unsupported_kind(common_.kind[idx]);
            return 0;
        }
        const auto* payload = payload_get<soa_detail::RollerPayload>(idx);
        return payload ? payload->selected : 0;
    }

    const char* SoaKernel::roller_item_text(WidgetHandle h, std::uint16_t index) const noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return "";
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::Roller) {
            unsupported_kind(common_.kind[idx]);
            return "";
        }
        const auto* payload = payload_get<soa_detail::RollerPayload>(idx);
        if (!payload || !payload->text_fn) return "";
        return payload->text_fn(payload->text_ctx, index);
    }

    void SoaKernel::set_roller_row_height(WidgetHandle h, int row_h) noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return;
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::Roller) {
            unsupported_kind(common_.kind[idx]);
            return;
        }
        auto* payload = payload_get<soa_detail::RollerPayload>(idx);
        if (!payload) return;
        payload->row_height = (row_h > 0) ? row_h : 1;
        payload->scroll_y = payload->selected * payload->row_height;
        mark_paint_dirty();
    }

    int SoaKernel::roller_row_height(WidgetHandle h) const noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return 24;
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::Roller) {
            unsupported_kind(common_.kind[idx]);
            return 24;
        }
        const auto* payload = payload_get<soa_detail::RollerPayload>(idx);
        return payload ? payload->row_height : 24;
    }

    void SoaKernel::set_roller_wheel_step(WidgetHandle h, int step) noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return;
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::Roller) {
            unsupported_kind(common_.kind[idx]);
            return;
        }
        auto* payload = payload_get<soa_detail::RollerPayload>(idx);
        if (!payload) return;
        payload->wheel_step = (step > 0) ? step : 1;
    }

    int SoaKernel::roller_wheel_step(WidgetHandle h) const noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return 24;
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::Roller) {
            unsupported_kind(common_.kind[idx]);
            return 24;
        }
        const auto* payload = payload_get<soa_detail::RollerPayload>(idx);
        return payload ? payload->wheel_step : 24;
    }

    void SoaKernel::console_clear(WidgetHandle h) noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return;
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::TextList) {
            unsupported_kind(common_.kind[idx]);
            return;
        }
        auto* payload = payload_get<soa_detail::TextListPayload>(idx);
        if (!payload) return;
        payload->count = 0;
        payload->start = 0;
        payload->selected = -1;
        payload->scroll_y = 0;
        mark_paint_dirty();
    }

    void SoaKernel::set_console_follow_tail(WidgetHandle h, bool follow) noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return;
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::TextList) {
            unsupported_kind(common_.kind[idx]);
            return;
        }
        auto* payload = payload_get<soa_detail::TextListPayload>(idx);
        if (!payload) return;
        payload->follow_tail = follow ? 1u : 0u;
    }

    void SoaKernel::console_append(WidgetHandle h, const char* text) noexcept {
        if (!text) return;
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return;
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::TextList) {
            unsupported_kind(common_.kind[idx]);
            return;
        }
        auto* payload = payload_get<soa_detail::TextListPayload>(idx);
        if (!payload) return;

        static constexpr std::size_t kMaxLine = 96;
        char line[kMaxLine + 1]{};
        std::size_t len = 0;
        bool added = false;

        auto push_line = [&](const char* text_line) {
            const auto id = payloads_.store_text(text_line);
            const std::uint16_t cap = soa_detail::kMaxTextListItems;
            std::uint16_t slot = 0;
            if (payload->count < cap) {
                slot = static_cast<std::uint16_t>((payload->start + payload->count) % cap);
                payload->count = static_cast<std::uint16_t>(payload->count + 1);
            } else {
                slot = payload->start;
                payload->start = static_cast<std::uint16_t>((payload->start + 1) % cap);
            }
            payload->items[slot] = id;
            added = true;
        };

        for (const char* p = text; *p; ++p) {
            const char ch = *p;
            if (ch == '\r') continue;
            if (ch == '\n') {
                line[len] = '\0';
                push_line(line);
                len = 0;
                continue;
            }
            if (len >= kMaxLine) {
                line[len] = '\0';
                push_line(line);
                len = 0;
            }
            line[len++] = ch;
        }
        if (len > 0) {
            line[len] = '\0';
            push_line(line);
        }

        if (!added) return;
        mark_paint_dirty();
        if (payload->follow_tail != 0) {
            set_scroll_y_clamped(h, max_scroll(h));
        }
    }

    void SoaKernel::set_scrollbar_orientation(WidgetHandle h, ScrollBarOrientation orient) noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return;
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::ScrollBar) {
            unsupported_kind(common_.kind[idx]);
            return;
        }
        auto* payload = payload_get<soa_detail::ScrollBarPayload>(idx);
        if (!payload) return;
        payload->orientation = static_cast<std::uint8_t>(orient);
        mark_paint_dirty();
    }

    ScrollBarOrientation SoaKernel::scrollbar_orientation(WidgetHandle h) const noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return ScrollBarOrientation::Horizontal;
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::ScrollBar) {
            unsupported_kind(common_.kind[idx]);
            return ScrollBarOrientation::Horizontal;
        }
        const auto* payload = payload_get<soa_detail::ScrollBarPayload>(idx);
        if (!payload) return ScrollBarOrientation::Horizontal;
        return static_cast<ScrollBarOrientation>(payload->orientation);
    }

    void SoaKernel::set_scrollbar_page_size(WidgetHandle h, int page_size) noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return;
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::ScrollBar) {
            unsupported_kind(common_.kind[idx]);
            return;
        }
        auto* payload = payload_get<soa_detail::ScrollBarPayload>(idx);
        if (!payload) return;
        payload->page_size = (page_size > 0) ? page_size : 0;
        mark_paint_dirty();
    }

    int SoaKernel::scrollbar_page_size(WidgetHandle h) const noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return 0;
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::ScrollBar) {
            unsupported_kind(common_.kind[idx]);
            return 0;
        }
        const auto* payload = payload_get<soa_detail::ScrollBarPayload>(idx);
        return payload ? payload->page_size : 0;
    }

    void SoaKernel::set_scrollbar_target(WidgetHandle h, WidgetHandle target) noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return;
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::ScrollBar) {
            unsupported_kind(common_.kind[idx]);
            return;
        }
        WidgetHandle next = target;
        if (next && !valid(next)) {
            next = {};
        }
        if (next && !input_is_scrollable_kind(kind(next))) {
            next = {};
        }
        auto* payload = payload_get<soa_detail::ScrollBarPayload>(idx);
        if (!payload) return;
        payload->target = next;
        mark_paint_dirty();
    }

    WidgetHandle SoaKernel::scrollbar_target(WidgetHandle h) const noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return {};
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::ScrollBar) {
            unsupported_kind(common_.kind[idx]);
            return {};
        }
        const auto* payload = payload_get<soa_detail::ScrollBarPayload>(idx);
        const WidgetHandle target = payload ? payload->target : WidgetHandle{};
        if (!target) return {};
        return valid(target) ? target : WidgetHandle{};
    }

    void SoaKernel::set_layout_kind(WidgetHandle h, SoaLayoutKind kind) noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return;
        common_.layout_kind[idx] = static_cast<std::uint8_t>(kind);
        mark_layout_dirty();
    }

    SoaLayoutKind SoaKernel::layout_kind(WidgetHandle h) const noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return SoaLayoutKind::None;
        return static_cast<SoaLayoutKind>(common_.layout_kind[idx]);
    }

    void SoaKernel::set_style_patch(WidgetHandle h, const StylePatch& patch) noexcept {
        set_style_adjust(h, patch);
    }

    void SoaKernel::set_style_adjust(WidgetHandle h, const StylePatch& patch) noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return;
        if (!style_patches_.set(common_.style_patch_slot[idx], patch, StylePatchKind::Adjust)) return;
        mark_layout_dirty();
        mark_paint_dirty();
    }

    void SoaKernel::set_style_override(WidgetHandle h, const StylePatch& patch) noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return;
        if (!style_patches_.set(common_.style_patch_slot[idx], patch, StylePatchKind::Override)) return;
        mark_layout_dirty();
        mark_paint_dirty();
    }

    void SoaKernel::clear_style_patch(WidgetHandle h) noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return;
        if (!style_patches_.clear(common_.style_patch_slot[idx])) return;
        mark_layout_dirty();
        mark_paint_dirty();
    }

    bool SoaKernel::has_style_patch(WidgetHandle h) const noexcept {
        const std::uint16_t idx = index_of(h);
        return idx != kInvalidIndex
            && style_patches_.get(common_.style_patch_slot[idx]) != nullptr;
    }

    const StylePatch* SoaKernel::style_patch(WidgetHandle h) const noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return nullptr;
        return style_patches_.get(common_.style_patch_slot[idx]);
    }

    StylePatchKind SoaKernel::style_patch_kind(WidgetHandle h) const noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return StylePatchKind::None;
        return style_patches_.kind(common_.style_patch_slot[idx]);
    }

    void SoaKernel::set_style_class(WidgetHandle h, StyleClassId id) noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return;
        common_.style_class[idx] = id;
        mark_layout_dirty();
        mark_paint_dirty();
    }

    void SoaKernel::clear_style_class(WidgetHandle h) noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return;
        if (common_.style_class[idx] == kStyleClassInvalid) return;
        common_.style_class[idx] = kStyleClassInvalid;
        mark_layout_dirty();
        mark_paint_dirty();
    }

    StyleClassId SoaKernel::style_class(WidgetHandle h) const noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return kStyleClassInvalid;
        return common_.style_class[idx];
    }

#if CHARM_VIVID_DRAW_DETAIL_EVIDENCE
    void SoaKernel::set_draw_scope(WidgetHandle h, std::uint16_t id) noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return;
        common_.draw_scope[idx] = id;
    }

    std::uint16_t SoaKernel::draw_scope(WidgetHandle h) const noexcept {
        const std::uint16_t idx = index_of(h);
        return (idx == kInvalidIndex) ? std::uint16_t{0} : common_.draw_scope[idx];
    }
#endif
