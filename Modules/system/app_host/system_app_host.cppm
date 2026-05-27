module;

#include <cstddef>

export module charm.system.app_host;

import charm.system.caps;
import charm.system.reactor_pump;
import charm.system.schedule_ref;
import input.pump;
import kernel.config;
import kernel.eda;
import kernel.poster;
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

        [[nodiscard]] auto posters(kernel::TaskId task) noexcept {
            return kernel::make_poster_set(scheduler_, task);
        }

        [[nodiscard]] auto event_poster(kernel::TaskId task) noexcept {
            return posters(task).event;
        }

        [[nodiscard]] auto io_ready_poster(kernel::TaskId task) noexcept {
            return posters(task).io_ready;
        }

        [[nodiscard]] auto demand_poster(kernel::TaskId task) noexcept {
            return posters(task).demand;
        }

        [[nodiscard]] PostRef post_ref() noexcept {
            return PostRef::raw(&charm::system::scheduler_post<Scheduler>, &scheduler_);
        }

        [[nodiscard]] PostRef post_io_ready_ref() noexcept {
            return PostRef::raw(&charm::system::scheduler_post_io_ready<Scheduler>, &scheduler_);
        }

        [[nodiscard]] PostRef post_demand_ref() noexcept {
            return PostRef::raw(&charm::system::scheduler_post_demand<Scheduler>, &scheduler_);
        }

        [[nodiscard]] ReactorPumpPosts reactor_pump_posts() noexcept {
            return ReactorPumpPosts{
                .wake = post_io_ready_ref(),
                .more = post_demand_ref(),
            };
        }

        template <typename Task>
        [[nodiscard]] auto posters() noexcept {
            return posters(task_id<Task>());
        }

        template <typename Task>
        [[nodiscard]] auto event_poster() noexcept {
            return posters<Task>().event;
        }

        template <typename Task>
        [[nodiscard]] auto io_ready_poster() noexcept {
            return posters<Task>().io_ready;
        }

        template <typename Task>
        [[nodiscard]] auto demand_poster() noexcept {
            return posters<Task>().demand;
        }

        [[nodiscard]] ScheduleRef schedule_ref() noexcept {
            return ScheduleRef::raw(&charm::system::scheduler_schedule_at<Scheduler>, &scheduler_);
        }

        [[nodiscard]] input::InputPumpPorts input_pump_ports() noexcept {
            return input::InputPumpPorts{
                .schedule = schedule_ref(),
                .post_more = post_demand_ref(),
            };
        }

        std::size_t dispatch_batch(std::size_t budget) noexcept { return scheduler_.dispatch_batch(budget); }
        bool run_once() noexcept { return scheduler_.run_once(); }
        bool run_budget(std::size_t budget) noexcept { return scheduler_.run_budget(budget); }
        bool run_auto() noexcept { return scheduler_.run_auto(); }

    private:
        Registry registry_{};
        Caps caps_{};
        Scheduler scheduler_;
    };
}



