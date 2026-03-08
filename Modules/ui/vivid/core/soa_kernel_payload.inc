    void set_text(WidgetHandle h, const char* text) noexcept {
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

    const char* text(WidgetHandle h) const noexcept {
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

    void set_image(WidgetHandle h, soa_detail::ImageId image) noexcept {
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

    soa_detail::ImageId image(WidgetHandle h) const noexcept {
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

    void set_button_icon(WidgetHandle h, soa_detail::ImageId icon) noexcept {
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

    void set_button_icon_size(WidgetHandle h, std::uint8_t size) noexcept {
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

    soa_detail::ImageId button_icon(WidgetHandle h) const noexcept {
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

    std::uint8_t button_icon_size(WidgetHandle h) const noexcept {
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

    void set_spinner_phase(WidgetHandle h, std::uint8_t phase) noexcept {
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

    std::uint8_t spinner_phase(WidgetHandle h) const noexcept {
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

    void set_segmented_count(WidgetHandle h, std::uint8_t count) noexcept {
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

    void set_segmented_label(WidgetHandle h, std::uint8_t index, const char* text) noexcept {
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

    void set_segmented_selected(WidgetHandle h, std::uint8_t index) noexcept {
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

    std::uint8_t segmented_count(WidgetHandle h) const noexcept {
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

    std::uint8_t segmented_selected(WidgetHandle h) const noexcept {
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

    const char* segmented_label(WidgetHandle h, std::uint8_t index) const noexcept {
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

    void set_stepper_count(WidgetHandle h, std::uint8_t count) noexcept {
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

    void set_stepper_current(WidgetHandle h, std::uint8_t index) noexcept {
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

    void set_stepper_label(WidgetHandle h, std::uint8_t index, const char* text) noexcept {
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

    std::uint8_t stepper_count(WidgetHandle h) const noexcept {
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

    std::uint8_t stepper_current(WidgetHandle h) const noexcept {
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

    const char* stepper_label(WidgetHandle h, std::uint8_t index) const noexcept {
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

    void set_text_list_count(WidgetHandle h, std::uint16_t count) noexcept {
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

    void set_text_list_item(WidgetHandle h, std::uint16_t index, const char* text) noexcept {
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

    std::uint16_t text_list_count(WidgetHandle h) const noexcept {
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

    int text_list_selected(WidgetHandle h) const noexcept {
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

    void set_text_list_selected(WidgetHandle h, int index) noexcept {
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

    const char* text_list_item(WidgetHandle h, std::uint16_t index) const noexcept {
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

    void set_number_list_count(WidgetHandle h, std::uint16_t count) noexcept {
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

    void set_number_list_range(WidgetHandle h, int start, int delta) noexcept {
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

    void set_number_list_selected(WidgetHandle h, int index) noexcept {
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

    std::uint16_t number_list_count(WidgetHandle h) const noexcept {
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

    int number_list_selected(WidgetHandle h) const noexcept {
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

    int number_list_value(WidgetHandle h, int index) const noexcept {
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

    void set_number_list_row_height(WidgetHandle h, int row_h) noexcept {
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

    int number_list_row_height(WidgetHandle h) const noexcept {
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

    void set_number_list_wheel_step(WidgetHandle h, int step) noexcept {
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

    int number_list_wheel_step(WidgetHandle h) const noexcept {
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

    void set_roller_source(WidgetHandle h, std::uint16_t count,
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

    void set_roller_selected(WidgetHandle h, int index) noexcept {
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

    std::uint16_t roller_count(WidgetHandle h) const noexcept {
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

    int roller_selected(WidgetHandle h) const noexcept {
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

    const char* roller_item_text(WidgetHandle h, std::uint16_t index) const noexcept {
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

    void set_roller_row_height(WidgetHandle h, int row_h) noexcept {
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

    int roller_row_height(WidgetHandle h) const noexcept {
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

    void set_roller_wheel_step(WidgetHandle h, int step) noexcept {
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

    int roller_wheel_step(WidgetHandle h) const noexcept {
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

    void console_clear(WidgetHandle h) noexcept {
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

    void set_console_follow_tail(WidgetHandle h, bool follow) noexcept {
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

    void console_append(WidgetHandle h, const char* text) noexcept {
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

    void set_list_view_source(WidgetHandle h, std::uint16_t count, const void* ctx,
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
        mark_layout_dirty();
    }

    void set_list_view_icon_source(WidgetHandle h,
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

    void set_list_view_count(WidgetHandle h, std::uint16_t count) noexcept {
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
        mark_layout_dirty();
    }

    std::uint16_t list_view_count(WidgetHandle h) const noexcept {
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

    int list_view_selected(WidgetHandle h) const noexcept {
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

    void set_list_view_selected(WidgetHandle h, int index) noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return;
        const auto desc = payload_descriptor(common_.kind[idx]);
        if (desc.payload != soa_detail::PayloadKind::ListView) {
            unsupported_kind(common_.kind[idx]);
            return;
        }
        auto* payload = payload_get<soa_detail::ListViewPayload>(idx);
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

    const char* list_view_item_text(WidgetHandle h, std::uint16_t index) const noexcept {
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

    soa_detail::ImageId list_view_item_icon(WidgetHandle h, std::uint16_t index) const noexcept {
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

    std::uint8_t list_view_icon_size(WidgetHandle h) const noexcept {
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

    std::uint8_t list_view_overscan(WidgetHandle h) const noexcept {
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

    void set_table_view_source(WidgetHandle h, std::uint16_t rows, std::uint8_t cols,
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

    void set_table_view_header(WidgetHandle h, const void* ctx,
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

    void set_table_view_header_height(WidgetHandle h, int height) noexcept {
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

    void set_table_view_header_padding(WidgetHandle h, int padding) noexcept {
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

    void set_table_view_header_style(WidgetHandle h, TableViewHeaderStyle style) noexcept {
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

    void set_table_view_header_divider(WidgetHandle h, bool enabled) noexcept {
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

    void set_table_view_col_dividers(WidgetHandle h, bool enabled) noexcept {
        set_table_view_col_divider_style(
            h,
            enabled ? TableViewColDividerStyle::Full : TableViewColDividerStyle::None);
    }

    void set_table_view_col_divider_style(WidgetHandle h, TableViewColDividerStyle style) noexcept {
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

    void set_table_view_count(WidgetHandle h, std::uint16_t rows) noexcept {
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

    std::uint16_t table_view_row_count(WidgetHandle h) const noexcept {
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

    bool table_view_has_header(WidgetHandle h) const noexcept {
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

    int table_view_header_height(WidgetHandle h) const noexcept {
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

    int table_view_header_padding(WidgetHandle h) const noexcept {
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

    TableViewHeaderStyle table_view_header_style(WidgetHandle h) const noexcept {
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

    bool table_view_header_divider(WidgetHandle h) const noexcept {
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

    bool table_view_col_dividers(WidgetHandle h) const noexcept {
        return table_view_col_divider_style(h) != TableViewColDividerStyle::None;
    }

    TableViewColDividerStyle table_view_col_divider_style(WidgetHandle h) const noexcept {
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

    std::uint8_t table_view_col_count(WidgetHandle h) const noexcept {
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

    const char* table_view_header_text(WidgetHandle h, std::uint8_t col) const noexcept {
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

    bool table_view_has_col_width_fn(WidgetHandle h) const noexcept {
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

    int table_view_col_width(WidgetHandle h) const noexcept {
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

    int table_view_col_width_at(WidgetHandle h, std::uint8_t col) const noexcept {
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

    int table_view_scroll_x(WidgetHandle h) const noexcept {
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

    void set_table_view_col_width(WidgetHandle h, int col_width) noexcept {
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

    void set_table_view_col_width_fn(WidgetHandle h, const void* ctx,
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

    void set_table_view_scroll_x(WidgetHandle h, int x) noexcept {
        set_table_view_scroll_x_clamped(h, x);
    }

    std::uint8_t table_view_overscan(WidgetHandle h) const noexcept {
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

    const char* table_view_cell_text(WidgetHandle h, std::uint16_t row, std::uint8_t col) const noexcept {
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

    void set_tree_view_source(WidgetHandle h, std::uint16_t count,
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

    void set_tree_view_count(WidgetHandle h, std::uint16_t count) noexcept {
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

    std::uint16_t tree_view_count(WidgetHandle h) const noexcept {
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

    std::uint8_t tree_view_overscan(WidgetHandle h) const noexcept {
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

    std::uint8_t tree_view_indent_px(WidgetHandle h) const noexcept {
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

    int tree_view_max_indent_px(WidgetHandle h) const noexcept {
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

    int tree_view_min_text_avail_px(WidgetHandle h) const noexcept {
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

    void set_tree_view_indent_px(WidgetHandle h, std::uint8_t px) noexcept {
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

    void set_tree_view_max_indent_px(WidgetHandle h, int px) noexcept {
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

    void set_tree_view_min_text_avail_px(WidgetHandle h, int px) noexcept {
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

    const char* tree_view_item_text(WidgetHandle h, std::uint16_t index) const noexcept {
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

    std::uint8_t tree_view_item_indent(WidgetHandle h, std::uint16_t index) const noexcept {
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

    void set_toggle_group_kind(WidgetHandle h, WidgetKind group_kind) noexcept {
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

    WidgetKind toggle_group_kind(WidgetHandle h) const noexcept {
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

    void set_value(WidgetHandle h, int value) noexcept {
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

    int value(WidgetHandle h) const noexcept {
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

    void set_range(WidgetHandle h, int min_value, int max_value) noexcept {
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

    int min_value(WidgetHandle h) const noexcept {
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

    int max_value(WidgetHandle h) const noexcept {
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

    void set_scrollbar_orientation(WidgetHandle h, ScrollBarOrientation orient) noexcept {
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

    ScrollBarOrientation scrollbar_orientation(WidgetHandle h) const noexcept {
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

    void set_scrollbar_page_size(WidgetHandle h, int page_size) noexcept {
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

    int scrollbar_page_size(WidgetHandle h) const noexcept {
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

    void set_scrollbar_target(WidgetHandle h, WidgetHandle target) noexcept {
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

    WidgetHandle scrollbar_target(WidgetHandle h) const noexcept {
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

    void set_checked(WidgetHandle h, bool on) noexcept {
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

    bool checked(WidgetHandle h) const noexcept {
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

    void set_scroll_y(WidgetHandle h, int y) noexcept {
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

    void add_scroll_y(WidgetHandle h, int dy) noexcept {
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

    int scroll_y(WidgetHandle h) const noexcept {
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

    void set_scroll_step(WidgetHandle h, int step) noexcept {
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

    int scroll_step(WidgetHandle h) const noexcept {
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

    void set_list_row_height(WidgetHandle h, int row_h) noexcept {
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

    int list_row_height(WidgetHandle h) const noexcept {
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

    void apply_list_layout(WidgetHandle h, int padding) noexcept {
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

    void set_layout_kind(WidgetHandle h, SoaLayoutKind kind) noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return;
        common_.layout_kind[idx] = static_cast<std::uint8_t>(kind);
        mark_layout_dirty();
    }

    SoaLayoutKind layout_kind(WidgetHandle h) const noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return SoaLayoutKind::None;
        return static_cast<SoaLayoutKind>(common_.layout_kind[idx]);
    }

    void set_layout_state_influence(bool on) noexcept {
        layout_state_influence_ = on;
        mark_layout_dirty();
    }

    bool layout_state_influence() const noexcept {
        return layout_state_influence_;
    }

    std::uint8_t layout_state_influence_mask(WidgetKind kind) const noexcept {
        return layout_state_mask_for_kind(kind);
    }

    std::uint32_t layout_dirty_version() const noexcept {
        return layout_dirty_version_;
    }

    std::uint32_t paint_dirty_version() const noexcept {
        return paint_dirty_version_;
    }

    bool payload_overflowed() const noexcept {
        return payloads_.overflowed();
    }

    bool text_overflowed() const noexcept {
        return payloads_.text_overflowed();
    }

#if defined(VIVID_SOA_TRACE_INPUT)
    soa_detail::PayloadStats payload_stats() const noexcept {
        return payloads_.stats();
    }
#endif

    std::uint32_t layout_applied_version() const noexcept {
        return layout_applied_version_;
    }

    void set_layout_applied_version(std::uint32_t v) noexcept {
        layout_applied_version_ = v;
    }

#if defined(VIVID_SOA_TRACE_INPUT)
    void layout_trace_reset() noexcept {
        layout_invalidated_count_ = 0;
        layout_pass_count_ = 0;
        paint_invalidated_count_ = 0;
    }

    std::uint32_t layout_invalidated_count() const noexcept {
        return layout_invalidated_count_;
    }

    std::uint32_t layout_pass_count() const noexcept {
        return layout_pass_count_;
    }

    std::uint32_t paint_invalidated_count() const noexcept {
        return paint_invalidated_count_;
    }

    void layout_trace_on_pass() noexcept {
        layout_pass_count_ += 1u;
    }
#endif

    int compute_content_height(WidgetHandle h) const noexcept {
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

    int max_scroll(WidgetHandle h) const noexcept {
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

    int clamp_scroll_y(WidgetHandle h, int y) const noexcept {
        const int max_scroll_value = max_scroll(h);
        if (y < 0) return 0;
        if (y > max_scroll_value) return max_scroll_value;
        return y;
    }

    int table_view_content_width(WidgetHandle h) const noexcept {
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

    int max_scroll_x(WidgetHandle h) const noexcept {
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

    int clamp_scroll_x(WidgetHandle h, int x) const noexcept {
        const int max_scroll_value = max_scroll_x(h);
        if (x < 0) return 0;
        if (x > max_scroll_value) return max_scroll_value;
        return x;
    }

    void set_table_view_scroll_x_clamped(WidgetHandle h, int x) noexcept {
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

    void set_scroll_y_clamped(WidgetHandle h, int y) noexcept {
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

