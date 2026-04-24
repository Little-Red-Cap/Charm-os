//
// Created by Joho on 2026/02/04.
//

module;
#include <cstdint>
#include <optional>

export module gui.ui_input_policy;

import gui.ui_input_adapter;
import gui.trace;
import input.intent;
import input.queue;
import input.raw_event;
import input.router;
import util.core;
import util.error;

export namespace gui::ui {
    enum class InputPolicyId : std::uint8_t {
        Default = 0,
        Encoder = 1,
        Touch = 2,
        Remote = 3,
        Custom = 4,
        Count
    };

    struct InputPolicy {
        using PollFn = std::optional<::input::Intent> (*)(void*, std::uint32_t) noexcept;

        PollFn poll{nullptr};
        void*  ctx{nullptr};

        [[nodiscard]] std::optional<::input::Intent> poll_intent(std::uint32_t now_ms) const noexcept {
            if (!poll) return std::nullopt;
            auto it = poll(ctx, now_ms);
            if (it) {
                const auto id = static_cast<util::u32>(gui::trace::TraceId::InputIntentBase)
                    + static_cast<util::u32>(it->type);
                gui::trace::trace_counter_delta(static_cast<gui::trace::TraceId>(id), 1);
            }
            return it;
        }
    };

    struct InputPolicyRegistry {
        static constexpr std::uint8_t kMax = static_cast<std::uint8_t>(InputPolicyId::Count);
        InputPolicy slots[kMax]{};
        bool        used[kMax]{};

        inline void set(InputPolicyId id, InputPolicy p) noexcept {
            const auto idx = static_cast<std::uint8_t>(id);
            if (idx >= kMax) return;
            slots[idx] = p;
            used[idx] = true;
        }

        [[nodiscard]] inline InputPolicy get(InputPolicyId id) const noexcept {
            const auto idx = static_cast<std::uint8_t>(id);
            if (idx >= kMax || !used[idx]) return {};
            return slots[idx];
        }
    };

    template <std::uint8_t MaxPolicies>
    struct PolicyChain {
        InputPolicy   policies[MaxPolicies]{};
        std::uint8_t  count{0};

        inline void clear() noexcept { count = 0; }

        inline bool add(InputPolicy p) noexcept {
            if (count >= MaxPolicies) return false;
            policies[count++] = p;
            return true;
        }
    };

    template <std::uint8_t MaxPolicies>
    std::optional<::input::Intent> chain_poll(void* ctx, std::uint32_t now_ms) noexcept {
        auto* c = static_cast<PolicyChain<MaxPolicies>*>(ctx);
        if (!c) return std::nullopt;
        for (std::uint8_t i = 0; i < c->count; ++i) {
            const auto it = c->policies[i].poll_intent(now_ms);
            if (it) return it;
        }
        return std::nullopt;
    }

    template <std::uint8_t MaxPolicies>
    [[nodiscard]] inline InputPolicy make_policy_chain(PolicyChain<MaxPolicies>& chain) noexcept {
        return InputPolicy{&chain_poll<MaxPolicies>, &chain};
    }

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

        std::optional<::input::Intent> poll(std::uint32_t) noexcept {
            return queue_.pop();
        }

        util::u32 dropped() const noexcept { return dropped_; }

    private:
        static bool on_raw(void* ctx, const ::input::RawInputEvent& raw) noexcept {
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

        static std::optional<::input::Intent> poll_trampoline(void* ctx,
                                                              std::uint32_t now_ms) noexcept {
            auto* self = static_cast<RouterIntentQueue*>(ctx);
            if (!self) return std::nullopt;
            return self->poll(now_ms);
        }

        ::input::Router* router_{nullptr};
        ::input::Subscription sub_{};
        ::input::RingQueue<::input::Intent, Capacity> queue_{};
        util::u32 dropped_{0};
        bool consume_{true};
    };
} // namespace gui::ui
