#include <cstddef>
#include <cstdio>

import charm.foundation;
import kernel.eda;
import kernel.evt;
import kernel.event_token;
import kernel.poster;
import service.deferred_signal;

namespace {
    struct DeferredEvent {
        int delta{0};
    };

    enum class SubmitRoute {
        none,
        event,
        io_ready,
        demand,
    };

    int g_free_hits = 0;

    void on_tick_free(int value) noexcept {
        ++g_free_hits;
        std::printf("[free] tick=%d hits=%d\n", value, g_free_hits);
    }

    struct Controller {
        int direct_sum{0};
        int state_change_count{0};
        int last_old{0};
        int last_new{0};
        int deferred_sum{0};

        void on_tick_member(int value) noexcept {
            direct_sum += value;
        }

        void on_level_changed(const int& now, const int& old) noexcept {
            ++state_change_count;
            last_old = old;
            last_new = now;
        }

        void on_deferred(const DeferredEvent& ev) noexcept {
            deferred_sum += ev.delta;
        }
    };

    struct FakeScheduler {
        SubmitRoute last_route{SubmitRoute::none};
        kernel::TaskId last_task{};
        kernel::Event last_event{};
        util::u64 next_token{1};

        [[nodiscard]] kernel::EventToken post_token(kernel::TaskId task, kernel::Event evt) noexcept {
            last_route = SubmitRoute::event;
            last_task = task;
            last_event = evt;
            return kernel::EventToken{next_token++};
        }

        [[nodiscard]] kernel::EventToken post_io_ready_token(kernel::TaskId task, kernel::Event evt) noexcept {
            last_route = SubmitRoute::io_ready;
            last_task = task;
            last_event = evt;
            return kernel::EventToken{next_token++};
        }

        [[nodiscard]] kernel::EventToken post_demand_token(kernel::TaskId task, kernel::Event evt) noexcept {
            last_route = SubmitRoute::demand;
            last_task = task;
            last_event = evt;
            return kernel::EventToken{next_token++};
        }

        [[nodiscard]] bool post(kernel::TaskId task, kernel::Event evt) noexcept {
            return post_token(task, evt).value != 0;
        }

        [[nodiscard]] bool post_io_ready(kernel::TaskId task, kernel::Event evt) noexcept {
            return post_io_ready_token(task, evt).value != 0;
        }

        [[nodiscard]] bool post_demand(kernel::TaskId task, kernel::Event evt) noexcept {
            return post_demand_token(task, evt).value != 0;
        }
    };

    template <util::usize Capacity>
    struct FixedPoster {
        service::Fifo<DeferredEvent, Capacity> fifo{};

        [[nodiscard]] bool post(const DeferredEvent& ev) noexcept {
            return fifo.push(ev);
        }

