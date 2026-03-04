//
// Created by Joho on 2026/03/02.
//

module;

#include <cstddef>
#include <cstdint>
#include <optional>

export module input.service;

import charm.system.clock;
import hal_input;
import input.raw_event;
import input.raw_sampler;
import input.sampler;
import util.core;

namespace input::detail {
    inline const hal::RawInputDriver g_null_driver{};
}

export namespace input {
    struct ServiceCfg {
        SamplerCfg sampler{};
    };

    class InputService {
    public:
        InputService() noexcept = default;
        explicit InputService(const hal::RawInputSource& source,
                              ServiceCfg cfg = {}) noexcept
            : source_(source), sampler_(cfg.sampler) {}

        void set_source(const hal::RawInputSource& source) noexcept {
            source_ = source;
        }

        RawSampler& sampler() noexcept { return sampler_; }

        std::optional<RawInputEvent> poll_raw() noexcept {
            const auto now = static_cast<std::uint32_t>(charm::system::clock().now_ms());
            return poll_raw_at(now);
        }

        std::optional<RawInputEvent> poll_raw_at(std::uint32_t now_ms) noexcept {
            return sampler_.poll(source_, now_ms);
        }

        template <class Queue>
        std::size_t poll_into(Queue& queue, std::size_t budget) noexcept {
            std::size_t pushed = 0;
            if (budget == 0) return 0;
            for (; pushed < budget; ++pushed) {
                auto ev = poll_raw();
                if (!ev) break;
                if (!queue.push(*ev)) break;
            }
            return pushed;
        }

    private:
        hal::RawInputSource source_{detail::g_null_driver};
        RawSampler sampler_{};
    };
} // namespace input
