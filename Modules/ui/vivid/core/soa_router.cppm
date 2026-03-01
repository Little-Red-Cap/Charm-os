module;
#include <cstddef>

export module charm.core.soa_router;

export import charm.core.soa_kernel;
export import charm.core.soa_layout;
export import charm.core.event;

export
class SoaRouter {
public:
    SoaRouter(SoaKernel& kernel, SoaLayoutPass& layout, WidgetHandle root) noexcept
        : kernel_(kernel), layout_(layout) {
        kernel_.set_input_root(root);
    }

    void set_root(WidgetHandle root) noexcept { kernel_.set_input_root(root); }
    WidgetHandle root() const noexcept { return kernel_.input_root(); }

    WidgetHandle hovered() const noexcept { return kernel_.input_hovered(); }
    WidgetHandle pressed() const noexcept { return kernel_.input_pressed(); }
    WidgetHandle focused() const noexcept { return kernel_.input_focused(); }
    WidgetHandle captured() const noexcept { return kernel_.input_captured(); }
    bool dragging() const noexcept { return kernel_.input_dragging(); }

    void set_drag_threshold(int px) noexcept { kernel_.set_drag_threshold(px); }

    void dispatch_event(const Event& e) noexcept {
        const WidgetHandle root = kernel_.input_root();
        if (!root) return;
        layout_.run_if_needed(root);
        kernel_.input_dispatch(e);
    }

    WidgetHandle hit_test(int x, int y) noexcept {
        const WidgetHandle root = kernel_.input_root();
        if (!root) return {};
        layout_.run_if_needed(root);
        return kernel_.input_hit_test(x, y);
    }

private:
    SoaKernel& kernel_;
    SoaLayoutPass& layout_;
};
