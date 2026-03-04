module;

#include <optional>

export module charm.core.input_router_bridge;

#if !defined(CHARM_VIVID_SOA_ONLY)
import charm.core.gui;
import input.raw_event;
import input.router;
import ui.input_adapter;
import util.error;

export
class InputRouterBridge {
public:
    void bind(input::Router& router, Gui& gui) noexcept {
        router_ = &router;
        gui_ = &gui;
    }

    void set_consume(bool on) noexcept { consume_ = on; }

    util::Result<void> start() noexcept {
        if (!router_ || !gui_) {
            return util::unexpected(util::Errc::invalid_arg);
        }
        if (sub_.id != 0) {
            return {};
        }
        auto r = router_->subscribe(&InputRouterBridge::on_raw, this);
        if (!r) {
            return util::unexpected(r.error());
        }
        sub_ = r.value();
        return {};
    }

    void stop() noexcept {
        if (!router_ || sub_.id == 0) return;
        (void)router_->unsubscribe(sub_);
        sub_ = {};
    }

private:
    static bool on_raw(void* ctx, const input::RawInputEvent& raw) noexcept {
        auto* self = static_cast<InputRouterBridge*>(ctx);
        if (!self || !self->gui_) return false;
        if (auto ev = input::adapter::to_vivid_event(raw)) {
            self->gui_->dispatch_event(*ev);
            return self->consume_;
        }
        return false;
    }

    input::Router* router_{nullptr};
    Gui* gui_{nullptr};
    input::Subscription sub_{};
    bool consume_{true};
};
#else
import util.error;

export
class InputRouterBridge {
public:
    template <class RouterT, class GuiT>
    void bind(RouterT&, GuiT&) noexcept {}

    void set_consume(bool) noexcept {}

    util::Result<void> start() noexcept { return {}; }

    void stop() noexcept {}
};
#endif
