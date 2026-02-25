//
// Created by Joho on 2026/02/04.
//

module;
#include <cstdint>
#include <optional>

export module gui.ui_input_policy;

import gui.input;
import gui.ui_input_adapter;
import gui.trace;
import util.core;

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
        using PollFn = std::optional<gui::input::Intent> (*)(void*, std::uint32_t) noexcept;

        PollFn poll{nullptr};
        void*  ctx{nullptr};

        [[nodiscard]] std::optional<gui::input::Intent> poll_intent(std::uint32_t now_ms) const noexcept {
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

    struct RawEventPolicy {
        using PollFn = std::optional<gui::input::RawInputEvent> (*)(void*, std::uint32_t) noexcept;

        PollFn poll{nullptr};
        void*  ctx{nullptr};

        [[nodiscard]] std::optional<gui::input::RawInputEvent> poll_raw(std::uint32_t now_ms) const noexcept {
            if (!poll) return std::nullopt;
            return poll(ctx, now_ms);
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
    std::optional<gui::input::Intent> chain_poll(void* ctx, std::uint32_t now_ms) noexcept {
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

    inline std::optional<gui::input::Intent> raw_event_poll(void* ctx, std::uint32_t now_ms) noexcept {
        auto* c = static_cast<RawEventPolicy*>(ctx);
        if (!c) return std::nullopt;
        const auto ev = c->poll_raw(now_ms);
        if (!ev) return std::nullopt;
        return intent_from_raw(*ev);
    }

    [[nodiscard]] inline InputPolicy make_raw_event_policy(RawEventPolicy& policy) noexcept {
        return InputPolicy{&raw_event_poll, &policy};
    }

    template <class RawSource>
    struct SamplerPolicyContext {
        RawSource*         src{nullptr};
        gui::input::Sampler* sampler{nullptr};
    };

    template <class RawSource>
    std::optional<gui::input::Intent> sampler_poll(void* ctx, std::uint32_t now_ms) noexcept {
        auto* c = static_cast<SamplerPolicyContext<RawSource>*>(ctx);
        if (!c || !c->src || !c->sampler) return std::nullopt;
        return c->sampler->poll(*c->src, now_ms);
    }

    template <class RawSource>
    [[nodiscard]] inline InputPolicy make_sampler_policy(SamplerPolicyContext<RawSource>& ctx) noexcept {
        return InputPolicy{&sampler_poll<RawSource>, &ctx};
    }

    template <class RawSource>
    struct RawSamplerPolicyContext {
        RawSource*            src{nullptr};
        gui::input::RawSampler* sampler{nullptr};
    };

    template <class RawSource>
    std::optional<gui::input::Intent> raw_sampler_poll(void* ctx, std::uint32_t now_ms) noexcept {
        auto* c = static_cast<RawSamplerPolicyContext<RawSource>*>(ctx);
        if (!c || !c->src || !c->sampler) return std::nullopt;
        if (auto ev = c->sampler->poll(*c->src, now_ms)) {
            return intent_from_raw(*ev);
        }
        return std::nullopt;
    }

    template <class RawSource>
    [[nodiscard]] inline InputPolicy make_raw_sampler_policy(RawSamplerPolicyContext<RawSource>& ctx) noexcept {
        return InputPolicy{&raw_sampler_poll<RawSource>, &ctx};
    }
} // namespace gui::ui
