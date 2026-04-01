module;

#include <cstddef>

export module charm.system.app_host;

import charm.system.caps;
import charm.system.reactor_pump;
import input.pump;
import kernel.config;
import kernel.eda;
import kernel.scheduler;
import kernel.task_state;
import util.core;

export namespace charm::system {
    struct AppHostConfig : kernel::KernelConfig {
        static constexpr std::size_t priority_levels = 1;
        static constexpr std::size_t evtq_capacity = 8;
    };

    template <typename Caps = DefaultCaps,
              typename Config = AppHostConfig,
              typename... ExtraTasks>
    class AppHost {
    public:
        using PumpTask = ReactorPumpTask;
        using InputPumpTask = input::InputPumpTask;
        using Registry = kernel::TaskRegistry<PumpTask, InputPumpTask, ExtraTasks...>;
        using Scheduler = kernel::Scheduler<Config, Registry, Caps, kernel::state::Running>;

        explicit AppHost(Caps caps = {}) noexcept
            : caps_(caps),
              scheduler_(kernel::start(kernel::make_scheduler<Config>(registry_, caps_))) {}

        Registry& registry() noexcept { return registry_; }
        Scheduler& scheduler() noexcept { return scheduler_; }
        Caps& caps() noexcept { return caps_; }

        PumpTask& pump() noexcept { return registry_.template get<PumpTask>(); }
        InputPumpTask& input_pump() noexcept { return registry_.template get<InputPumpTask>(); }

        static consteval kernel::TaskId pump_id() noexcept {
            return Registry::template id_of<PumpTask>();
        }

        static consteval kernel::TaskId input_pump_id() noexcept {
            return Registry::template id_of<InputPumpTask>();
        }

        template <typename Task>
        Task& task() noexcept { return registry_.template get<Task>(); }

        template <typename Task>
        static consteval kernel::TaskId task_id() noexcept {
            return Registry::template id_of<Task>();
        }

        PostFn post_fn() noexcept { return &charm::system::scheduler_post<Scheduler>; }
        PostFn post_io_ready_fn() noexcept { return &charm::system::scheduler_post_io_ready<Scheduler>; }
        PostFn post_demand_fn() noexcept { return &charm::system::scheduler_post_demand<Scheduler>; }
        void* post_ctx() noexcept { return &scheduler_; }

        input::ScheduleFn schedule_fn() noexcept {
            return &input::scheduler_schedule_at<Scheduler>;
        }
        void* schedule_ctx() noexcept { return &scheduler_; }

        bool run_once() noexcept { return scheduler_.run_once(); }
        bool run_budget(std::size_t budget) noexcept { return scheduler_.run_budget(budget); }
        bool run_auto() noexcept { return scheduler_.run_auto(); }

    private:
        Registry registry_{};
        Caps caps_{};
        Scheduler scheduler_;
    };
}



