#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <utility>

import kernel.capabilities;
import kernel.config;
import kernel.eda;
import kernel.evt;
import kernel.ipc;
import kernel.scheduler;
import kernel.sync;
import kernel.sync_object;
import kernel.thread;
import kernel.thread_blocking;
import kernel.thread_api;
import kernel.timer;
import kernel.timer_wheel;
import kernel.wait_list;
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

    struct Config {
        static constexpr bool enable_timer = true;
        static constexpr bool enable_dynamic_priority = true;
        static constexpr std::size_t priority_levels = 3;
        static constexpr std::size_t evtq_capacity = 16;
        static constexpr std::size_t timer_capacity = 8;
        using timer_policy = kernel::HierWheelTimerPolicy<8, 2>;
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

    struct Heartbeat {
        static constexpr kernel::Priority priority{1};
        std::uint32_t ticks{0};

        void on_event(kernel::Event evt) {
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

    struct Logger {
        static constexpr kernel::Priority priority{0};

        void on_event(kernel::Event evt) {
            if (evt.id == kernel::EventId::init) {
                std::printf("[Logger] init\n");
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
            return;
        }
    }

    using BlockingTask = kernel::ThreadBlockingTask<BlockingContext, blocking_handler, kernel::Priority{0}>;
}

struct Caps {
    using TimeSource = platform::win::ManualTimeSource;
    using IrqGuard = platform::win::SpinIrqGuard;
    using Wakeup = platform::win::NoopWakeup;
    using SwiTrigger = kernel::NoopSwiTrigger;
};

int main() {
    using Registry = kernel::TaskRegistry<demo::Urgent, demo::Heartbeat, demo::Logger, demo::ThreadTask, demo::BlockingTask>;
    Registry registry{};

    Caps caps{};

    auto created = kernel::make_scheduler<demo::Config>(registry, caps);
    auto running = kernel::start(std::move(created));

    const auto urgent_id = Registry::id_of<demo::Urgent>();
    const auto heartbeat_id = Registry::id_of<demo::Heartbeat>();
    const auto logger_id = Registry::id_of<demo::Logger>();
    const auto thread_id = Registry::id_of<demo::ThreadTask>();
    const auto blocking_id = Registry::id_of<demo::BlockingTask>();

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

    (void)msg_queue.send(logger_id, 42);
    (void)sem_ipc.wait(heartbeat_id);
    (void)sync_object.pend(blocking_id);

    for (std::uint32_t i = 1; i <= 3; ++i) {
        const auto base = Caps::TimeSource::now();
        (void)running.schedule_at(base + 1000, logger_id, kernel::make_event(kernel::EventId::tick, i));
        (void)running.schedule_at(base + 1000, urgent_id, kernel::make_event(kernel::EventId::tick, i));
        (void)running.schedule_at(base + 1000, thread_id, kernel::make_event(kernel::EventId::tick, i));
        (void)running.schedule_at(base + 2000, heartbeat_id, kernel::make_event(kernel::EventId::tick, i));
        (void)thread_api.sleep(blocking_id, base + 1500);

        Caps::TimeSource::advance(1000);
        const auto t1 = Caps::TimeSource::now();
        while (running.tick(t1)) {
        }
        while (running.run_once()) {
        }

        Caps::TimeSource::advance(1000);
        const auto t2 = Caps::TimeSource::now();
        while (running.tick(t2)) {
        }
        while (running.run_once()) {
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

        Caps::TimeSource::advance(1000);
    }

    return 0;
}
