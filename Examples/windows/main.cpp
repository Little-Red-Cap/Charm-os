#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <utility>
#include <type_traits>

import kernel.capabilities;
import kernel.config;
import kernel.deps;
import kernel.eda;
import kernel.evt;
import kernel.event_token;
import kernel.dynamic_registry;
import kernel.init_list;
import kernel.ipc;
import kernel.scheduler;
import kernel.sync;
import kernel.sync_base;
import kernel.sync_object;
import kernel.sync_unified;
import kernel.task_api;
import kernel.task_decl;
import kernel.task_auto;
import kernel.thread;
import kernel.thread_blocking;
import kernel.thread_api;
import kernel.task_pool;
import kernel.timer;
import kernel.timer_wheel;
import kernel.wait_list;
import kernel.wait_set;
import kernel.wait_token;
import kernel.startup;
import platform.win.irq_guard;
import platform.win.manual_time_source;
import platform.win.wakeup;
import service.bitmap;
import service.ring_queue;
import service.static_pool;
import service.fixed_vector;
import service.handle_table;
import service.slab;
import service.fixed_allocator;
import service.slot_pool;

namespace demo {
    inline service::RingQueue<std::uint32_t, 4>* g_queue = nullptr;
    struct Logger;

    struct ServiceConfig {
        static constexpr bool use_ring_queue = true;
        static constexpr bool use_static_pool = true;
        static constexpr bool use_slot_pool = true;
    };

    struct ComponentConfig {
        static constexpr bool use_demo_component = true;
        static constexpr bool use_pool_component = true;
    };

    inline void service_hook_a() {
        std::printf("[Service] init A\n");
    }

    inline void hal_hook_a() {
        std::printf("[HAL] init A\n");
    }

    inline void component_hook_a() {
        std::printf("[Component] init A\n");
    }

    struct Config {
        static constexpr bool enable_timer = true;
        static constexpr bool enable_dynamic_priority = true;
        static constexpr std::size_t priority_levels = 3;
        static constexpr std::size_t evtq_capacity = 16;
        static constexpr std::size_t timer_capacity = 8;
        static constexpr std::size_t dispatch_budget = 2;
        using timer_policy = kernel::HierWheelTimerPolicy<8, 2>;
        static constexpr bool enable_event_boost = true;
        static constexpr std::uint32_t boost_mask = kernel::event_mask(kernel::EventId::sync);
        static constexpr bool enable_timer_merge = true;
        static constexpr bool enable_event_dedup = true;
        static constexpr std::size_t wakeup_batch = 2;
        static constexpr bool enable_event_debounce = true;
        static constexpr std::size_t debounce_window = 1;
        static constexpr bool enable_prio_stats = true;
        static constexpr bool enable_event_coalesce = true;
        static constexpr bool enable_trace = true;
        static constexpr std::size_t trace_capacity = 8;
        static constexpr bool enable_rate_limit = true;
        static constexpr bool enable_task_boost = true;
        static constexpr bool enable_alert = true;
        static constexpr std::size_t alert_queue_warn = 1;
        static constexpr std::size_t alert_timer_warn = 4;
        static constexpr std::size_t alert_filtered_warn = 10;
        static constexpr std::size_t alert_queue_err = 2;
        static constexpr std::size_t alert_timer_err = 6;
        static constexpr std::size_t alert_filtered_err = 15;
        static constexpr bool drop_oldest = true;

        template <typename Task>
        static consteval bool enable_task() {
            if constexpr (std::is_same_v<Task, Logger>) {
                return false;
            } else {
                return true;
            }
        }
    };

    struct Urgent {
        static constexpr kernel::Priority priority{2};

        void on_event(kernel::Event evt) {
            if (evt.id == kernel::EventId::init) {
                std::printf("[Urgent] init\n");
                return;
            }
            if (evt.id == kernel::EventId::tick) {
                std::printf("[Urgent] tick=%u\n", kernel::payload_u32(evt));
            }
        }
    };

    struct HeartbeatHandler {
        std::uint32_t ticks{0};

        static constexpr kernel::EventMask event_mask =
            kernel::event_mask(kernel::EventId::init)
            | kernel::event_mask(kernel::EventId::tick)
            | kernel::event_mask(kernel::EventId::sync);

