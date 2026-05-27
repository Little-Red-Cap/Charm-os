#include <cstdint>
#include <cstdio>

import charm.core;
import power.core;
import power.policy;
import power.port;
import power.trace;
import power.types;
import service_trace;
import trace_core;
import platform.win.power;

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

    struct TraceSink {
        service::TraceBuffer<util::u32, 16> buffer{};
        util::u32 tick{0};
    };

    void trace_emit(void* ctx, trace::TraceKind kind, std::uint32_t id, std::uint64_t payload) noexcept {
        auto* sink = static_cast<TraceSink*>(ctx);
        if (!sink) return;
        service::TraceRecord<util::u32, 16> rec{};
        rec.time = sink->tick++;
        rec.id = id;
        rec.payload = payload;
        rec.count = 1;
        rec.kind = kind;
        sink->buffer.push(rec);
        std::printf("[power] kind=%u id=%u payload=%llu\n",
                    static_cast<unsigned>(kind),
                    id,
                    static_cast<unsigned long long>(payload));
    }
}

int main() {
    power::Manager mgr{};
    DemoPolicy policy{};
    TraceSink trace_sink{};
    power::PortOps port_ops{
        .enter = platform::win::NoopPower::enter,
        .exit = platform::win::NoopPower::exit
    };
    mgr.set_policy(&policy);
    mgr.set_port(&port_ops);

    power::trace::set_sink(power::trace::Sink{
        .ctx = &trace_sink,
        .emit = trace_emit
    });

    mgr.request(power::State::deep_sleep);
    mgr.add_wake_source(power::WakeRequest{.source = power::WakeSource::rtc});
    mgr.add_clock_domain(power::ClockRequest{.domain = power::ClockDomain::core, .enable = true});

    const auto target = mgr.decide_target();
    mgr.enter_state(target);
    mgr.exit_state(target);

    std::printf("[power] trace_count=%zu\n", trace_sink.buffer.size());
    return 0;
}
