module;

#include <cstdint>
#include <optional>

export module gui.ui_input_router_bridge;

import gui.input;
import gui.ui_input_adapter;
import gui.ui_input_policy;
import input.queue;
import input.router;
import util.core;
import util.error;

export namespace gui::ui {
    template <std::uint16_t Capacity = 32>
    class RouterIntentQueue {
    public:
        util::Result<void> start(::input::Router& router,
                                 ::input::EventMask mask = ::input::kAll) noexcept {
            if (sub_.id != 0) {
                return {};
            }
            router_ = &router;
            auto r = router.subscribe(&RouterIntentQueue::on_raw, this, mask);
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

        void set_consume(bool on) noexcept { consume_ = on; }

        InputPolicy policy() noexcept {
            return InputPolicy{&RouterIntentQueue::poll_trampoline, this};
        }

        std::optional<gui::input::Intent> poll(std::uint32_t) noexcept {
            return queue_.pop();
        }

        util::u32 dropped() const noexcept { return dropped_; }

    private:
        static bool on_raw(void* ctx, const gui::input::RawInputEvent& raw) noexcept {
            auto* self = static_cast<RouterIntentQueue*>(ctx);
            if (!self) return false;
            auto it = intent_from_raw(raw);
            if (it) {
                if (!self->queue_.push(*it)) {
                    ++self->dropped_;
                }
                return self->consume_;
            }
            return false;
        }

        static std::optional<gui::input::Intent> poll_trampoline(void* ctx,
                                                                 std::uint32_t now_ms) noexcept {
            auto* self = static_cast<RouterIntentQueue*>(ctx);
            if (!self) return std::nullopt;
            return self->poll(now_ms);
        }

        ::input::Router* router_{nullptr};
        ::input::Subscription sub_{};
        ::input::RingQueue<gui::input::Intent, Capacity> queue_{};
        util::u32 dropped_{0};
        bool consume_{true};
    };
} // namespace gui::ui
