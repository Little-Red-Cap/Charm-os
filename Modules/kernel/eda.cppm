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

        void init_all() {
            (init_one<Tasks>(), ...);
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
