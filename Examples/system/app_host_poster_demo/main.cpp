#include <cstdio>

import charm.system.app_host;
import charm.system.caps;
import kernel.eda;
import kernel.evt;
import kernel.ssu;
import service.deferred_signal;
import util.core;

namespace {
    struct WorkerTask {
        static constexpr kernel::Priority priority{0};
        static constexpr kernel::EventMask mask = kernel::event_mask(kernel::EventId::message);

        util::u32 handled{0};
        util::u32 last_payload{0};

        static consteval kernel::ssu::Meta ssu_meta() noexcept {
            return kernel::ssu::Meta{
                .domain = kernel::ssu::ExecutionDomain::task_only,
                .trigger = kernel::ssu::TriggerKind::demand,
                .budget = kernel::ssu::BudgetKind::single_step,
                .blocking = kernel::ssu::BlockingKind::non_blocking,
                .name = "demo.worker",
            };
        }

        void on_event(kernel::Event evt) noexcept {
            if (evt.id != kernel::EventId::message) {
                return;
            }
            ++handled;
            last_payload = kernel::payload_u32(evt);
        }
    };

    [[nodiscard]] bool expect(bool condition, const char* message) noexcept {
        if (!condition) {
            std::printf("[ERR] %s\n", message);
            return false;
        }
        return true;
    }
}

int main() {
    using Host = charm::system::AppHost<charm::system::DefaultCaps,
                                        charm::system::AppHostConfig,
                                        WorkerTask>;

    Host host{};
    auto& worker = host.task<WorkerTask>();
    auto posters = host.posters<WorkerTask>();

    if (!expect(static_cast<bool>(posters), "materialize task-local posters")) return 1;
    if (!expect(posters.task_id() == Host::task_id<WorkerTask>(), "poster set keeps worker task id")) return 1;

    const auto event_one = kernel::make_event(kernel::EventId::message, util::u32(7));
    if (!expect(posters.event.post(event_one), "post event lane")) return 1;
    if (!expect(worker.handled == 0, "event lane stays deferred until scheduler runs")) return 1;

    if (!expect(host.dispatch_batch(8) >= 1, "dispatch first batch")) return 1;
    if (!expect(worker.handled == 1, "event lane reaches worker after scheduler run")) return 1;
    if (!expect(worker.last_payload == 7, "event lane preserves payload")) return 1;

    service::deferred_signal<kernel::Event, decltype(posters.demand)> deferred{posters.demand};
    const auto event_two = kernel::make_event(kernel::EventId::message, util::u32(11));
    if (!expect(deferred.post(event_two), "post demand lane through deferred signal")) return 1;
    if (!expect(worker.handled == 1, "demand lane also stays deferred until scheduler runs")) return 1;

    if (!expect(host.dispatch_batch(8) >= 1, "dispatch second batch")) return 1;
    if (!expect(worker.handled == 2, "demand lane reaches worker after scheduler run")) return 1;
    if (!expect(worker.last_payload == 11, "demand lane preserves payload")) return 1;

    std::printf("[worker] handled=%u last_payload=%u task=%zu\n",
                static_cast<unsigned>(worker.handled),
                static_cast<unsigned>(worker.last_payload),
                posters.task_id().value);
    std::puts("[app_host_poster_demo] ok");
    return 0;
}
