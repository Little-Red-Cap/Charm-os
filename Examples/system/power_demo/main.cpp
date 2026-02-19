#include <cstdint>
#include <cstdio>

import charm.runtime;

namespace {
    struct DemoPolicy final : power::Policy {
        power::Constraints constraints() const noexcept override {
            return power::Constraints{.min_state = power::State::idle, .allow_deep = false};
        }

        power::State choose_target(const power::PolicySnapshot& snapshot) const noexcept override {
            if (snapshot.target > power::State::sleep) return power::State::sleep;
            return snapshot.target;
        }
    };

    void trace_emit(void*, trace::TraceKind kind, std::uint32_t id, std::uint64_t payload) noexcept {
        std::printf("[power] kind=%u id=%u payload=%llu\n",
                    static_cast<unsigned>(kind),
                    id,
                    static_cast<unsigned long long>(payload));
    }
}

int main() {
    power::Manager mgr{};
    DemoPolicy policy{};
    mgr.set_policy(&policy);

    power::trace::set_sink(power::trace::Sink{
        .ctx = nullptr,
        .emit = trace_emit
    });

    mgr.request(power::State::deep_sleep);
    mgr.add_wake_source(power::WakeRequest{.source = power::WakeSource::rtc});
    mgr.add_clock_domain(power::ClockRequest{.domain = power::ClockDomain::core, .enable = true});

    const auto target = mgr.decide_target();
    mgr.enter_state(target);
    mgr.exit_state(target);
    return 0;
}
