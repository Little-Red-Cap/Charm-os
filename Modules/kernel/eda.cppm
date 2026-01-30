module;

#include <array>
#include <cstddef>
#include <tuple>
#include <type_traits>

export module kernel.eda;

import util.core;
import util.type_list;
import kernel.evt;

export namespace kernel {
    struct Priority {
        std::size_t value{0};
        constexpr auto operator<=>(const Priority&) const = default;
    };

    struct TaskId {
        std::size_t value{0};
        constexpr auto operator<=>(const TaskId&) const = default;
    };

    template <typename Task>
    concept EdaTask = requires(Task task, Event evt) {
        { Task::priority } -> std::convertible_to<Priority>;
        { task.on_event(evt) } -> std::same_as<void>;
    };

    template <typename Task, typename Config>
    consteval void validate_task_priority() {
        static_assert(Task::priority.value < Config::priority_levels);
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
            return std::array<std::size_t, count>{Tasks::priority.value...};
        }

        template <typename T>
        T& get() {
            return std::get<T>(tasks);
        }

        template <typename Config>
        void fill_priorities(std::array<std::size_t, count>& out) const noexcept {
            out = priority_table<Config>();
        }

        [[nodiscard]] bool is_active(TaskId) const noexcept {
            return true;
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
    };
}