        [[nodiscard]] util::usize drain(util::delegate<const DeferredEvent&> sink) noexcept {
            if (!sink) {
                return 0;
            }
            util::usize drained = 0;
            DeferredEvent ev{};
            while (fifo.pop(ev)) {
                sink(ev);
                ++drained;
            }
            return drained;
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
    Controller controller{};

    service::signal<void(int), 2> tick{};
    const auto free_conn = tick.connect(util::delegate<int>::bind<&on_tick_free>());
    const auto member_conn = tick.connect(util::delegate<int>::bind<&Controller::on_tick_member>(controller));

    if (!expect(static_cast<bool>(free_conn), "connect free slot")) return 1;
    if (!expect(static_cast<bool>(member_conn), "connect member slot")) return 1;

    const auto emitted_first = tick.emit(3);
    if (!expect(emitted_first == 2, "first emit fanout")) return 1;
    if (!expect(g_free_hits == 1, "free slot invoked")) return 1;
    if (!expect(controller.direct_sum == 3, "member slot invoked")) return 1;

    if (!expect(tick.disconnect(free_conn.value()), "disconnect first free slot")) return 1;
    const auto free_conn_rebound = tick.connect(util::delegate<int>::bind<&on_tick_free>());
    if (!expect(static_cast<bool>(free_conn_rebound), "reconnect free slot")) return 1;
    if (!expect(!tick.disconnect(free_conn.value()), "stale connection token rejected")) return 1;

    const auto emitted_second = tick.emit(2);
    if (!expect(emitted_second == 2, "second emit fanout")) return 1;
    if (!expect(g_free_hits == 2, "rebound free slot invoked")) return 1;
    if (!expect(controller.direct_sum == 5, "member slot accumulated")) return 1;

    service::state<int, 2> level{10};
    const auto state_conn =
        level.connect(util::delegate<const int&, const int&>::bind<&Controller::on_level_changed>(controller));
    if (!expect(static_cast<bool>(state_conn), "connect state observer")) return 1;
    if (!expect(!level.set(10), "state suppresses unchanged set")) return 1;
    if (!expect(level.set(12), "state accepts changed set")) return 1;
    if (!expect(controller.state_change_count == 1, "state changed once")) return 1;
    if (!expect(controller.last_old == 10 && controller.last_new == 12, "state reports old and new")) return 1;

    FixedPoster<4> poster{};
    service::deferred_signal<DeferredEvent, FixedPoster<4>> deferred{poster};
    if (!expect(deferred.post(DeferredEvent{4}), "post deferred event 4")) return 1;
    if (!expect(deferred.post(DeferredEvent{6}), "post deferred event 6")) return 1;
    if (!expect(controller.deferred_sum == 0, "deferred post does not call immediately")) return 1;

    const auto drained_first =
        poster.drain(util::delegate<const DeferredEvent&>::bind<&Controller::on_deferred>(controller));
    if (!expect(drained_first == 2, "drain first deferred batch")) return 1;
    if (!expect(controller.deferred_sum == 10, "deferred events applied after drain")) return 1;

    if (!expect(deferred.post(DeferredEvent{1}), "post deferred event 1")) return 1;
    if (!expect(deferred.post(DeferredEvent{2}), "post deferred event 2")) return 1;
    if (!expect(deferred.post(DeferredEvent{3}), "post deferred event 3")) return 1;
    if (!expect(deferred.post(DeferredEvent{4}), "post deferred event 4 again")) return 1;
    if (!expect(!deferred.post(DeferredEvent{5}), "deferred post stays bounded")) return 1;

    const auto drained_second =
        poster.drain(util::delegate<const DeferredEvent&>::bind<&Controller::on_deferred>(controller));
    if (!expect(drained_second == 4, "drain bounded deferred batch")) return 1;
    if (!expect(controller.deferred_sum == 20, "bounded deferred batch accumulated")) return 1;

    FakeScheduler scheduler{};
    constexpr auto worker_task = kernel::TaskId{7};
    const auto scheduler_event = kernel::make_event(kernel::EventId::message, util::u32(9));

    auto posters = kernel::make_poster_set(scheduler, worker_task);

    if (!expect(static_cast<bool>(posters), "make scheduler poster set")) return 1;
    if (!expect(posters.task_id() == worker_task, "poster set keeps task id")) return 1;
    if (!expect(posters.event.post(scheduler_event), "post scheduler event")) return 1;
    if (!expect(scheduler.last_route == SubmitRoute::event, "event poster keeps event submit semantics")) return 1;
    if (!expect(scheduler.last_task == worker_task, "event poster keeps task id")) return 1;

    if (!expect(posters.io_ready.post(scheduler_event), "post scheduler io_ready")) return 1;
    if (!expect(scheduler.last_route == SubmitRoute::io_ready, "io_ready poster keeps io_ready semantics")) return 1;

    service::deferred_signal<kernel::Event, decltype(posters.demand)> scheduler_deferred{posters.demand};
    if (!expect(scheduler_deferred.post(scheduler_event), "post scheduler demand")) return 1;
    if (!expect(scheduler.last_route == SubmitRoute::demand, "deferred signal keeps demand semantics")) return 1;
    if (!expect(kernel::payload_u32(scheduler.last_event) == 9, "scheduler payload preserved")) return 1;

    std::printf("[signal] direct_sum=%d free_hits=%d\n", controller.direct_sum, g_free_hits);
    std::printf("[state] old=%d new=%d changes=%d\n",
                controller.last_old,
                controller.last_new,
                controller.state_change_count);
    std::printf("[deferred] deferred_sum=%d\n", controller.deferred_sum);
    std::printf("[scheduler] route=%d task=%zu payload=%u\n",
                static_cast<int>(scheduler.last_route),
                worker_task.value,
                static_cast<unsigned>(kernel::payload_u32(scheduler.last_event)));
    std::puts("[service_signal_state_demo] ok");
    return 0;
}
