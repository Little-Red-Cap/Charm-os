module;

#include <cstdint>

module charm.core.soa_kernel:layout_state;

import :kernel_class;
import :types;

    void SoaKernel::mark_layout_dirty() noexcept {
        layout_dirty_version_ += 1u;
#if defined(VIVID_SOA_TRACE_INPUT)
        layout_invalidated_count_ += 1u;
#endif
    }

    void SoaKernel::mark_paint_dirty() noexcept {
        paint_dirty_version_ += 1u;
#if defined(VIVID_SOA_TRACE_INPUT)
        paint_invalidated_count_ += 1u;
#endif
    }

    void SoaKernel::set_layout_state_influence(bool on) noexcept {
        layout_state_influence_ = on;
        mark_layout_dirty();
    }

    bool SoaKernel::layout_state_influence() const noexcept {
        return layout_state_influence_;
    }

    std::uint8_t SoaKernel::layout_state_influence_mask(WidgetKind kind) const noexcept {
        return layout_state_mask_for_kind(kind);
    }

    std::uint32_t SoaKernel::layout_dirty_version() const noexcept {
        return layout_dirty_version_;
    }

    std::uint32_t SoaKernel::paint_dirty_version() const noexcept {
        return paint_dirty_version_;
    }

    std::uint32_t SoaKernel::layout_applied_version() const noexcept {
        return layout_applied_version_;
    }

    void SoaKernel::set_layout_applied_version(std::uint32_t v) noexcept {
        layout_applied_version_ = v;
    }

#if defined(VIVID_SOA_TRACE_INPUT)
    void SoaKernel::layout_trace_reset() noexcept {
        layout_invalidated_count_ = 0;
        layout_pass_count_ = 0;
        paint_invalidated_count_ = 0;
    }

    std::uint32_t SoaKernel::layout_invalidated_count() const noexcept {
        return layout_invalidated_count_;
    }

    std::uint32_t SoaKernel::layout_pass_count() const noexcept {
        return layout_pass_count_;
    }

    std::uint32_t SoaKernel::paint_invalidated_count() const noexcept {
        return paint_invalidated_count_;
    }

    void SoaKernel::layout_trace_on_pass() noexcept {
        layout_pass_count_ += 1u;
    }
#endif

    void SoaKernel::on_state_change(std::uint16_t idx, SoaStateMask bit) noexcept {
        if (!layout_state_influence_) {
            mark_paint_dirty();
            return;
        }
        const std::uint8_t mask = layout_state_mask_for_kind(common_.kind[idx]);
        if ((mask & static_cast<std::uint8_t>(bit)) != 0) {
            mark_layout_dirty();
            return;
        }
        mark_paint_dirty();
    }

    constexpr std::uint8_t SoaKernel::layout_state_mask_for_kind(WidgetKind kind) noexcept {
        switch (kind) {
        case WidgetKind::Container:
        case WidgetKind::ScrollContainer:
        case WidgetKind::Label:
        case WidgetKind::Button:
        case WidgetKind::Switch:
        case WidgetKind::Slider:
        case WidgetKind::Progress:
        case WidgetKind::Checkbox:
        case WidgetKind::Radio:
        case WidgetKind::List:
        case WidgetKind::ListItem:
            return 0;
        default:
            return 0;
        }
    }
