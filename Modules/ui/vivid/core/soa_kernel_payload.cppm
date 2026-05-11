module;
#include <cstddef>
#include <cstdint>
#include <cstring>

export module charm.core.soa_kernel:payload;

import :kernel_class;
import :types;
import charm.core.handle;
import charm.core.soa_payload;
import charm.core.soa_registry;
import charm.core.style;
import charm.core.style_sheet;
import alg_list_scroll;

    soa_detail::TextSlotId SoaKernel::alloc_text_slot() noexcept {
        return payloads_.alloc_text_slot();
    }

    void SoaKernel::free_text_slot(soa_detail::TextSlotId slot) noexcept {
        payloads_.free_text_slot(slot);
    }

    void SoaKernel::set_text(WidgetHandle h, const char* text) noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return;
        const auto id = payloads_.store_text(text);
        const auto desc = payload_descriptor(common_.kind[idx]);
        switch (desc.payload) {
        case soa_detail::PayloadKind::Label: {
            auto* payload = payload_get<soa_detail::LabelPayload>(idx);
            if (!payload) return;
            payload->text = id;
            break;
        }
        case soa_detail::PayloadKind::Button: {
            auto* payload = payload_get<soa_detail::ButtonPayload>(idx);
            if (!payload) return;
            payload->text = id;
            break;
        }
        case soa_detail::PayloadKind::TextInput: {
            auto* payload = payload_get<soa_detail::TextInputPayload>(idx);
            if (!payload) return;
            payload->text = id;
            break;
        }
        case soa_detail::PayloadKind::TextArea: {
            auto* payload = payload_get<soa_detail::TextAreaPayload>(idx);
            if (!payload) return;
            payload->text = id;
            break;
        }
        case soa_detail::PayloadKind::NumberInput: {
            auto* payload = payload_get<soa_detail::NumberInputPayload>(idx);
            if (!payload) return;
            payload->text = id;
            break;
        }
        case soa_detail::PayloadKind::Checkbox: {
            auto* payload = payload_get<soa_detail::CheckboxPayload>(idx);
            if (!payload) return;
            payload->text = id;
            break;
        }
        case soa_detail::PayloadKind::Radio: {
            auto* payload = payload_get<soa_detail::RadioPayload>(idx);
            if (!payload) return;
            payload->text = id;
            break;
        }
        case soa_detail::PayloadKind::ListItem: {
            auto* payload = payload_get<soa_detail::ListItemPayload>(idx);
            if (!payload) return;
            payload->text = id;
            break;
        }
        case soa_detail::PayloadKind::TextList: {
            auto* payload = payload_get<soa_detail::TextListPayload>(idx);
            if (!payload) return;
            if (payload->count == 0) {
                payload->count = 1;
            }
            payload->items[0] = id;
            break;
        }
        case soa_detail::PayloadKind::None:
        case soa_detail::PayloadKind::Image:
        case soa_detail::PayloadKind::SegmentedControl:
        case soa_detail::PayloadKind::ToggleGroup:
        case soa_detail::PayloadKind::Switch:
        case soa_detail::PayloadKind::Slider:
        case soa_detail::PayloadKind::ScrollBar:
        case soa_detail::PayloadKind::Progress:
        case soa_detail::PayloadKind::List:
        case soa_detail::PayloadKind::ScrollContainer:
        case soa_detail::PayloadKind::Spinner:
            unsupported_kind(common_.kind[idx]);
            break;
        default:
            unsupported_kind(common_.kind[idx]);
            break;
        }
        mark_layout_dirty();
    }

    void SoaKernel::set_text_static(WidgetHandle h, const char* text) noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return;
        const auto id = payloads_.store_text_static(text);
        const auto desc = payload_descriptor(common_.kind[idx]);
        switch (desc.payload) {
        case soa_detail::PayloadKind::Label: {
            auto* payload = payload_get<soa_detail::LabelPayload>(idx);
            if (!payload) return;
            payload->text = id;
            break;
        }
        case soa_detail::PayloadKind::Button: {
            auto* payload = payload_get<soa_detail::ButtonPayload>(idx);
            if (!payload) return;
            payload->text = id;
            break;
        }
        case soa_detail::PayloadKind::TextInput: {
            auto* payload = payload_get<soa_detail::TextInputPayload>(idx);
            if (!payload) return;
            payload->text = id;
            break;
        }
        case soa_detail::PayloadKind::TextArea: {
            auto* payload = payload_get<soa_detail::TextAreaPayload>(idx);
            if (!payload) return;
            payload->text = id;
            break;
        }
        case soa_detail::PayloadKind::NumberInput: {
            auto* payload = payload_get<soa_detail::NumberInputPayload>(idx);
            if (!payload) return;
            payload->text = id;
            break;
        }
        case soa_detail::PayloadKind::Checkbox: {
            auto* payload = payload_get<soa_detail::CheckboxPayload>(idx);
            if (!payload) return;
            payload->text = id;
            break;
        }
        case soa_detail::PayloadKind::Radio: {
            auto* payload = payload_get<soa_detail::RadioPayload>(idx);
            if (!payload) return;
            payload->text = id;
            break;
        }
        case soa_detail::PayloadKind::ListItem: {
            auto* payload = payload_get<soa_detail::ListItemPayload>(idx);
            if (!payload) return;
            payload->text = id;
            break;
        }
        case soa_detail::PayloadKind::TextList: {
            auto* payload = payload_get<soa_detail::TextListPayload>(idx);
            if (!payload) return;
            if (payload->count == 0) {
                payload->count = 1;
            }
            payload->items[0] = id;
            break;
        }
        case soa_detail::PayloadKind::None:
        case soa_detail::PayloadKind::Image:
        case soa_detail::PayloadKind::SegmentedControl:
        case soa_detail::PayloadKind::ToggleGroup:
        case soa_detail::PayloadKind::Switch:
        case soa_detail::PayloadKind::Slider:
        case soa_detail::PayloadKind::ScrollBar:
        case soa_detail::PayloadKind::Progress:
        case soa_detail::PayloadKind::List:
        case soa_detail::PayloadKind::ScrollContainer:
        case soa_detail::PayloadKind::Spinner:
            unsupported_kind(common_.kind[idx]);
            break;
        default:
            unsupported_kind(common_.kind[idx]);
            break;
        }
        mark_layout_dirty();
    }

    void SoaKernel::set_text_slot(WidgetHandle h, soa_detail::TextSlotId slot, const char* text) noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return;
        const auto id = payloads_.store_text_slot(slot, text);
        const auto desc = payload_descriptor(common_.kind[idx]);
        switch (desc.payload) {
        case soa_detail::PayloadKind::Label: {
            auto* payload = payload_get<soa_detail::LabelPayload>(idx);
            if (!payload) return;
            payload->text = id;
            break;
        }
        case soa_detail::PayloadKind::Button: {
            auto* payload = payload_get<soa_detail::ButtonPayload>(idx);
            if (!payload) return;
            payload->text = id;
            break;
        }
        case soa_detail::PayloadKind::TextInput: {
            auto* payload = payload_get<soa_detail::TextInputPayload>(idx);
            if (!payload) return;
            payload->text = id;
            break;
        }
        case soa_detail::PayloadKind::TextArea: {
            auto* payload = payload_get<soa_detail::TextAreaPayload>(idx);
            if (!payload) return;
            payload->text = id;
            break;
        }
        case soa_detail::PayloadKind::NumberInput: {
            auto* payload = payload_get<soa_detail::NumberInputPayload>(idx);
            if (!payload) return;
            payload->text = id;
            break;
        }
        case soa_detail::PayloadKind::Checkbox: {
            auto* payload = payload_get<soa_detail::CheckboxPayload>(idx);
            if (!payload) return;
            payload->text = id;
            break;
        }
        case soa_detail::PayloadKind::Radio: {
            auto* payload = payload_get<soa_detail::RadioPayload>(idx);
            if (!payload) return;
            payload->text = id;
            break;
        }
        case soa_detail::PayloadKind::ListItem: {
            auto* payload = payload_get<soa_detail::ListItemPayload>(idx);
            if (!payload) return;
            payload->text = id;
            break;
        }
        case soa_detail::PayloadKind::TextList: {
            auto* payload = payload_get<soa_detail::TextListPayload>(idx);
            if (!payload) return;
            if (payload->count == 0) {
                payload->count = 1;
            }
            payload->items[0] = id;
            break;
        }
        case soa_detail::PayloadKind::None:
        case soa_detail::PayloadKind::Image:
        case soa_detail::PayloadKind::SegmentedControl:
        case soa_detail::PayloadKind::ToggleGroup:
        case soa_detail::PayloadKind::Switch:
        case soa_detail::PayloadKind::Slider:
        case soa_detail::PayloadKind::ScrollBar:
        case soa_detail::PayloadKind::Progress:
        case soa_detail::PayloadKind::List:
        case soa_detail::PayloadKind::ScrollContainer:
        case soa_detail::PayloadKind::Spinner:
            unsupported_kind(common_.kind[idx]);
            break;
        default:
            unsupported_kind(common_.kind[idx]);
            break;
        }
        mark_layout_dirty();
    }

    const char* SoaKernel::text(WidgetHandle h) const noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return "";
        const auto desc = payload_descriptor(common_.kind[idx]);
        switch (desc.payload) {
        case soa_detail::PayloadKind::Label: {
            const auto* payload = payload_get<soa_detail::LabelPayload>(idx);
            return payload ? payloads_.text_c_str(payload->text) : "";
        }
        case soa_detail::PayloadKind::Button: {
            const auto* payload = payload_get<soa_detail::ButtonPayload>(idx);
            return payload ? payloads_.text_c_str(payload->text) : "";
        }
        case soa_detail::PayloadKind::TextInput: {
            const auto* payload = payload_get<soa_detail::TextInputPayload>(idx);
            return payload ? payloads_.text_c_str(payload->text) : "";
        }
        case soa_detail::PayloadKind::TextArea: {
            const auto* payload = payload_get<soa_detail::TextAreaPayload>(idx);
            return payload ? payloads_.text_c_str(payload->text) : "";
        }
        case soa_detail::PayloadKind::NumberInput: {
            const auto* payload = payload_get<soa_detail::NumberInputPayload>(idx);
            return payload ? payloads_.text_c_str(payload->text) : "";
        }
        case soa_detail::PayloadKind::Checkbox: {
            const auto* payload = payload_get<soa_detail::CheckboxPayload>(idx);
            return payload ? payloads_.text_c_str(payload->text) : "";
        }
        case soa_detail::PayloadKind::Radio: {
            const auto* payload = payload_get<soa_detail::RadioPayload>(idx);
            return payload ? payloads_.text_c_str(payload->text) : "";
        }
        case soa_detail::PayloadKind::ListItem: {
            const auto* payload = payload_get<soa_detail::ListItemPayload>(idx);
            return payload ? payloads_.text_c_str(payload->text) : "";
        }
        case soa_detail::PayloadKind::TextList: {
            const auto* payload = payload_get<soa_detail::TextListPayload>(idx);
            if (!payload || payload->count == 0) return "";
            return payloads_.text_c_str(payload->items[0]);
        }
        case soa_detail::PayloadKind::None:
        case soa_detail::PayloadKind::Image:
        case soa_detail::PayloadKind::SegmentedControl:
        case soa_detail::PayloadKind::ToggleGroup:
        case soa_detail::PayloadKind::Switch:
        case soa_detail::PayloadKind::Slider:
        case soa_detail::PayloadKind::ScrollBar:
        case soa_detail::PayloadKind::Progress:
        case soa_detail::PayloadKind::List:
        case soa_detail::PayloadKind::ScrollContainer:
        case soa_detail::PayloadKind::Spinner:
            unsupported_kind(common_.kind[idx]);
            return "";
        default:
            unsupported_kind(common_.kind[idx]);
            return "";
        }
        return "";
    }

    void SoaKernel::set_image(WidgetHandle h, soa_detail::ImageId image) noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return;
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::Image) {
            unsupported_kind(common_.kind[idx]);
            return;
        }
        auto* payload = payload_get<soa_detail::ImagePayload>(idx);
        if (!payload) return;
        if (payload->image != image) {
            payload->image = image;
            mark_paint_dirty();
        }
    }

    soa_detail::ImageId SoaKernel::image(WidgetHandle h) const noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return soa_detail::invalid_image_id();
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::Image) {
            unsupported_kind(common_.kind[idx]);
            return soa_detail::invalid_image_id();
        }
        const auto* payload = payload_get<soa_detail::ImagePayload>(idx);
        return payload ? payload->image : soa_detail::invalid_image_id();
    }

    void SoaKernel::set_image_shape(WidgetHandle h,
                                    soa_detail::ImageShapeKind kind,
                                    std::uint8_t extent) noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return;
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::Image) {
            unsupported_kind(common_.kind[idx]);
            return;
        }
        auto* payload = payload_get<soa_detail::ImagePayload>(idx);
        if (!payload) return;
        const auto kind_u8 = static_cast<std::uint8_t>(kind);
        if (payload->shape_kind != kind_u8 || payload->shape_extent != extent) {
            payload->shape_kind = kind_u8;
            payload->shape_extent = extent;
            mark_paint_dirty();
        }
    }

    soa_detail::ImageShapeKind SoaKernel::image_shape_kind(WidgetHandle h) const noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return soa_detail::ImageShapeKind::Auto;
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::Image) {
            unsupported_kind(common_.kind[idx]);
            return soa_detail::ImageShapeKind::Auto;
        }
        const auto* payload = payload_get<soa_detail::ImagePayload>(idx);
        return payload
            ? static_cast<soa_detail::ImageShapeKind>(payload->shape_kind)
            : soa_detail::ImageShapeKind::Auto;
    }

    std::uint8_t SoaKernel::image_shape_extent(WidgetHandle h) const noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return 0;
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::Image) {
            unsupported_kind(common_.kind[idx]);
            return 0;
        }
        const auto* payload = payload_get<soa_detail::ImagePayload>(idx);
        return payload ? payload->shape_extent : 0;
    }

    void SoaKernel::set_image_rotation_deg(WidgetHandle h, std::int16_t degrees) noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return;
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::Image) {
            unsupported_kind(common_.kind[idx]);
            return;
        }
        auto* payload = payload_get<soa_detail::ImagePayload>(idx);
        if (!payload) return;
        if (payload->rotation_deg != degrees) {
            payload->rotation_deg = degrees;
            mark_paint_dirty();
        }
    }

    std::int16_t SoaKernel::image_rotation_deg(WidgetHandle h) const noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return 0;
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::Image) {
            unsupported_kind(common_.kind[idx]);
            return 0;
        }
        const auto* payload = payload_get<soa_detail::ImagePayload>(idx);
        return payload ? payload->rotation_deg : 0;
    }

    void SoaKernel::set_button_icon(WidgetHandle h, soa_detail::ImageId icon) noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return;
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::Button) {
            unsupported_kind(common_.kind[idx]);
            return;
        }
        auto* payload = payload_get<soa_detail::ButtonPayload>(idx);
        if (!payload) return;
        if (payload->icon != icon) {
            payload->icon = icon;
            mark_paint_dirty();
        }
    }

    void SoaKernel::set_button_icon_size(WidgetHandle h, std::uint8_t size) noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return;
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::Button) {
            unsupported_kind(common_.kind[idx]);
            return;
        }
        auto* payload = payload_get<soa_detail::ButtonPayload>(idx);
        if (!payload) return;
        if (payload->icon_size != size) {
            payload->icon_size = size;
            mark_paint_dirty();
        }
    }

    soa_detail::ImageId SoaKernel::button_icon(WidgetHandle h) const noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return soa_detail::invalid_image_id();
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::Button) {
            unsupported_kind(common_.kind[idx]);
            return soa_detail::invalid_image_id();
        }
        const auto* payload = payload_get<soa_detail::ButtonPayload>(idx);
        return payload ? payload->icon : soa_detail::invalid_image_id();
    }

    std::uint8_t SoaKernel::button_icon_size(WidgetHandle h) const noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return 0;
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::Button) {
            unsupported_kind(common_.kind[idx]);
            return 0;
        }
        const auto* payload = payload_get<soa_detail::ButtonPayload>(idx);
        return payload ? payload->icon_size : 0;
    }

    void SoaKernel::set_spinner_phase(WidgetHandle h, std::uint8_t phase) noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return;
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::Spinner) {
            unsupported_kind(common_.kind[idx]);
            return;
        }
        auto* payload = payload_get<soa_detail::SpinnerPayload>(idx);
        if (!payload) return;
        if (payload->phase != phase) {
            payload->phase = phase;
            mark_paint_dirty();
        }
    }

    std::uint8_t SoaKernel::spinner_phase(WidgetHandle h) const noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return 0;
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::Spinner) {
            unsupported_kind(common_.kind[idx]);
            return 0;
        }
        const auto* payload = payload_get<soa_detail::SpinnerPayload>(idx);
        return payload ? payload->phase : 0;
    }

    void SoaKernel::set_segmented_count(WidgetHandle h, std::uint8_t count) noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return;
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::SegmentedControl) {
            unsupported_kind(common_.kind[idx]);
            return;
        }
        if (count > soa_detail::kMaxSegments) count = soa_detail::kMaxSegments;
        auto* payload = payload_get<soa_detail::SegmentedControlPayload>(idx);
        if (!payload) return;
        if (payload->count != count) {
            payload->count = count;
            if (payload->selected >= count && count > 0) {
                payload->selected = static_cast<std::uint8_t>(count - 1);
            }
            mark_layout_dirty();
            mark_paint_dirty();
        }
    }

    void SoaKernel::set_segmented_label(WidgetHandle h, std::uint8_t index, const char* text) noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return;
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::SegmentedControl) {
            unsupported_kind(common_.kind[idx]);
            return;
        }
        auto* payload = payload_get<soa_detail::SegmentedControlPayload>(idx);
        if (!payload) return;
        if (index >= soa_detail::kMaxSegments) return;
        const auto id = payloads_.store_text(text);
        payload->labels[index] = id;
        if (index >= payload->count) {
            payload->count = static_cast<std::uint8_t>(index + 1);
            if (payload->selected >= payload->count && payload->count > 0) {
                payload->selected = static_cast<std::uint8_t>(payload->count - 1);
            }
        }
        mark_layout_dirty();
        mark_paint_dirty();
    }

    void SoaKernel::set_segmented_selected(WidgetHandle h, std::uint8_t index) noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return;
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::SegmentedControl) {
            unsupported_kind(common_.kind[idx]);
            return;
        }
        auto* payload = payload_get<soa_detail::SegmentedControlPayload>(idx);
        if (!payload) return;
        if (payload->count == 0) return;
        if (index >= payload->count) index = static_cast<std::uint8_t>(payload->count - 1);
        if (payload->selected != index) {
            payload->selected = index;
            mark_paint_dirty();
        }
    }

    std::uint8_t SoaKernel::segmented_count(WidgetHandle h) const noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return 0;
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::SegmentedControl) {
            unsupported_kind(common_.kind[idx]);
            return 0;
        }
        const auto* payload = payload_get<soa_detail::SegmentedControlPayload>(idx);
        return payload ? payload->count : 0;
    }

    std::uint8_t SoaKernel::segmented_selected(WidgetHandle h) const noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return 0;
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::SegmentedControl) {
            unsupported_kind(common_.kind[idx]);
            return 0;
        }
        const auto* payload = payload_get<soa_detail::SegmentedControlPayload>(idx);
        return payload ? payload->selected : 0;
    }

    const char* SoaKernel::segmented_label(WidgetHandle h, std::uint8_t index) const noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return "";
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::SegmentedControl) {
            unsupported_kind(common_.kind[idx]);
            return "";
        }
        const auto* payload = payload_get<soa_detail::SegmentedControlPayload>(idx);
        if (!payload) return "";
        if (index >= payload->count) return "";
        return payloads_.text_c_str(payload->labels[index]);
    }

    void SoaKernel::set_stepper_count(WidgetHandle h, std::uint8_t count) noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return;
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::Stepper) {
            unsupported_kind(common_.kind[idx]);
            return;
        }
        if (count < 1) count = 1;
        if (count > soa_detail::kMaxStepperSteps) count = soa_detail::kMaxStepperSteps;
        auto* payload = payload_get<soa_detail::StepperPayload>(idx);
        if (!payload) return;
        payload->count = count;
        if (payload->current >= count) {
            payload->current = static_cast<std::uint8_t>(count - 1u);
        }
        mark_paint_dirty();
    }

    void SoaKernel::set_stepper_current(WidgetHandle h, std::uint8_t index) noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return;
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::Stepper) {
            unsupported_kind(common_.kind[idx]);
            return;
        }
        auto* payload = payload_get<soa_detail::StepperPayload>(idx);
        if (!payload || payload->count == 0) return;
        if (index >= payload->count) index = static_cast<std::uint8_t>(payload->count - 1u);
        if (payload->current == index) return;
        payload->current = index;
        mark_paint_dirty();
    }

    void SoaKernel::set_stepper_label(WidgetHandle h, std::uint8_t index, const char* text) noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return;
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::Stepper) {
            unsupported_kind(common_.kind[idx]);
            return;
        }
        if (index >= soa_detail::kMaxStepperSteps) return;
        auto* payload = payload_get<soa_detail::StepperPayload>(idx);
        if (!payload) return;
        payload->labels[index] = payloads_.store_text(text);
        if (index >= payload->count) {
            payload->count = static_cast<std::uint8_t>(index + 1u);
        }
        mark_paint_dirty();
    }

    std::uint8_t SoaKernel::stepper_count(WidgetHandle h) const noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return 0;
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::Stepper) {
            unsupported_kind(common_.kind[idx]);
            return 0;
        }
        const auto* payload = payload_get<soa_detail::StepperPayload>(idx);
        return payload ? payload->count : 0;
    }

    std::uint8_t SoaKernel::stepper_current(WidgetHandle h) const noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return 0;
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::Stepper) {
            unsupported_kind(common_.kind[idx]);
            return 0;
        }
        const auto* payload = payload_get<soa_detail::StepperPayload>(idx);
        return payload ? payload->current : 0;
    }

    const char* SoaKernel::stepper_label(WidgetHandle h, std::uint8_t index) const noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return "";
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::Stepper) {
            unsupported_kind(common_.kind[idx]);
            return "";
        }
        const auto* payload = payload_get<soa_detail::StepperPayload>(idx);
        if (!payload) return "";
        if (index >= payload->count) return "";
        return payloads_.text_c_str(payload->labels[index]);
    }

    void SoaKernel::set_toggle_group_kind(WidgetHandle h, WidgetKind group_kind) noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return;
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::ToggleGroup) {
            unsupported_kind(common_.kind[idx]);
            return;
        }
        auto* payload = payload_get<soa_detail::ToggleGroupPayload>(idx);
        if (!payload) return;
        payload->group_kind = group_kind;
    }

    WidgetKind SoaKernel::toggle_group_kind(WidgetHandle h) const noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return WidgetKind::None;
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::ToggleGroup) {
            unsupported_kind(common_.kind[idx]);
            return WidgetKind::None;
        }
        const auto* payload = payload_get<soa_detail::ToggleGroupPayload>(idx);
        return payload ? payload->group_kind : WidgetKind::None;
    }

    void SoaKernel::set_value(WidgetHandle h, int value) noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return;
        const auto desc = payload_descriptor(common_.kind[idx]);
        switch (desc.payload) {
        case soa_detail::PayloadKind::Slider: {
            auto* payload = payload_get<soa_detail::SliderPayload>(idx);
            if (!payload) return;
            if (payload->value != value) {
                payload->value = value;
                mark_paint_dirty();
            }
            break;
        }
        case soa_detail::PayloadKind::ScrollBar: {
            auto* payload = payload_get<soa_detail::ScrollBarPayload>(idx);
            if (!payload) return;
            if (payload->value != value) {
                payload->value = value;
                mark_paint_dirty();
            }
            break;
        }
        case soa_detail::PayloadKind::Progress: {
            auto* payload = payload_get<soa_detail::ProgressPayload>(idx);
            if (!payload) return;
            if (payload->value != value) {
                payload->value = value;
                mark_paint_dirty();
            }
            break;
        }
        default:
            unsupported_kind(common_.kind[idx]);
            break;
        }
    }

    int SoaKernel::value(WidgetHandle h) const noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return 0;
        const auto desc = payload_descriptor(common_.kind[idx]);
        switch (desc.payload) {
        case soa_detail::PayloadKind::Slider: {
            const auto* payload = payload_get<soa_detail::SliderPayload>(idx);
            return payload ? payload->value : 0;
        }
        case soa_detail::PayloadKind::ScrollBar: {
            const auto* payload = payload_get<soa_detail::ScrollBarPayload>(idx);
            return payload ? payload->value : 0;
        }
        case soa_detail::PayloadKind::Progress: {
            const auto* payload = payload_get<soa_detail::ProgressPayload>(idx);
            return payload ? payload->value : 0;
        }
        default:
            unsupported_kind(common_.kind[idx]);
            return 0;
        }
        return 0;
    }

    void SoaKernel::set_range(WidgetHandle h, int min_value, int max_value) noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return;
        const auto desc = payload_descriptor(common_.kind[idx]);
        switch (desc.payload) {
        case soa_detail::PayloadKind::Slider: {
            auto* payload = payload_get<soa_detail::SliderPayload>(idx);
            if (!payload) return;
            payload->min_value = min_value;
            payload->max_value = max_value;
            if (payload->value < min_value) payload->value = min_value;
            if (payload->value > max_value) payload->value = max_value;
            mark_layout_dirty();
            break;
        }
        case soa_detail::PayloadKind::ScrollBar: {
            auto* payload = payload_get<soa_detail::ScrollBarPayload>(idx);
            if (!payload) return;
            payload->min_value = min_value;
            payload->max_value = max_value;
            if (payload->value < min_value) payload->value = min_value;
            if (payload->value > max_value) payload->value = max_value;
            mark_layout_dirty();
            break;
        }
        case soa_detail::PayloadKind::Progress: {
            auto* payload = payload_get<soa_detail::ProgressPayload>(idx);
            if (!payload) return;
            payload->min_value = min_value;
            payload->max_value = max_value;
            if (payload->value < min_value) payload->value = min_value;
            if (payload->value > max_value) payload->value = max_value;
            mark_layout_dirty();
            break;
        }
        default:
            unsupported_kind(common_.kind[idx]);
            break;
        }
    }

    int SoaKernel::min_value(WidgetHandle h) const noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return 0;
        const auto desc = payload_descriptor(common_.kind[idx]);
        switch (desc.payload) {
        case soa_detail::PayloadKind::Slider: {
            const auto* payload = payload_get<soa_detail::SliderPayload>(idx);
            return payload ? payload->min_value : 0;
        }
        case soa_detail::PayloadKind::ScrollBar: {
            const auto* payload = payload_get<soa_detail::ScrollBarPayload>(idx);
            return payload ? payload->min_value : 0;
        }
        case soa_detail::PayloadKind::Progress: {
            const auto* payload = payload_get<soa_detail::ProgressPayload>(idx);
            return payload ? payload->min_value : 0;
        }
        default:
            unsupported_kind(common_.kind[idx]);
            return 0;
        }
        return 0;
    }

    int SoaKernel::max_value(WidgetHandle h) const noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return 0;
        const auto desc = payload_descriptor(common_.kind[idx]);
        switch (desc.payload) {
        case soa_detail::PayloadKind::Slider: {
            const auto* payload = payload_get<soa_detail::SliderPayload>(idx);
            return payload ? payload->max_value : 0;
        }
        case soa_detail::PayloadKind::ScrollBar: {
            const auto* payload = payload_get<soa_detail::ScrollBarPayload>(idx);
            return payload ? payload->max_value : 0;
        }
        case soa_detail::PayloadKind::Progress: {
            const auto* payload = payload_get<soa_detail::ProgressPayload>(idx);
            return payload ? payload->max_value : 0;
        }
        default:
            unsupported_kind(common_.kind[idx]);
            return 0;
        }
        return 0;
    }

    void SoaKernel::set_checked(WidgetHandle h, bool on) noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return;
        const auto desc = payload_descriptor(common_.kind[idx]);
        const std::uint8_t value = static_cast<std::uint8_t>(on ? 1 : 0);
        switch (desc.payload) {
        case soa_detail::PayloadKind::Switch: {
            auto* payload = payload_get<soa_detail::SwitchPayload>(idx);
            if (!payload) return;
            payload->checked = value;
            break;
        }
        case soa_detail::PayloadKind::Checkbox: {
            auto* payload = payload_get<soa_detail::CheckboxPayload>(idx);
            if (!payload) return;
            payload->checked = value;
            break;
        }
        case soa_detail::PayloadKind::Radio: {
            auto* payload = payload_get<soa_detail::RadioPayload>(idx);
            if (!payload) return;
            payload->checked = value;
            break;
        }
        case soa_detail::PayloadKind::ListItem: {
            auto* payload = payload_get<soa_detail::ListItemPayload>(idx);
            if (!payload) return;
            payload->checked = value;
            break;
        }
        case soa_detail::PayloadKind::TextList: {
            auto* payload = payload_get<soa_detail::TextListPayload>(idx);
            if (!payload) return;
            if (on) {
                if (payload->count > 0 && payload->selected < 0) {
                    payload->selected = 0;
                }
            } else {
                payload->selected = -1;
            }
            break;
        }
        default:
            unsupported_kind(common_.kind[idx]);
            break;
        }
        mark_paint_dirty();
    }

    bool SoaKernel::checked(WidgetHandle h) const noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return false;
        const auto desc = payload_descriptor(common_.kind[idx]);
        switch (desc.payload) {
        case soa_detail::PayloadKind::Switch: {
            const auto* payload = payload_get<soa_detail::SwitchPayload>(idx);
            return payload ? payload->checked != 0 : false;
        }
        case soa_detail::PayloadKind::Checkbox: {
            const auto* payload = payload_get<soa_detail::CheckboxPayload>(idx);
            return payload ? payload->checked != 0 : false;
        }
        case soa_detail::PayloadKind::Radio: {
            const auto* payload = payload_get<soa_detail::RadioPayload>(idx);
            return payload ? payload->checked != 0 : false;
        }
        case soa_detail::PayloadKind::ListItem: {
            const auto* payload = payload_get<soa_detail::ListItemPayload>(idx);
            return payload ? payload->checked != 0 : false;
        }
        case soa_detail::PayloadKind::TextList: {
            const auto* payload = payload_get<soa_detail::TextListPayload>(idx);
            return payload ? payload->selected >= 0 : false;
        }
        default:
            unsupported_kind(common_.kind[idx]);
            return false;
        }
        return false;
    }

    void SoaKernel::set_scroll_y(WidgetHandle h, int y) noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return;
        const auto desc = payload_descriptor(common_.kind[idx]);
        switch (desc.payload) {
        case soa_detail::PayloadKind::List: {
            auto* payload = payload_get<soa_detail::ListPayload>(idx);
            if (!payload) return;
            if (payload->scroll_y != y) {
                payload->scroll_y = y;
                mark_paint_dirty();
            }
            break;
        }
        case soa_detail::PayloadKind::TextList: {
            auto* payload = payload_get<soa_detail::TextListPayload>(idx);
            if (!payload) return;
            if (payload->scroll_y != y) {
                payload->scroll_y = y;
                mark_paint_dirty();
            }
            break;
        }
        case soa_detail::PayloadKind::ListView: {
            auto* payload = payload_get<soa_detail::ListViewPayload>(idx);
            if (!payload) return;
            if (payload->scroll_y != y) {
                payload->scroll_y = y;
                mark_paint_dirty();
            }
            break;
        }
        case soa_detail::PayloadKind::TableView: {
            auto* payload = payload_get<soa_detail::TableViewPayload>(idx);
            if (!payload) return;
            if (payload->scroll_y != y) {
                payload->scroll_y = y;
                mark_paint_dirty();
            }
            break;
        }
        case soa_detail::PayloadKind::TreeView: {
            auto* payload = payload_get<soa_detail::TreeViewPayload>(idx);
            if (!payload) return;
            if (payload->scroll_y != y) {
                payload->scroll_y = y;
                mark_paint_dirty();
            }
            break;
        }
        case soa_detail::PayloadKind::NumberList: {
            auto* payload = payload_get<soa_detail::NumberListPayload>(idx);
            if (!payload) return;
            if (payload->scroll_y != y) {
                payload->scroll_y = y;
                if (payload->row_height > 0 && payload->count > 0) {
                    int next = (y + payload->row_height / 2) / payload->row_height;
                    if (next < 0) next = 0;
                    if (next >= payload->count) next = payload->count - 1;
                    payload->selected = next;
                }
                mark_paint_dirty();
            }
            break;
        }
        case soa_detail::PayloadKind::Roller: {
            auto* payload = payload_get<soa_detail::RollerPayload>(idx);
            if (!payload) return;
            if (payload->scroll_y != y) {
                payload->scroll_y = y;
                if (payload->row_height > 0 && payload->count > 0) {
                    int next = (y + payload->row_height / 2) / payload->row_height;
                    next %= payload->count;
                    if (next < 0) next += payload->count;
                    payload->selected = next;
                }
                mark_paint_dirty();
            }
            break;
        }
        case soa_detail::PayloadKind::ScrollContainer: {
            auto* payload = payload_get<soa_detail::ScrollContainerPayload>(idx);
            if (!payload) return;
            if (payload->scroll_y != y) {
                payload->scroll_y = y;
                mark_paint_dirty();
            }
            break;
        }
        default:
            unsupported_kind(common_.kind[idx]);
            break;
        }
    }

    void SoaKernel::add_scroll_y(WidgetHandle h, int dy) noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return;
        const auto desc = payload_descriptor(common_.kind[idx]);
        switch (desc.payload) {
        case soa_detail::PayloadKind::List: {
            auto* payload = payload_get<soa_detail::ListPayload>(idx);
            if (!payload) return;
            payload->scroll_y += dy;
            break;
        }
        case soa_detail::PayloadKind::TextList: {
            auto* payload = payload_get<soa_detail::TextListPayload>(idx);
            if (!payload) return;
            payload->scroll_y += dy;
            break;
        }
        case soa_detail::PayloadKind::ListView: {
            auto* payload = payload_get<soa_detail::ListViewPayload>(idx);
            if (!payload) return;
            payload->scroll_y += dy;
            break;
        }
        case soa_detail::PayloadKind::TableView: {
            auto* payload = payload_get<soa_detail::TableViewPayload>(idx);
            if (!payload) return;
            payload->scroll_y += dy;
            break;
        }
        case soa_detail::PayloadKind::TreeView: {
            auto* payload = payload_get<soa_detail::TreeViewPayload>(idx);
            if (!payload) return;
            payload->scroll_y += dy;
            break;
        }
        case soa_detail::PayloadKind::NumberList: {
            auto* payload = payload_get<soa_detail::NumberListPayload>(idx);
            if (!payload) return;
            payload->scroll_y += dy;
            break;
        }
        case soa_detail::PayloadKind::Roller: {
            auto* payload = payload_get<soa_detail::RollerPayload>(idx);
            if (!payload) return;
            payload->scroll_y += dy;
            break;
        }
        case soa_detail::PayloadKind::ScrollContainer: {
            auto* payload = payload_get<soa_detail::ScrollContainerPayload>(idx);
            if (!payload) return;
            payload->scroll_y += dy;
            break;
        }
        default:
            unsupported_kind(common_.kind[idx]);
            break;
        }
    }

    int SoaKernel::scroll_y(WidgetHandle h) const noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return 0;
        const auto desc = payload_descriptor(common_.kind[idx]);
        switch (desc.payload) {
        case soa_detail::PayloadKind::List: {
            const auto* payload = payload_get<soa_detail::ListPayload>(idx);
            return payload ? payload->scroll_y : 0;
        }
        case soa_detail::PayloadKind::TextList: {
            const auto* payload = payload_get<soa_detail::TextListPayload>(idx);
            return payload ? payload->scroll_y : 0;
        }
        case soa_detail::PayloadKind::ListView: {
            const auto* payload = payload_get<soa_detail::ListViewPayload>(idx);
            return payload ? payload->scroll_y : 0;
        }
        case soa_detail::PayloadKind::TableView: {
            const auto* payload = payload_get<soa_detail::TableViewPayload>(idx);
            return payload ? payload->scroll_y : 0;
        }
        case soa_detail::PayloadKind::TreeView: {
            const auto* payload = payload_get<soa_detail::TreeViewPayload>(idx);
            return payload ? payload->scroll_y : 0;
        }
        case soa_detail::PayloadKind::NumberList: {
            const auto* payload = payload_get<soa_detail::NumberListPayload>(idx);
            return payload ? payload->scroll_y : 0;
        }
        case soa_detail::PayloadKind::Roller: {
            const auto* payload = payload_get<soa_detail::RollerPayload>(idx);
            return payload ? payload->scroll_y : 0;
        }
        case soa_detail::PayloadKind::ScrollContainer: {
            const auto* payload = payload_get<soa_detail::ScrollContainerPayload>(idx);
            return payload ? payload->scroll_y : 0;
        }
        default:
            unsupported_kind(common_.kind[idx]);
            return 0;
        }
        return 0;
    }

    void SoaKernel::set_scroll_step(WidgetHandle h, int step) noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return;
        const auto desc = payload_descriptor(common_.kind[idx]);
        const int value = (step > 0) ? step : 1;
        switch (desc.payload) {
        case soa_detail::PayloadKind::List: {
            auto* payload = payload_get<soa_detail::ListPayload>(idx);
            if (!payload) return;
            payload->scroll_step = value;
            break;
        }
        case soa_detail::PayloadKind::TextList: {
            auto* payload = payload_get<soa_detail::TextListPayload>(idx);
            if (!payload) return;
            payload->wheel_step = value;
            break;
        }
        case soa_detail::PayloadKind::ListView: {
            auto* payload = payload_get<soa_detail::ListViewPayload>(idx);
            if (!payload) return;
            payload->wheel_step = value;
            break;
        }
        case soa_detail::PayloadKind::TableView: {
            auto* payload = payload_get<soa_detail::TableViewPayload>(idx);
            if (!payload) return;
            payload->wheel_step = value;
            break;
        }
        case soa_detail::PayloadKind::TreeView: {
            auto* payload = payload_get<soa_detail::TreeViewPayload>(idx);
            if (!payload) return;
            payload->wheel_step = value;
            break;
        }
        case soa_detail::PayloadKind::NumberList: {
            auto* payload = payload_get<soa_detail::NumberListPayload>(idx);
            if (!payload) return;
            payload->wheel_step = value;
            break;
        }
        case soa_detail::PayloadKind::Roller: {
            auto* payload = payload_get<soa_detail::RollerPayload>(idx);
            if (!payload) return;
            payload->wheel_step = value;
            break;
        }
        case soa_detail::PayloadKind::ScrollContainer: {
            auto* payload = payload_get<soa_detail::ScrollContainerPayload>(idx);
            if (!payload) return;
            payload->scroll_step = value;
            break;
        }
        default:
            unsupported_kind(common_.kind[idx]);
            break;
        }
    }

    int SoaKernel::scroll_step(WidgetHandle h) const noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return 24;
        const auto desc = payload_descriptor(common_.kind[idx]);
        switch (desc.payload) {
        case soa_detail::PayloadKind::List: {
            const auto* payload = payload_get<soa_detail::ListPayload>(idx);
            return payload ? payload->scroll_step : 24;
        }
        case soa_detail::PayloadKind::TextList: {
            const auto* payload = payload_get<soa_detail::TextListPayload>(idx);
            return payload ? payload->wheel_step : 24;
        }
        case soa_detail::PayloadKind::ListView: {
            const auto* payload = payload_get<soa_detail::ListViewPayload>(idx);
            return payload ? payload->wheel_step : 24;
        }
        case soa_detail::PayloadKind::TableView: {
            const auto* payload = payload_get<soa_detail::TableViewPayload>(idx);
            return payload ? payload->wheel_step : 24;
        }
        case soa_detail::PayloadKind::TreeView: {
            const auto* payload = payload_get<soa_detail::TreeViewPayload>(idx);
            return payload ? payload->wheel_step : 24;
        }
        case soa_detail::PayloadKind::NumberList: {
            const auto* payload = payload_get<soa_detail::NumberListPayload>(idx);
            return payload ? payload->wheel_step : 24;
        }
        case soa_detail::PayloadKind::Roller: {
            const auto* payload = payload_get<soa_detail::RollerPayload>(idx);
            return payload ? payload->wheel_step : 24;
        }
        case soa_detail::PayloadKind::ScrollContainer: {
            const auto* payload = payload_get<soa_detail::ScrollContainerPayload>(idx);
            return payload ? payload->scroll_step : 24;
        }
        default:
            unsupported_kind(common_.kind[idx]);
            return 24;
        }
        return 24;
    }

    void SoaKernel::set_list_row_height(WidgetHandle h, int row_h) noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return;
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload == soa_detail::PayloadKind::List) {
            auto* payload = payload_get<soa_detail::ListPayload>(idx);
            if (!payload) return;
            payload->row_height = (row_h > 0) ? row_h : 1;
            mark_layout_dirty();
            return;
        }
        if (desc.payload == soa_detail::PayloadKind::TextList) {
            auto* payload = payload_get<soa_detail::TextListPayload>(idx);
            if (!payload) return;
            payload->row_height = (row_h > 0) ? row_h : 1;
            mark_layout_dirty();
            return;
        }
        if (desc.payload == soa_detail::PayloadKind::ListView) {
            auto* payload = payload_get<soa_detail::ListViewPayload>(idx);
            if (!payload) return;
            payload->row_height = (row_h > 0) ? row_h : 1;
            mark_layout_dirty();
            return;
        }
        if (desc.payload == soa_detail::PayloadKind::TableView) {
            auto* payload = payload_get<soa_detail::TableViewPayload>(idx);
            if (!payload) return;
            payload->row_height = (row_h > 0) ? row_h : 1;
            mark_layout_dirty();
            return;
        }
        if (desc.payload == soa_detail::PayloadKind::TreeView) {
            auto* payload = payload_get<soa_detail::TreeViewPayload>(idx);
            if (!payload) return;
            payload->row_height = (row_h > 0) ? row_h : 1;
            mark_layout_dirty();
            return;
        }
        unsupported_kind(common_.kind[idx]);
    }

    int SoaKernel::list_row_height(WidgetHandle h) const noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return 28;
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload == soa_detail::PayloadKind::List) {
            const auto* payload = payload_get<soa_detail::ListPayload>(idx);
            return payload ? payload->row_height : 28;
        }
        if (desc.payload == soa_detail::PayloadKind::TextList) {
            const auto* payload = payload_get<soa_detail::TextListPayload>(idx);
            return payload ? payload->row_height : 28;
        }
        if (desc.payload == soa_detail::PayloadKind::ListView) {
            const auto* payload = payload_get<soa_detail::ListViewPayload>(idx);
            return payload ? payload->row_height : 28;
        }
        if (desc.payload == soa_detail::PayloadKind::TableView) {
            const auto* payload = payload_get<soa_detail::TableViewPayload>(idx);
            return payload ? payload->row_height : 28;
        }
        if (desc.payload == soa_detail::PayloadKind::TreeView) {
            const auto* payload = payload_get<soa_detail::TreeViewPayload>(idx);
            return payload ? payload->row_height : 28;
        }
        unsupported_kind(common_.kind[idx]);
        return 28;
    }

    void SoaKernel::apply_list_layout(WidgetHandle h, int padding) noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return;
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::List) {
            unsupported_kind(common_.kind[idx]);
            return;
        }
        const auto* payload = payload_get<soa_detail::ListPayload>(idx);
        if (!payload) return;
        const int row_h = payload->row_height;
        Rect r = common_.rects[idx];
        int x = padding;
        int y = padding;
        int w = r.w - padding * 2;
        if (w < 0) w = 0;
        std::uint16_t child = common_.first_child[idx];
        while (child != kInvalidIndex) {
            common_.rects[child] = Rect{x, y, w, row_h};
            y += row_h;
            child = common_.next_sibling[child];
        }
    }

    bool SoaKernel::payload_overflowed() const noexcept {
        return payloads_.overflowed();
    }

    bool SoaKernel::text_overflowed() const noexcept {
        return payloads_.text_overflowed();
    }

#if defined(VIVID_SOA_TRACE_INPUT)
    soa_detail::PayloadStats SoaKernel::payload_stats() const noexcept {
        return payloads_.stats();
    }
#endif