        bool should_accept(kernel::Event evt) const {
            if (evt.id == kernel::EventId::tick) {
                return (kernel::payload_u32(evt) % 2u) == 1u;
            }
            return true;
        }

        void on_start() {
            std::printf("[Heartbeat] on_start\n");
        }

        void on_stop() {
            std::printf("[Heartbeat] on_stop\n");
        }

        void operator()(kernel::Event evt) {
            if (evt.id == kernel::EventId::init) {
                ticks = 0;
                std::printf("[Heartbeat] init\n");
                return;
            }
            if (evt.id == kernel::EventId::tick) {
                ++ticks;
                std::printf("[Heartbeat] tick=%u\n", ticks);
            }
            if (evt.id == kernel::EventId::sync) {
                std::printf("[Heartbeat] sync status=%u\n", kernel::payload_u32(evt));
            }
        }
    };

    using Heartbeat = kernel::EdaTaskDecl<HeartbeatHandler, kernel::Priority{1}>;

    struct Logger {
        static constexpr kernel::Priority priority{0};

        void on_event(kernel::Event evt) {
            if (evt.id == kernel::EventId::init) {
                std::printf("[Logger] init\n");
                return;
            }
            if (evt.id == kernel::EventId::terminate) {
                std::printf("[Logger] terminate\n");
                return;
            }
            if (evt.id == kernel::EventId::tick) {
                std::printf("[Logger] event value=%u\n", kernel::payload_u32(evt));
                return;
            }
            if (evt.id == kernel::EventId::message && g_queue != nullptr) {
                auto item = g_queue->pop();
                if (item.has_value()) {
                    std::printf("[Logger] message=%u\n", *item);
                }
                return;
            }
            if (evt.id == kernel::EventId::sync) {
                std::printf("[Logger] sync status=%u\n", kernel::payload_u32(evt));
            }
        }
    };

    struct BlinkContext {
        std::uint32_t count{0};
    };

    inline void blink_step(BlinkContext& ctx, kernel::ThreadControl& ctrl, kernel::Event evt) {
        if (evt.id == kernel::EventId::init) {
            std::printf("[Thread] init\n");
            return;
        }
        if (evt.id == kernel::EventId::tick) {
            ++ctx.count;
            std::printf("[Thread] step=%u\n", ctx.count);
            if (ctx.count >= 3) {
                ctrl.finish();
            }
        }
    }

    using ThreadTask = kernel::ThreadTask<BlinkContext, blink_step, kernel::Priority{0}>;

    struct BlockingContext {
        bool waiting{false};
    };

    inline void blocking_handler(kernel::ThreadState<BlockingContext>& state, kernel::Event evt) {
        if (evt.id == kernel::EventId::init) {
            std::printf("[Blocking] init\n");
            return;
        }
        if (evt.id == kernel::EventId::sync) {
            std::printf("[Blocking] sync status=%u\n", kernel::payload_u32(evt));
            if (state.control != nullptr) {
                if (kernel::payload_u32(evt) == static_cast<std::uint32_t>(kernel::WaitResult::timeout)) {
                    state.control->block();
                    std::printf("[Blocking] blocked\n");
                } else {
                    state.control->resume();
                    std::printf("[Blocking] resumed\n");
                }
            }
            return;
        }
    }

    using BlockingTask = kernel::ThreadBlockingTask<BlockingContext, blocking_handler, kernel::Priority{0}>;

    struct DynTask {
        std::uint32_t ticks{0};

        void on_start() {
            std::printf("[Dyn] on_start\n");
        }

        void on_stop() {
            std::printf("[Dyn] on_stop\n");
        }

        void on_event(kernel::Event evt) {
            if (evt.id == kernel::EventId::init) {
                std::printf("[Dyn] init\n");
                return;
            }
            if (evt.id == kernel::EventId::tick) {
                ++ticks;
                std::printf("[Dyn] tick=%u\n", ticks);
                return;
            }
            if (evt.id == kernel::EventId::terminate) {
                std::printf("[Dyn] terminate\n");
            }
        }
    };
}

struct Caps {
    using TimeSource = platform::win::ManualTimeSource;
    using IrqGuard = platform::win::SpinIrqGuard;
    using Wakeup = platform::win::NoopWakeup;
    using SwiTrigger = kernel::NoopSwiTrigger;
};

