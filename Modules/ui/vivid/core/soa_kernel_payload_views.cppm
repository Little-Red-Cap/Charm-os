module;
#include <cstddef>
#include <cstdint>
#include <cstring>

export module charm.core.soa_kernel:payload_views;

import :kernel_class;
import :types;
import charm.core.handle;
import charm.core.soa_payload;
import charm.core.soa_registry;
import charm.core.style;
import charm.core.style_sheet;
import alg_list_scroll;

    void SoaKernel::set_list_view_source(WidgetHandle h, std::uint16_t count, const void* ctx,
                              soa_detail::ListViewTextFn fn) noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return;
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::ListView) {
            unsupported_kind(common_.kind[idx]);
            return;
        }
        auto* payload = payload_get<soa_detail::ListViewPayload>(idx);
        if (!payload) return;
        payload->text_ctx = ctx;
        payload->text_fn = fn;
        payload->count = count;
        if (payload->selected >= static_cast<int>(count)) {
            payload->selected = (count > 0) ? static_cast<int>(count - 1) : -1;
        }
        if (payload->active >= static_cast<int>(count)) {
            payload->active = -1;
        }
        if (payload->pending_tail_action >= static_cast<int>(count)) {
            payload->pending_tail_action = -1;
        }
        payload->scroll_y = clamp_scroll_y(h, payload->scroll_y);
        mark_layout_dirty();
    }

    void SoaKernel::set_list_view_subtitle_source(WidgetHandle h,
                                                  const void* ctx,
                                                  soa_detail::ListViewSubtitleFn fn) noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return;
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::ListView) {
            unsupported_kind(common_.kind[idx]);
            return;
        }
        auto* payload = payload_get<soa_detail::ListViewPayload>(idx);
        if (!payload) return;
        payload->subtitle_ctx = ctx;
        payload->subtitle_fn = fn;
        mark_paint_dirty();
    }

    void SoaKernel::set_list_view_tail_source(WidgetHandle h,
                                              const void* ctx,
                                              soa_detail::ListViewTailFn fn) noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return;
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::ListView) {
            unsupported_kind(common_.kind[idx]);
            return;
        }
        auto* payload = payload_get<soa_detail::ListViewPayload>(idx);
        if (!payload) return;
        payload->tail_ctx = ctx;
        payload->tail_fn = fn;
        mark_paint_dirty();
    }

    void SoaKernel::set_list_view_row_flags_source(WidgetHandle h,
                                                   const void* ctx,
                                                   soa_detail::ListViewRowFlagsFn fn) noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return;
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::ListView) {
            unsupported_kind(common_.kind[idx]);
            return;
        }
        auto* payload = payload_get<soa_detail::ListViewPayload>(idx);
        if (!payload) return;
        payload->row_flags_ctx = ctx;
        payload->row_flags_fn = fn;
        mark_paint_dirty();
    }

    void SoaKernel::set_list_view_tail_icon_source(WidgetHandle h,
                                                   const void* ctx,
                                                   soa_detail::ListViewIconFn fn,
                                                   std::uint8_t size) noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return;
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::ListView) {
            unsupported_kind(common_.kind[idx]);
            return;
        }
        auto* payload = payload_get<soa_detail::ListViewPayload>(idx);
        if (!payload) return;
        payload->tail_icon_ctx = ctx;
        payload->tail_icon_fn = fn;
        payload->tail_icon_size = size;
        mark_paint_dirty();
    }

    void SoaKernel::set_list_view_tail_action_icon_source(WidgetHandle h,
                                                          const void* ctx,
                                                          soa_detail::ListViewIconFn fn,
                                                          std::uint8_t size) noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return;
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::ListView) {
            unsupported_kind(common_.kind[idx]);
            return;
        }
        auto* payload = payload_get<soa_detail::ListViewPayload>(idx);
        if (!payload) return;
        payload->tail_action_icon_ctx = ctx;
        payload->tail_action_icon_fn = fn;
        payload->tail_action_icon_size = size;
        mark_paint_dirty();
    }

    void SoaKernel::set_list_view_icon_source(WidgetHandle h,
                                              const void* ctx,
                                              soa_detail::ListViewIconFn fn,
                                              std::uint8_t icon_size) noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return;
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::ListView) {
            unsupported_kind(common_.kind[idx]);
            return;
        }
        auto* payload = payload_get<soa_detail::ListViewPayload>(idx);
        if (!payload) return;
        payload->icon_ctx = ctx;
        payload->icon_fn = fn;
        payload->icon_size = icon_size;
        mark_paint_dirty();
    }

    void SoaKernel::set_list_view_icon_corner_radius(WidgetHandle h, std::uint8_t radius) noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return;
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::ListView) {
            unsupported_kind(common_.kind[idx]);
            return;
        }
        auto* payload = payload_get<soa_detail::ListViewPayload>(idx);
        if (!payload) return;
        if (payload->icon_corner_radius != radius) {
            payload->icon_corner_radius = radius;
            mark_paint_dirty();
        }
    }

    void SoaKernel::set_list_view_count(WidgetHandle h, std::uint16_t count) noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return;
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::ListView) {
            unsupported_kind(common_.kind[idx]);
            return;
        }
        auto* payload = payload_get<soa_detail::ListViewPayload>(idx);
        if (!payload) return;
        payload->count = count;
        if (payload->selected >= static_cast<int>(count)) {
            payload->selected = (count > 0) ? static_cast<int>(count - 1) : -1;
        }
        if (payload->active >= static_cast<int>(count)) {
            payload->active = -1;
        }
        if (payload->pending_tail_action >= static_cast<int>(count)) {
            payload->pending_tail_action = -1;
        }
        payload->scroll_y = clamp_scroll_y(h, payload->scroll_y);
        mark_layout_dirty();
    }

    std::uint16_t SoaKernel::list_view_count(WidgetHandle h) const noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return 0;
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::ListView) {
            unsupported_kind(common_.kind[idx]);
            return 0;
        }
        const auto* payload = payload_get<soa_detail::ListViewPayload>(idx);
        return payload ? payload->count : 0;
    }

    int SoaKernel::list_view_selected(WidgetHandle h) const noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return -1;
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::ListView) {
            unsupported_kind(common_.kind[idx]);
            return -1;
        }
        const auto* payload = payload_get<soa_detail::ListViewPayload>(idx);
        return payload ? payload->selected : -1;
    }

    int SoaKernel::list_view_active(WidgetHandle h) const noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return -1;
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::ListView) {
            unsupported_kind(common_.kind[idx]);
            return -1;
        }
        const auto* payload = payload_get<soa_detail::ListViewPayload>(idx);
        return payload ? payload->active : -1;
    }

    void SoaKernel::set_list_view_selected(WidgetHandle h, int index) noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return;
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::ListView) {
            unsupported_kind(common_.kind[idx]);
            return;
        }
        auto* payload = payload_get<soa_detail::ListViewPayload>(idx);
        if (!payload) return;
        if (payload->count == 0 || index < 0) {
            if (payload->selected == -1) return;
            payload->selected = -1;
            mark_paint_dirty();
            return;
        }
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

    void SoaKernel::set_list_view_active(WidgetHandle h, int index) noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return;
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::ListView) {
            unsupported_kind(common_.kind[idx]);
            return;
        }
        auto* payload = payload_get<soa_detail::ListViewPayload>(idx);
        if (!payload) return;
        if (index < -1 || index >= payload->count) index = -1;
        if (payload->active == index) return;
        payload->active = index;
        mark_paint_dirty();
    }

    const char* SoaKernel::list_view_item_text(WidgetHandle h, std::uint16_t index) const noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return "";
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::ListView) {
            unsupported_kind(common_.kind[idx]);
            return "";
        }
        const auto* payload = payload_get<soa_detail::ListViewPayload>(idx);
        if (!payload) return "";
        if (index >= payload->count) return "";
        if (!payload->text_fn) return "";
        const char* text = payload->text_fn(payload->text_ctx, index);
        return text ? text : "";
    }

    const char* SoaKernel::list_view_item_subtitle(WidgetHandle h, std::uint16_t index) const noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return "";
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::ListView) {
            unsupported_kind(common_.kind[idx]);
            return "";
        }
        const auto* payload = payload_get<soa_detail::ListViewPayload>(idx);
        if (!payload) return "";
        if (index >= payload->count) return "";
        if (!payload->subtitle_fn) return "";
        const char* text = payload->subtitle_fn(payload->subtitle_ctx, index);
        return text ? text : "";
    }

    const char* SoaKernel::list_view_item_tail(WidgetHandle h, std::uint16_t index) const noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return "";
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::ListView) {
            unsupported_kind(common_.kind[idx]);
            return "";
        }
        const auto* payload = payload_get<soa_detail::ListViewPayload>(idx);
        if (!payload) return "";
        if (index >= payload->count) return "";
        if (!payload->tail_fn) return "";
        const char* text = payload->tail_fn(payload->tail_ctx, index);
        return text ? text : "";
    }

    std::uint8_t SoaKernel::list_view_item_row_flags(WidgetHandle h, std::uint16_t index) const noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return 0;
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::ListView) {
            unsupported_kind(common_.kind[idx]);
            return 0;
        }
        const auto* payload = payload_get<soa_detail::ListViewPayload>(idx);
        if (!payload) return 0;
        if (index >= payload->count) return 0;
        if (!payload->row_flags_fn) return 0;
        return payload->row_flags_fn(payload->row_flags_ctx, index);
    }

    soa_detail::ImageId SoaKernel::list_view_item_tail_icon(WidgetHandle h, std::uint16_t index) const noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return soa_detail::invalid_image_id();
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::ListView) {
            unsupported_kind(common_.kind[idx]);
            return soa_detail::invalid_image_id();
        }
        const auto* payload = payload_get<soa_detail::ListViewPayload>(idx);
        if (!payload) return soa_detail::invalid_image_id();
        if (index >= payload->count) return soa_detail::invalid_image_id();
        if (!payload->tail_icon_fn) return soa_detail::invalid_image_id();
        return payload->tail_icon_fn(payload->tail_icon_ctx, index);
    }

    soa_detail::ImageId SoaKernel::list_view_item_tail_action_icon(WidgetHandle h, std::uint16_t index) const noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return soa_detail::invalid_image_id();
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::ListView) {
            unsupported_kind(common_.kind[idx]);
            return soa_detail::invalid_image_id();
        }
        const auto* payload = payload_get<soa_detail::ListViewPayload>(idx);
        if (!payload) return soa_detail::invalid_image_id();
        if (index >= payload->count) return soa_detail::invalid_image_id();
        if (!payload->tail_action_icon_fn) return soa_detail::invalid_image_id();
        return payload->tail_action_icon_fn(payload->tail_action_icon_ctx, index);
    }

    soa_detail::ImageId SoaKernel::list_view_item_icon(WidgetHandle h, std::uint16_t index) const noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return soa_detail::invalid_image_id();
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::ListView) {
            unsupported_kind(common_.kind[idx]);
            return soa_detail::invalid_image_id();
        }
        const auto* payload = payload_get<soa_detail::ListViewPayload>(idx);
        if (!payload) return soa_detail::invalid_image_id();
        if (index >= payload->count) return soa_detail::invalid_image_id();
        if (!payload->icon_fn) return soa_detail::invalid_image_id();
        return payload->icon_fn(payload->icon_ctx, index);
    }

    std::uint8_t SoaKernel::list_view_tail_icon_size(WidgetHandle h) const noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return 0;
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::ListView) {
            unsupported_kind(common_.kind[idx]);
            return 0;
        }
        const auto* payload = payload_get<soa_detail::ListViewPayload>(idx);
        return payload ? payload->tail_icon_size : 0;
    }

    std::uint8_t SoaKernel::list_view_tail_action_icon_size(WidgetHandle h) const noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return 0;
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::ListView) {
            unsupported_kind(common_.kind[idx]);
            return 0;
        }
        const auto* payload = payload_get<soa_detail::ListViewPayload>(idx);
        return payload ? payload->tail_action_icon_size : 0;
    }

    int SoaKernel::consume_list_view_tail_action(WidgetHandle h) noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return -1;
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::ListView) {
            unsupported_kind(common_.kind[idx]);
            return -1;
        }
        auto* payload = payload_get<soa_detail::ListViewPayload>(idx);
        if (!payload) return -1;
        const int pending = payload->pending_tail_action;
        payload->pending_tail_action = -1;
        return pending;
    }

    std::uint8_t SoaKernel::list_view_icon_corner_radius(WidgetHandle h) const noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return 0;
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::ListView) {
            unsupported_kind(common_.kind[idx]);
            return 0;
        }
        const auto* payload = payload_get<soa_detail::ListViewPayload>(idx);
        return payload ? payload->icon_corner_radius : 0;
    }

    std::uint8_t SoaKernel::list_view_icon_size(WidgetHandle h) const noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return 0;
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::ListView) {
            unsupported_kind(common_.kind[idx]);
            return 0;
        }
        const auto* payload = payload_get<soa_detail::ListViewPayload>(idx);
        return payload ? payload->icon_size : 0;
    }

    std::uint8_t SoaKernel::list_view_overscan(WidgetHandle h) const noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return 0;
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::ListView) {
            unsupported_kind(common_.kind[idx]);
            return 0;
        }
        const auto* payload = payload_get<soa_detail::ListViewPayload>(idx);
        return payload ? payload->overscan : 0;
    }

    void SoaKernel::set_table_view_source(WidgetHandle h, std::uint16_t rows, std::uint8_t cols,
                               const void* ctx, soa_detail::TableViewTextFn fn) noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return;
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::TableView) {
            unsupported_kind(common_.kind[idx]);
            return;
        }
        auto* payload = payload_get<soa_detail::TableViewPayload>(idx);
        if (!payload) return;
        payload->text_ctx = ctx;
        payload->text_fn = fn;
        payload->row_count = rows;
        payload->col_count = cols;
        if (payload->scroll_x != 0) {
            payload->scroll_x = clamp_scroll_x(h, payload->scroll_x);
        }
        mark_layout_dirty();
    }

    void SoaKernel::set_table_view_header(WidgetHandle h, const void* ctx,
                               soa_detail::TableViewHeaderFn fn) noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return;
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::TableView) {
            unsupported_kind(common_.kind[idx]);
            return;
        }
        auto* payload = payload_get<soa_detail::TableViewPayload>(idx);
        if (!payload) return;
        if (payload->header_ctx != ctx || payload->header_fn != fn) {
            payload->header_ctx = ctx;
            payload->header_fn = fn;
            if (payload->scroll_y != 0) {
                set_scroll_y_clamped(h, payload->scroll_y);
            }
            mark_paint_dirty();
        }
    }

    void SoaKernel::set_table_view_header_height(WidgetHandle h, int height) noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return;
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::TableView) {
            unsupported_kind(common_.kind[idx]);
            return;
        }
        auto* payload = payload_get<soa_detail::TableViewPayload>(idx);
        if (!payload) return;
        if (height < 0) height = 0;
        if (payload->header_height != height) {
            payload->header_height = height;
            if (payload->scroll_y != 0) {
                set_scroll_y_clamped(h, payload->scroll_y);
            }
            mark_layout_dirty();
        }
    }

    void SoaKernel::set_table_view_header_padding(WidgetHandle h, int padding) noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return;
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::TableView) {
            unsupported_kind(common_.kind[idx]);
            return;
        }
        auto* payload = payload_get<soa_detail::TableViewPayload>(idx);
        if (!payload) return;
        if (padding < 0) padding = 0;
        if (payload->header_padding != padding) {
            payload->header_padding = padding;
            mark_paint_dirty();
        }
    }

    void SoaKernel::set_table_view_header_style(WidgetHandle h, TableViewHeaderStyle style) noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return;
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::TableView) {
            unsupported_kind(common_.kind[idx]);
            return;
        }
        auto* payload = payload_get<soa_detail::TableViewPayload>(idx);
        if (!payload) return;
        const std::uint8_t next = static_cast<std::uint8_t>(style);
        if (payload->header_style != next) {
            payload->header_style = next;
            mark_paint_dirty();
        }
    }

    void SoaKernel::set_table_view_header_divider(WidgetHandle h, bool enabled) noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return;
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::TableView) {
            unsupported_kind(common_.kind[idx]);
            return;
        }
        auto* payload = payload_get<soa_detail::TableViewPayload>(idx);
        if (!payload) return;
        const std::uint8_t next = enabled ? 1 : 0;
        if (payload->header_divider != next) {
            payload->header_divider = next;
            mark_paint_dirty();
        }
    }

    void SoaKernel::set_table_view_col_dividers(WidgetHandle h, bool enabled) noexcept {
        set_table_view_col_divider_style(
            h,
            enabled ? TableViewColDividerStyle::Full : TableViewColDividerStyle::None);
    }

    void SoaKernel::set_table_view_col_divider_style(WidgetHandle h, TableViewColDividerStyle style) noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return;
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::TableView) {
            unsupported_kind(common_.kind[idx]);
            return;
        }
        auto* payload = payload_get<soa_detail::TableViewPayload>(idx);
        if (!payload) return;
        const std::uint8_t next = static_cast<std::uint8_t>(style);
        if (payload->col_dividers != next) {
            payload->col_dividers = next;
            mark_paint_dirty();
        }
    }

    void SoaKernel::set_table_view_count(WidgetHandle h, std::uint16_t rows) noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return;
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::TableView) {
            unsupported_kind(common_.kind[idx]);
            return;
        }
        auto* payload = payload_get<soa_detail::TableViewPayload>(idx);
        if (!payload) return;
        payload->row_count = rows;
        mark_layout_dirty();
    }

    std::uint16_t SoaKernel::table_view_row_count(WidgetHandle h) const noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return 0;
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::TableView) {
            unsupported_kind(common_.kind[idx]);
            return 0;
        }
        const auto* payload = payload_get<soa_detail::TableViewPayload>(idx);
        return payload ? payload->row_count : 0;
    }

    bool SoaKernel::table_view_has_header(WidgetHandle h) const noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return false;
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::TableView) {
            unsupported_kind(common_.kind[idx]);
            return false;
        }
        const auto* payload = payload_get<soa_detail::TableViewPayload>(idx);
        return payload ? (payload->header_fn != nullptr) : false;
    }

    int SoaKernel::table_view_header_height(WidgetHandle h) const noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return 0;
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::TableView) {
            unsupported_kind(common_.kind[idx]);
            return 0;
        }
        const auto* payload = payload_get<soa_detail::TableViewPayload>(idx);
        if (!payload || !payload->header_fn) return 0;
        int height = payload->header_height;
        if (height <= 0) height = payload->row_height;
        return height > 0 ? height : 0;
    }

    int SoaKernel::table_view_header_padding(WidgetHandle h) const noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return 0;
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::TableView) {
            unsupported_kind(common_.kind[idx]);
            return 0;
        }
        const auto* payload = payload_get<soa_detail::TableViewPayload>(idx);
        return payload ? payload->header_padding : 0;
    }

    TableViewHeaderStyle SoaKernel::table_view_header_style(WidgetHandle h) const noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return TableViewHeaderStyle::Default;
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::TableView) {
            unsupported_kind(common_.kind[idx]);
            return TableViewHeaderStyle::Default;
        }
        const auto* payload = payload_get<soa_detail::TableViewPayload>(idx);
        return payload ? static_cast<TableViewHeaderStyle>(payload->header_style)
                       : TableViewHeaderStyle::Default;
    }

    bool SoaKernel::table_view_header_divider(WidgetHandle h) const noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return false;
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::TableView) {
            unsupported_kind(common_.kind[idx]);
            return false;
        }
        const auto* payload = payload_get<soa_detail::TableViewPayload>(idx);
        return payload ? (payload->header_divider != 0) : false;
    }

    bool SoaKernel::table_view_col_dividers(WidgetHandle h) const noexcept {
        return table_view_col_divider_style(h) != TableViewColDividerStyle::None;
    }

    TableViewColDividerStyle SoaKernel::table_view_col_divider_style(WidgetHandle h) const noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return TableViewColDividerStyle::None;
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::TableView) {
            unsupported_kind(common_.kind[idx]);
            return TableViewColDividerStyle::None;
        }
        const auto* payload = payload_get<soa_detail::TableViewPayload>(idx);
        if (!payload) return TableViewColDividerStyle::None;
        return static_cast<TableViewColDividerStyle>(payload->col_dividers);
    }

    std::uint8_t SoaKernel::table_view_col_count(WidgetHandle h) const noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return 0;
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::TableView) {
            unsupported_kind(common_.kind[idx]);
            return 0;
        }
        const auto* payload = payload_get<soa_detail::TableViewPayload>(idx);
        return payload ? payload->col_count : 0;
    }

    const char* SoaKernel::table_view_header_text(WidgetHandle h, std::uint8_t col) const noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return "";
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::TableView) {
            unsupported_kind(common_.kind[idx]);
            return "";
        }
        const auto* payload = payload_get<soa_detail::TableViewPayload>(idx);
        if (!payload || !payload->header_fn) return "";
        if (col >= payload->col_count) return "";
        const char* text = payload->header_fn(payload->header_ctx, col);
        return text ? text : "";
    }

    bool SoaKernel::table_view_has_col_width_fn(WidgetHandle h) const noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return false;
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::TableView) {
            unsupported_kind(common_.kind[idx]);
            return false;
        }
        const auto* payload = payload_get<soa_detail::TableViewPayload>(idx);
        return payload ? (payload->col_width_fn != nullptr) : false;
    }

    int SoaKernel::table_view_col_width(WidgetHandle h) const noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return 0;
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::TableView) {
            unsupported_kind(common_.kind[idx]);
            return 0;
        }
        const auto* payload = payload_get<soa_detail::TableViewPayload>(idx);
        return payload ? payload->col_width : 0;
    }

    int SoaKernel::table_view_col_width_at(WidgetHandle h, std::uint8_t col) const noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return 0;
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::TableView) {
            unsupported_kind(common_.kind[idx]);
            return 0;
        }
        const auto* payload = payload_get<soa_detail::TableViewPayload>(idx);
        if (!payload) return 0;
        if (payload->col_width_fn) {
            int w = payload->col_width_fn(payload->col_width_ctx, col);
            if (w < 0) w = 0;
            return w;
        }
        return payload->col_width;
    }

    int SoaKernel::table_view_scroll_x(WidgetHandle h) const noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return 0;
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::TableView) {
            unsupported_kind(common_.kind[idx]);
            return 0;
        }
        const auto* payload = payload_get<soa_detail::TableViewPayload>(idx);
        return payload ? payload->scroll_x : 0;
    }

    void SoaKernel::set_table_view_col_width(WidgetHandle h, int col_width) noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return;
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::TableView) {
            unsupported_kind(common_.kind[idx]);
            return;
        }
        auto* payload = payload_get<soa_detail::TableViewPayload>(idx);
        if (!payload) return;
        if (col_width < 0) col_width = 0;
        if (payload->col_width != col_width || payload->col_width_fn || payload->col_width_ctx) {
            payload->col_width = col_width;
            payload->col_width_ctx = nullptr;
            payload->col_width_fn = nullptr;
            if (payload->scroll_x != 0) {
                payload->scroll_x = clamp_scroll_x(h, payload->scroll_x);
            }
            mark_paint_dirty();
        }
    }

    void SoaKernel::set_table_view_col_width_fn(WidgetHandle h, const void* ctx,
                                     soa_detail::TableViewColWidthFn fn) noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return;
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::TableView) {
            unsupported_kind(common_.kind[idx]);
            return;
        }
        auto* payload = payload_get<soa_detail::TableViewPayload>(idx);
        if (!payload) return;
        if (payload->col_width_ctx != ctx || payload->col_width_fn != fn) {
            payload->col_width_ctx = fn ? ctx : nullptr;
            payload->col_width_fn = fn;
            if (fn) {
                payload->col_width = 0;
            }
            if (payload->scroll_x != 0) {
                payload->scroll_x = clamp_scroll_x(h, payload->scroll_x);
            }
            mark_paint_dirty();
        }
    }

    void SoaKernel::set_table_view_scroll_x(WidgetHandle h, int x) noexcept {
        set_table_view_scroll_x_clamped(h, x);
    }

    std::uint8_t SoaKernel::table_view_overscan(WidgetHandle h) const noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return 0;
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::TableView) {
            unsupported_kind(common_.kind[idx]);
            return 0;
        }
        const auto* payload = payload_get<soa_detail::TableViewPayload>(idx);
        return payload ? payload->overscan : 0;
    }

    const char* SoaKernel::table_view_cell_text(WidgetHandle h, std::uint16_t row, std::uint8_t col) const noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return "";
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::TableView) {
            unsupported_kind(common_.kind[idx]);
            return "";
        }
        const auto* payload = payload_get<soa_detail::TableViewPayload>(idx);
        if (!payload) return "";
        if (row >= payload->row_count) return "";
        if (col >= payload->col_count) return "";
        if (!payload->text_fn) return "";
        const char* text = payload->text_fn(payload->text_ctx, row, col);
        return text ? text : "";
    }

    void SoaKernel::set_tree_view_source(WidgetHandle h, std::uint16_t count,
                              const void* text_ctx, soa_detail::TreeViewTextFn text_fn,
                              const void* indent_ctx, soa_detail::TreeViewIndentFn indent_fn) noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return;
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::TreeView) {
            unsupported_kind(common_.kind[idx]);
            return;
        }
        auto* payload = payload_get<soa_detail::TreeViewPayload>(idx);
        if (!payload) return;
        payload->text_ctx = text_ctx;
        payload->text_fn = text_fn;
        payload->indent_ctx = indent_ctx;
        payload->indent_fn = indent_fn;
        payload->count = count;
        mark_layout_dirty();
    }

    void SoaKernel::set_tree_view_count(WidgetHandle h, std::uint16_t count) noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return;
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::TreeView) {
            unsupported_kind(common_.kind[idx]);
            return;
        }
        auto* payload = payload_get<soa_detail::TreeViewPayload>(idx);
        if (!payload) return;
        payload->count = count;
        mark_layout_dirty();
    }

    std::uint16_t SoaKernel::tree_view_count(WidgetHandle h) const noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return 0;
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::TreeView) {
            unsupported_kind(common_.kind[idx]);
            return 0;
        }
        const auto* payload = payload_get<soa_detail::TreeViewPayload>(idx);
        return payload ? payload->count : 0;
    }

    std::uint8_t SoaKernel::tree_view_overscan(WidgetHandle h) const noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return 0;
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::TreeView) {
            unsupported_kind(common_.kind[idx]);
            return 0;
        }
        const auto* payload = payload_get<soa_detail::TreeViewPayload>(idx);
        return payload ? payload->overscan : 0;
    }

    std::uint8_t SoaKernel::tree_view_indent_px(WidgetHandle h) const noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return 0;
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::TreeView) {
            unsupported_kind(common_.kind[idx]);
            return 0;
        }
        const auto* payload = payload_get<soa_detail::TreeViewPayload>(idx);
        return payload ? payload->indent_px : 0;
    }

    int SoaKernel::tree_view_max_indent_px(WidgetHandle h) const noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return 0;
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::TreeView) {
            unsupported_kind(common_.kind[idx]);
            return 0;
        }
        const auto* payload = payload_get<soa_detail::TreeViewPayload>(idx);
        return payload ? static_cast<int>(payload->max_indent_px) : 0;
    }

    int SoaKernel::tree_view_min_text_avail_px(WidgetHandle h) const noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return 0;
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::TreeView) {
            unsupported_kind(common_.kind[idx]);
            return 0;
        }
        const auto* payload = payload_get<soa_detail::TreeViewPayload>(idx);
        return payload ? static_cast<int>(payload->min_text_avail_px) : 0;
    }

    void SoaKernel::set_tree_view_indent_px(WidgetHandle h, std::uint8_t px) noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return;
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::TreeView) {
            unsupported_kind(common_.kind[idx]);
            return;
        }
        auto* payload = payload_get<soa_detail::TreeViewPayload>(idx);
        if (!payload) return;
        if (payload->indent_px != px) {
            payload->indent_px = px;
            mark_paint_dirty();
        }
    }

    void SoaKernel::set_tree_view_max_indent_px(WidgetHandle h, int px) noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return;
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::TreeView) {
            unsupported_kind(common_.kind[idx]);
            return;
        }
        if (px < 0) px = 0;
        if (px > 0xFFFF) px = 0xFFFF;
        auto* payload = payload_get<soa_detail::TreeViewPayload>(idx);
        if (!payload) return;
        const auto next = static_cast<std::uint16_t>(px);
        if (payload->max_indent_px != next) {
            payload->max_indent_px = next;
            mark_paint_dirty();
        }
    }

    void SoaKernel::set_tree_view_min_text_avail_px(WidgetHandle h, int px) noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return;
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::TreeView) {
            unsupported_kind(common_.kind[idx]);
            return;
        }
        if (px < 0) px = 0;
        if (px > 0xFFFF) px = 0xFFFF;
        auto* payload = payload_get<soa_detail::TreeViewPayload>(idx);
        if (!payload) return;
        const auto next = static_cast<std::uint16_t>(px);
        if (payload->min_text_avail_px != next) {
            payload->min_text_avail_px = next;
            mark_paint_dirty();
        }
    }

    const char* SoaKernel::tree_view_item_text(WidgetHandle h, std::uint16_t index) const noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return "";
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::TreeView) {
            unsupported_kind(common_.kind[idx]);
            return "";
        }
        const auto* payload = payload_get<soa_detail::TreeViewPayload>(idx);
        if (!payload) return "";
        if (index >= payload->count) return "";
        if (!payload->text_fn) return "";
        const char* text = payload->text_fn(payload->text_ctx, index);
        return text ? text : "";
    }

    std::uint8_t SoaKernel::tree_view_item_indent(WidgetHandle h, std::uint16_t index) const noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return 0;
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::TreeView) {
            unsupported_kind(common_.kind[idx]);
            return 0;
        }
        const auto* payload = payload_get<soa_detail::TreeViewPayload>(idx);
        if (!payload) return 0;
        if (index >= payload->count) return 0;
        if (!payload->indent_fn) return 0;
        return payload->indent_fn(payload->indent_ctx, index);
    }

    int SoaKernel::compute_content_height(WidgetHandle h) const noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return 0;
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload == soa_detail::PayloadKind::TextList) {
            const auto* payload = payload_get<soa_detail::TextListPayload>(idx);
            if (!payload) return 0;
            const int count = payload->count;
            const int row_h = payload->row_height;
            if (count <= 0 || row_h <= 0) return 0;
            return count * row_h;
        }
        if (desc.payload == soa_detail::PayloadKind::ListView) {
            const auto* payload = payload_get<soa_detail::ListViewPayload>(idx);
            if (!payload) return 0;
            const int count = payload->count;
            const int row_h = payload->row_height;
            if (count <= 0 || row_h <= 0) return 0;
            return count * row_h;
        }
        if (desc.payload == soa_detail::PayloadKind::TableView) {
            const auto* payload = payload_get<soa_detail::TableViewPayload>(idx);
            if (!payload) return 0;
            const int count = payload->row_count;
            const int row_h = payload->row_height;
            if (row_h <= 0) return 0;
            int total = count * row_h;
            if (payload->header_fn) {
                int header_h = payload->header_height;
                if (header_h <= 0) header_h = row_h;
                if (header_h > 0) total += header_h;
            }
            return total;
        }
        if (desc.payload == soa_detail::PayloadKind::TreeView) {
            const auto* payload = payload_get<soa_detail::TreeViewPayload>(idx);
            if (!payload) return 0;
            const int count = payload->count;
            const int row_h = payload->row_height;
            if (count <= 0 || row_h <= 0) return 0;
            return count * row_h;
        }
        if (desc.payload == soa_detail::PayloadKind::NumberList) {
            const auto* payload = payload_get<soa_detail::NumberListPayload>(idx);
            if (!payload) return 0;
            const int count = payload->count;
            const int row_h = payload->row_height;
            if (count <= 0 || row_h <= 0) return 0;
            return count * row_h;
        }
        if (desc.payload == soa_detail::PayloadKind::Roller) {
            const auto* payload = payload_get<soa_detail::RollerPayload>(idx);
            if (!payload) return 0;
            const int count = payload->count;
            const int row_h = payload->row_height;
            if (count <= 0 || row_h <= 0) return 0;
            return count * row_h;
        }
        int max_bottom = 0;
        std::uint16_t child = common_.first_child[idx];
        while (child != kInvalidIndex) {
            const Rect r = common_.rects[child];
            const int bottom = r.y + r.h;
            if (bottom > max_bottom) {
                max_bottom = bottom;
            }
            child = common_.next_sibling[child];
        }
        return max_bottom;
    }

    int SoaKernel::max_scroll(WidgetHandle h) const noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return 0;
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload == soa_detail::PayloadKind::NumberList) {
            const auto* payload = payload_get<soa_detail::NumberListPayload>(idx);
            if (!payload) return 0;
            const int count = payload->count;
            const int row_h = payload->row_height;
            if (count <= 1 || row_h <= 0) return 0;
            return (count - 1) * row_h;
        }
        if (desc.payload == soa_detail::PayloadKind::Roller) {
            const auto* payload = payload_get<soa_detail::RollerPayload>(idx);
            if (!payload) return 0;
            const int count = payload->count;
            const int row_h = payload->row_height;
            if (count <= 1 || row_h <= 0) return 0;
            return (count - 1) * row_h;
        }
        const Rect r = common_.rects[idx];
        const int content_h = compute_content_height(h);
        int max_scroll = content_h - r.h;
        if (max_scroll < 0) max_scroll = 0;
        return max_scroll;
    }

    int SoaKernel::clamp_scroll_y(WidgetHandle h, int y) const noexcept {
        const int max_scroll_value = max_scroll(h);
        if (y < 0) return 0;
        if (y > max_scroll_value) return max_scroll_value;
        return y;
    }

    int SoaKernel::table_view_content_width(WidgetHandle h) const noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return 0;
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::TableView) return 0;
        const auto* payload = payload_get<soa_detail::TableViewPayload>(idx);
        if (!payload) return 0;
        const std::uint8_t cols = payload->col_count;
        if (cols == 0) return 0;
        if (!payload->col_width_fn) {
            if (payload->col_width <= 0) return 0;
            return payload->col_width * static_cast<int>(cols);
        }
        int total = 0;
        for (std::uint8_t col = 0; col < cols; ++col) {
            int w = payload->col_width_fn(payload->col_width_ctx, col);
            if (w <= 0) w = 1;
            total += w;
        }
        return total;
    }

    int SoaKernel::max_scroll_x(WidgetHandle h) const noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return 0;
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::TableView) return 0;
        const Rect r = common_.rects[idx];
        int pad = 0;
        const StyleState state = input_make_state(*this, h);
        const ResolvedStyleView view = StyleSheet::instance().lookup(common_.kind[idx], state);
        if (view.metrics) {
            pad = view.metrics->padding;
            if (pad < 0) pad = 0;
        }
        int viewport = r.w - pad * 2;
        if (viewport < 0) viewport = 0;
        const int content_w = table_view_content_width(h);
        int max_scroll = content_w - viewport;
        if (max_scroll < 0) max_scroll = 0;
        return max_scroll;
    }

    int SoaKernel::clamp_scroll_x(WidgetHandle h, int x) const noexcept {
        const int max_scroll_value = max_scroll_x(h);
        if (x < 0) return 0;
        if (x > max_scroll_value) return max_scroll_value;
        return x;
    }

    void SoaKernel::set_table_view_scroll_x_clamped(WidgetHandle h, int x) noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return;
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::TableView) {
            unsupported_kind(common_.kind[idx]);
            return;
        }
        auto* payload = payload_get<soa_detail::TableViewPayload>(idx);
        if (!payload) return;
        const int clamped = clamp_scroll_x(h, x);
        if (payload->scroll_x != clamped) {
            payload->scroll_x = clamped;
            mark_paint_dirty();
        }
    }

    void SoaKernel::set_scroll_y_clamped(WidgetHandle h, int y) noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return;
        const int clamped = clamp_scroll_y(h, y);
        const auto desc = payload_descriptor(common_.kind[idx]);
        switch (desc.payload) {
        case soa_detail::PayloadKind::List: {
            auto* payload = payload_get<soa_detail::ListPayload>(idx);
            if (!payload) return;
            if (payload->scroll_y != clamped) {
                payload->scroll_y = clamped;
                mark_paint_dirty();
            }
            break;
        }
        case soa_detail::PayloadKind::TextList: {
            auto* payload = payload_get<soa_detail::TextListPayload>(idx);
            if (!payload) return;
            if (payload->scroll_y != clamped) {
                payload->scroll_y = clamped;
                mark_paint_dirty();
            }
            break;
        }
        case soa_detail::PayloadKind::ListView: {
            auto* payload = payload_get<soa_detail::ListViewPayload>(idx);
            if (!payload) return;
            if (payload->scroll_y != clamped) {
                payload->scroll_y = clamped;
                mark_paint_dirty();
            }
            break;
        }
        case soa_detail::PayloadKind::TableView: {
            auto* payload = payload_get<soa_detail::TableViewPayload>(idx);
            if (!payload) return;
            if (payload->scroll_y != clamped) {
                payload->scroll_y = clamped;
                mark_paint_dirty();
            }
            break;
        }
        case soa_detail::PayloadKind::TreeView: {
            auto* payload = payload_get<soa_detail::TreeViewPayload>(idx);
            if (!payload) return;
            if (payload->scroll_y != clamped) {
                payload->scroll_y = clamped;
                mark_paint_dirty();
            }
            break;
        }
        case soa_detail::PayloadKind::NumberList: {
            auto* payload = payload_get<soa_detail::NumberListPayload>(idx);
            if (!payload) return;
            const int count = payload->count;
            const int row_h = payload->row_height;
            int max_scroll_value = (count > 1 && row_h > 0) ? (count - 1) * row_h : 0;
            int next = y;
            if (next < 0) next = 0;
            if (next > max_scroll_value) next = max_scroll_value;
            if (payload->scroll_y != next) {
                payload->scroll_y = next;
                if (row_h > 0 && count > 0) {
                    int idx_from_scroll = (next + row_h / 2) / row_h;
                    if (idx_from_scroll < 0) idx_from_scroll = 0;
                    if (idx_from_scroll >= count) idx_from_scroll = count - 1;
                    payload->selected = idx_from_scroll;
                }
                mark_paint_dirty();
            }
            break;
        }
        case soa_detail::PayloadKind::Roller: {
            auto* payload = payload_get<soa_detail::RollerPayload>(idx);
            if (!payload) return;
            const int count = payload->count;
            const int row_h = payload->row_height;
            int total = (count > 0 && row_h > 0) ? (count * row_h) : 0;
            int next = y;
            if (total > 0) {
                next %= total;
                if (next < 0) next += total;
            } else {
                next = 0;
            }
            if (payload->scroll_y != next) {
                payload->scroll_y = next;
                if (row_h > 0 && count > 0) {
                    int idx_from_scroll = (next + row_h / 2) / row_h;
                    idx_from_scroll %= count;
                    if (idx_from_scroll < 0) idx_from_scroll += count;
                    payload->selected = idx_from_scroll;
                }
                mark_paint_dirty();
            }
            break;
        }
        case soa_detail::PayloadKind::ScrollContainer: {
            auto* payload = payload_get<soa_detail::ScrollContainerPayload>(idx);
            if (!payload) return;
            if (payload->scroll_y != clamped) {
                payload->scroll_y = clamped;
                mark_paint_dirty();
            }
            break;
        }
        default:
            unsupported_kind(common_.kind[idx]);
            break;
        }
    }
