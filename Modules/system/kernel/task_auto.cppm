module;

// Optional/experimental module: auto register tasks by Config.
#include <array>
#include <cstddef>
#include <optional>
#include <tuple>
#include <type_traits>

export module kernel.task_auto;

import kernel.config;
import kernel.dynamic_registry;
import kernel.eda;
import util.core;

export namespace kernel {
    template <typename Config>
    consteval bool task_enabled_for_config() {
        return true;
    }

    template <typename Config, typename T>
    consteval bool task_enabled_for_config() {
        if constexpr (requires { Config::template enable_task<T>(); }) {
            return Config::template enable_task<T>();
        } else {
            return true;
        }
    }

    template <typename Config, typename... Tasks, std::size_t Capacity>
    [[nodiscard]] auto register_enabled(
        DynamicTaskRegistry<Capacity>& registry,
        std::tuple<Tasks...>& tasks) {
        static_assert(sizeof...(Tasks) <= Capacity);
        std::array<std::optional<TaskId>, sizeof...(Tasks)> ids{};
        register_enabled_impl<Config>(registry, tasks, ids, std::index_sequence_for<Tasks...>{});
        return ids;
    }

    template <typename Config, typename... Tasks, std::size_t Capacity, std::size_t... I>
    void register_enabled_impl(
        DynamicTaskRegistry<Capacity>& registry,
        std::tuple<Tasks...>& tasks,
        std::array<std::optional<TaskId>, sizeof...(Tasks)>& out,
        std::index_sequence<I...>) {
        (register_one<Config, std::remove_reference_t<std::tuple_element_t<I, std::tuple<Tasks...>>>>(
            registry, std::get<I>(tasks), out[I]), ...);
    }

    template <typename Config, typename T, std::size_t Capacity>
    void register_one(
        DynamicTaskRegistry<Capacity>& registry,
        T& task,
        std::optional<TaskId>& out) {
        if constexpr (!task_enabled_for_config<Config, T>()) {
            out = std::nullopt;
            return;
        }
        if constexpr (requires { T::priority; }) {
            out = registry.register_task(task, T::priority);
        } else {
            out = std::nullopt;
        }
    }
}