int main() {
    kernel::validate_deps<demo::ServiceConfig, demo::ComponentConfig>();

    using Registry = kernel::TaskRegistry<demo::Urgent, demo::Heartbeat, demo::Logger, demo::ThreadTask, demo::BlockingTask>;
    Registry registry{};

    Caps caps{};

    kernel::InitList<2> services{};
    kernel::InitList<2> hals{};
    kernel::InitList<2> components{};
    (void)services.add(demo::service_hook_a);
    (void)hals.add(demo::hal_hook_a);
    (void)components.add(demo::component_hook_a);

    auto running = kernel::boot<demo::Config>(registry, caps, services, hals, components);

    const auto urgent_id = Registry::id_of<demo::Urgent>();
    const auto heartbeat_id = Registry::id_of<demo::Heartbeat>();
    const auto logger_id = Registry::id_of<demo::Logger>();
    const auto thread_id = Registry::id_of<demo::ThreadTask>();
    const auto blocking_id = Registry::id_of<demo::BlockingTask>();

    using PostItem = decltype(running)::PostItem;
    std::array<PostItem, 4> batch{
        PostItem{urgent_id, kernel::make_event(kernel::EventId::tick, static_cast<std::uint32_t>(7))},
        PostItem{heartbeat_id, kernel::make_event(kernel::EventId::sync, static_cast<std::uint32_t>(1))},
        PostItem{heartbeat_id, kernel::make_event(kernel::EventId::sync, static_cast<std::uint32_t>(2))},
        PostItem{urgent_id, kernel::make_event(kernel::EventId::tick, static_cast<std::uint32_t>(8))}
    };
    (void)running.post_many_dedup(batch.data(), batch.size());
    (void)running.dispatch_batch(2);
    (void)running.set_task_cap(heartbeat_id, 1);
    (void)running.set_task_event_cap(heartbeat_id, kernel::EventId::tick, 1);
    using AlertType = decltype(running)::AlertType;
    using AlertLevel = decltype(running)::AlertLevel;
    running.set_alert_hook([](AlertType type, AlertLevel level, std::size_t value) {
        const char* name = type == AlertType::queue ? "queue"
            : (type == AlertType::timer ? "timer" : "filtered");
        const char* lvl = level == AlertLevel::warning ? "warn" : "error";
        std::printf("[Alert] %s %s=%u\n", lvl, name, static_cast<unsigned>(value));
    });
    (void)running.set_task_boost(heartbeat_id,
        kernel::event_mask(kernel::EventId::sync),
        2,
        2);

    kernel::Semaphore<Caps, 2> sem{};
    kernel::Mutex<Caps> lock{};
    service::Bitmap<32> bitmap{};
    service::RingQueue<std::uint32_t, 4> queue{};
    service::StaticPool<std::uint32_t, 4> pool{};
    service::FixedVector<std::uint32_t, 4> vec{};
    service::HandleTable<std::uint32_t, 4> handles{};
    service::HandleTable<std::uint32_t, 4>::Handle handle{};
    service::Slab<16, 4> slab{};
    service::FixedAllocator<32> allocator{};
    service::SlotPool<std::uint32_t, 4> slot_pool{};

    demo::g_queue = &queue;

    kernel::QueueIpc<decltype(running), std::uint32_t, 4> msg_queue(running, queue);
    kernel::SemaphoreIpc<Caps, 2, decltype(running)> sem_ipc(running);
    kernel::SyncObject<decltype(running)> sync_object(running);
    kernel::ThreadApi<decltype(running)> thread_api(running);
    kernel::SyncUnified<decltype(running), 4> sync_unified(running);
    kernel::TaskApi<decltype(running)> task_api(running);

    (void)msg_queue.send(logger_id, 42);
    (void)sem_ipc.wait(heartbeat_id);
    (void)sync_object.pend(blocking_id);
    (void)sync_unified.wait(blocking_id, kernel::WaitToken{1});

    for (std::uint32_t i = 1; i <= 3; ++i) {
        const auto base = Caps::TimeSource::now();
        const auto cancel_token = running.schedule_at_token(base + 1000, logger_id, kernel::make_event(kernel::EventId::tick, i));
        (void)running.schedule_at(base + 1000, urgent_id, kernel::make_event(kernel::EventId::tick, i));
        (void)running.schedule_at(base + 1000, thread_id, kernel::make_event(kernel::EventId::tick, i));
        (void)running.schedule_at(base + 2000, heartbeat_id, kernel::make_event(kernel::EventId::tick, i));
        (void)thread_api.sleep(blocking_id, base + 1500);
        (void)task_api.sleep(blocking_id, base + 1700);
        (void)task_api.wait_timeout(sync_unified, blocking_id, kernel::WaitToken{2}, base + 1800);

        if (i == 2) {
            (void)running.cancel_event(cancel_token);
        }

        Caps::TimeSource::advance(1000);
        const auto t1 = Caps::TimeSource::now();
        while (running.tick(t1)) {
        }
        while (running.run_auto()) {
        }

        Caps::TimeSource::advance(1000);
        const auto t2 = Caps::TimeSource::now();
        while (running.tick(t2)) {
        }
        while (running.run_auto()) {
        }

        const bool released = sem.release();
        const bool acquired = sem.try_acquire();
        if (released && acquired && lock.try_lock()) {
            std::printf("[Sync] released=%u acquired=1 locked=1\n", i);
            lock.unlock();
        } else {
            std::printf("[Sync] released=%u acquired=%u locked=0\n", i, acquired ? 1u : 0u);
        }

        if (i == 2) {
            (void)running.set_priority(heartbeat_id, kernel::Priority{2});
            (void)task_api.stop_task(heartbeat_id);
            std::printf("[Heartbeat] state=%u\n", static_cast<unsigned>(task_api.state_of(heartbeat_id)));
        }
        if (i == 3) {
            (void)running.schedule_at(base + 4000, logger_id, kernel::make_event(kernel::EventId::tick, static_cast<std::uint32_t>(99)));
            (void)task_api.restart_task(heartbeat_id);
            std::printf("[Heartbeat] state=%u\n", static_cast<unsigned>(task_api.state_of(heartbeat_id)));
            (void)task_api.terminate_task(logger_id);
        }

        bitmap.set(i);
        (void)queue.push(i);
        const auto slot = pool.allocate();
        if (slot.has_value()) {
            pool.get(*slot) = i;
            pool.free(*slot);
        }
        (void)vec.push_back(i);
        if (handles.allocate(handle)) {
            handles.get(handle) = i;
            handles.release(handle);
        }
        const auto slab_index = slab.allocate();
        if (slab_index.has_value()) {
            auto block = slab.block(*slab_index);
            if (!block.empty()) {
                block[0] = std::byte{static_cast<unsigned char>(i)};
            }
            slab.release(*slab_index);
        }
        if (auto addr = allocator.allocate(8); addr.has_value()) {
            allocator.release();
        }
        if (auto slot = slot_pool.acquire(); slot.has_value()) {
            slot_pool.get(*slot) = i;
            slot_pool.release(*slot);
        }

        (void)sem_ipc.post();
        (void)sync_object.post_one();
        (void)sync_unified.notify_one(kernel::WaitResult::ok);

        Caps::TimeSource::advance(1000);
        const auto t3 = Caps::TimeSource::now();
        while (running.tick(t3)) {
        }
        while (running.run_auto()) {
        }
    }

    std::printf("[Demo] drain after terminate\n");
    Caps::TimeSource::advance(5000);
    const auto t4 = Caps::TimeSource::now();
    while (running.tick(t4)) {
    }
    while (running.run_auto()) {
    }

    std::printf("[Demo] dynamic registry\n");
    kernel::DynamicTaskRegistry<2> dyn_registry{};
    kernel::TaskPool<demo::DynTask, 2> dyn_pool{};
    auto dyn_handle = dyn_pool.acquire();
    if (!dyn_handle.has_value()) {
        return 0;
    }
    auto& dyn_task = dyn_pool.get(*dyn_handle);
    auto dyn_id = dyn_registry.register_task(dyn_task, kernel::Priority{1});
    auto dyn_created = kernel::make_scheduler<demo::Config>(dyn_registry, caps);
    auto dyn_running = kernel::start(std::move(dyn_created));
    if (dyn_id.has_value()) {
        (void)dyn_running.schedule_at(Caps::TimeSource::now() + 1, *dyn_id, kernel::make_event(kernel::EventId::tick));
    }
    Caps::TimeSource::advance(1);
    const auto dt1 = Caps::TimeSource::now();
    while (dyn_running.tick(dt1)) {
    }
    while (dyn_running.run_auto()) {
    }
    if (dyn_id.has_value()) {
        demo::DynTask dyn_task2{};
        (void)dyn_registry.replace(*dyn_id, dyn_task2, kernel::Priority{1});
        (void)dyn_running.activate_task(*dyn_id, kernel::Priority{1});
        (void)dyn_running.schedule_at(Caps::TimeSource::now() + 1, *dyn_id, kernel::make_event(kernel::EventId::tick));
        Caps::TimeSource::advance(1);
        const auto dtr = Caps::TimeSource::now();
        while (dyn_running.tick(dtr)) {
        }
        while (dyn_running.run_auto()) {
        }
        (void)dyn_running.terminate_task(*dyn_id);
    }
    while (dyn_running.run_auto()) {
    }
    if (dyn_handle.has_value()) {
        dyn_pool.release(*dyn_handle);
    }
    auto dyn_id2 = dyn_registry.register_task(dyn_task, kernel::Priority{1});
    if (dyn_id2.has_value()) {
        (void)dyn_running.activate_task(*dyn_id2, kernel::Priority{1});
        (void)dyn_running.schedule_at(Caps::TimeSource::now() + 1, *dyn_id2, kernel::make_event(kernel::EventId::tick));
    }
    Caps::TimeSource::advance(1);
    const auto dt2 = Caps::TimeSource::now();
    while (dyn_running.tick(dt2)) {
    }
    while (dyn_running.run_auto()) {
    }

    std::printf("[Demo] auto register\n");
    demo::DynTask dyn_a{};
    demo::DynTask dyn_b{};
    auto dyn_tasks = std::tuple<demo::DynTask&, demo::DynTask&>{dyn_a, dyn_b};
    auto auto_ids = kernel::register_enabled<demo::Config>(dyn_registry, dyn_tasks);
    for (std::size_t i = 0; i < auto_ids.size(); ++i) {
        if (auto_ids[i].has_value()) {
            (void)dyn_running.activate_task(*auto_ids[i], kernel::Priority{1});
            (void)dyn_running.schedule_at(Caps::TimeSource::now() + 1, *auto_ids[i], kernel::make_event(kernel::EventId::tick));
        } else {
            std::printf("[Auto] task %u disabled\n", static_cast<unsigned>(i));
        }
    }
    Caps::TimeSource::advance(1);
    const auto dt3 = Caps::TimeSource::now();
    while (dyn_running.tick(dt3)) {
    }
    while (dyn_running.run_auto()) {
    }

    char stats_buf[256]{};
    char stats_json[256]{};
    char tasks_json[512]{};
    char trace_json[512]{};
    char events_json[512]{};
    char source_json[128]{};
    char diff_buf[128]{};
    (void)running.format_snapshot(stats_buf, sizeof(stats_buf));
    (void)running.format_snapshot_json(stats_json, sizeof(stats_json));
    (void)running.format_tasks_json(tasks_json, sizeof(tasks_json));
    (void)running.format_trace_json(trace_json, sizeof(trace_json));
    (void)running.format_event_stats_json(events_json, sizeof(events_json));
    (void)running.format_event_source_json(source_json, sizeof(source_json));
    (void)running.format_snapshot_diff(diff_buf, sizeof(diff_buf));
    std::printf("[Stats] %s\n", stats_buf);
    std::printf("[Stats.json] %s\n", stats_json);
    std::printf("[Tasks.json] %s\n", tasks_json);
    std::printf("[Trace.json] %s\n", trace_json);
    std::printf("[Events.json] %s\n", events_json);
    std::printf("[Source.json] %s\n", source_json);
    std::printf("[Stats.diff] %s\n", diff_buf);
    std::printf("[Replay] %llu\n", static_cast<unsigned long long>(running.replay_recent(3)));
    const auto task_snap = running.task_snapshot();
    std::printf("[Task] urgent state=%u enabled=%u prio=%u\n",
        static_cast<unsigned>(task_snap[urgent_id.value].state),
        task_snap[urgent_id.value].enabled ? 1u : 0u,
        static_cast<unsigned>(task_snap[urgent_id.value].priority));
    std::printf("[Task] heartbeat state=%u enabled=%u prio=%u\n",
        static_cast<unsigned>(task_snap[heartbeat_id.value].state),
        task_snap[heartbeat_id.value].enabled ? 1u : 0u,
        static_cast<unsigned>(task_snap[heartbeat_id.value].priority));

    return 0;
}
