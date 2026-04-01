module;

#include <array>
#include <cstddef>
#include <compare>
#include <tuple>
#include <string_view>
#include <type_traits>

export module kernel.eda;

import util.core;
import util.type_list;
import kernel.evt;
import kernel.ssu;

export namespace kernel {
    struct Priority {
        std::size_t value{0};
        constexpr bool operator==(const Priority&) const = default;
        constexpr bool operator<(const Priority& other) const noexcept { return value < other.value; }
    };

    struct TaskId {
        std::size_t value{0};
        constexpr bool operator==(const TaskId&) const = default;
        constexpr bool operator<(const TaskId& other) const noexcept { return value < other.value; }
    };

    template <typename Task>
    concept EdaTask = requires(Task task, Event evt) {
        { Task::priority } -> std::convertible_to<Priority>;
        { task.on_event(evt) } -> std::same_as<void>;
    };

    template <typename Task>
    concept EdaTaskWithSsu = EdaTask<Task> && requires {
        { Task::ssu_meta() } -> std::same_as<kernel::ssu::Meta>;
    };

    constexpr bool require_ssu_meta() noexcept {
    #ifdef CHARM_KERNEL_REQUIRE_SSU_META
        return CHARM_KERNEL_REQUIRE_SSU_META != 0;
    #else
        return false;
    #endif
    }

    template <typename Task>
    constexpr EventMask task_mask() noexcept {
        if constexpr (requires { Task::mask; }) {
            return Task::mask;
        } else {
            return EventMask{0xFFFF'FFFFu};
        }
    }

    template <typename Task, typename Config>
    consteval void validate_task_priority() {
        static_assert(Task::priority.value < Config::priority_levels);
    }

    template <typename Task>
    consteval void validate_task_ssu() {
        if constexpr (require_ssu_meta()) {
            static_assert(EdaTaskWithSsu<Task>, "kernel task must declare ssu_meta() when CHARM_KERNEL_REQUIRE_SSU_META is enabled");
        }
    }

    template <typename... Tasks>
    struct TaskRegistry {
        std::tuple<Tasks...> tasks{};
        static constexpr std::size_t count = sizeof...(Tasks);

        template <typename T>
        static consteval TaskId id_of() {
            using list = util::type_list<Tasks...>;
            return TaskId{util::index_of<T, list>::value};
        }

        template <typename Config>
        static consteval auto priority_table() {
            (validate_task_priority<Tasks, Config>(), ...);
            (validate_task_ssu<Tasks>(), ...);
            return std::array<std::size_t, count>{Tasks::priority.value...};
        }

        template <typename Config>
        static consteval auto active_table() {
            if constexpr (requires { Config::template enable_task<std::tuple_element_t<0, std::tuple<Tasks...>>>(); }) {
                return std::array<bool, count>{Config::template enable_task<Tasks>()...};
            } else {
                return std::array<bool, count>{((void)sizeof(Tasks), true)...};
            }
        }

        template <typename T>
        T& get() {
            return std::get<T>(tasks);
        }

        template <typename Config>
        void fill_priorities(std::array<std::size_t, count>& out) const noexcept {
            out = priority_table<Config>();
        }

        template <typename Config>
        [[nodiscard]] bool is_active(TaskId id) const noexcept {
            if (id.value >= count) {
                return false;
            }
            constexpr auto table = active_table<Config>();
            return table[id.value];
        }

        [[nodiscard]] std::string_view task_ssu_name(TaskId id) const noexcept {
            if (id.value >= count) {
                return {};
            }
            return task_ssu_name_by_index<0>(id.value);
        }

        void init_all() {
            (init_one<Tasks>(), ...);
        }

        void start(TaskId id) {
            if (id.value >= count) {
                util::halt();
            }
            start_by_index<0>(id.value);
        }

        void stop(TaskId id) {
            if (id.value >= count) {
                util::halt();
            }
            stop_by_index<0>(id.value);
        }

        void dispatch(TaskId id, Event evt) {
            if (id.value >= count) {
                util::halt();
            }
            if (!accept_event(id, evt)) {
                return;
            }
            dispatch_by_index<0>(id.value, evt);
        }

    private:
        template <typename T>
        void init_one() {
            if constexpr (requires(T& t) { t.on_start(); }) {
                std::get<T>(tasks).on_start();
            }
        }

        template <typename T>
        void start_one() {
            if constexpr (requires(T& t) { t.on_start(); }) {
                std::get<T>(tasks).on_start();
            }
        }

        template <typename T>
        void stop_one() {
            if constexpr (requires(T& t) { t.on_stop(); }) {
                std::get<T>(tasks).on_stop();
            }
        }

        template <std::size_t I>
        void start_by_index(std::size_t index) {
            if (index == I) {
                start_one<std::tuple_element_t<I, std::tuple<Tasks...>>>();
                return;
            }
            if constexpr (I + 1 < count) {
                start_by_index<I + 1>(index);
            }
        }

        template <std::size_t I>
        void stop_by_index(std::size_t index) {
            if (index == I) {
                stop_one<std::tuple_element_t<I, std::tuple<Tasks...>>>();
                return;
            }
            if constexpr (I + 1 < count) {
                stop_by_index<I + 1>(index);
            }
        }

        template <std::size_t I>
        void dispatch_by_index(std::size_t index, Event evt) {
            if (index == I) {
                std::get<I>(tasks).on_event(evt);
                return;
            }
            if constexpr (I + 1 < count) {
                dispatch_by_index<I + 1>(index, evt);
            }
        }

        template <std::size_t I>
        bool accept_event_by_index(std::size_t index, Event evt) const {
            if (index == I) {
                using Task = std::tuple_element_t<I, std::tuple<Tasks...>>;
                const auto mask = task_mask<Task>();
                if (evt.id == EventId::terminate) {
                    return true;
                }
                if ((mask & event_mask(evt.id)) == 0) {
                    return false;
                }
                if constexpr (requires(Task& t, Event e) { t.should_accept(e); }) {
                    return std::get<I>(tasks).should_accept(evt);
                }
                return true;
            }
            if constexpr (I + 1 < count) {
                return accept_event_by_index<I + 1>(index, evt);
            } else {
                return false;
            }
        }

        template <std::size_t I>
        [[nodiscard]] static consteval std::string_view task_ssu_name_for() noexcept {
            using Task = std::tuple_element_t<I, std::tuple<Tasks...>>;
            if constexpr (requires { Task::ssu_meta(); }) {
                return Task::ssu_meta().name;
            } else {
                return {};
            }
        }

        template <std::size_t I>
        [[nodiscard]] std::string_view task_ssu_name_by_index(std::size_t index) const noexcept {
            if (index == I) {
                return task_ssu_name_for<I>();
            }
            if constexpr (I + 1 < count) {
                return task_ssu_name_by_index<I + 1>(index);
            } else {
                return {};
            }
        }

        bool accept_event(TaskId id, Event evt) const {
            return accept_event_by_index<0>(id.value, evt);
        }
    };
}
