module;

#include <string_view>

export module kernel.ssu;

import kernel.evt;

export namespace kernel::ssu {
    enum class ExecutionDomain : unsigned char {
        isr_only,
        task_only,
        anywhere,
    };

    enum class TriggerKind : unsigned char {
        event,
        io_ready,
        timer,
        frame,
        demand,
    };

    enum class BudgetKind : unsigned char {
        single_step,
        budgeted,
    };

    enum class BlockingKind : unsigned char {
        non_blocking,
        may_block,
    };

    struct Meta {
        ExecutionDomain domain{ExecutionDomain::task_only};
        TriggerKind trigger{TriggerKind::event};
        BudgetKind budget{BudgetKind::single_step};
        BlockingKind blocking{BlockingKind::non_blocking};
        std::string_view name{};
    };

    template <typename T>
    concept HasStaticMeta = requires {
        { T::ssu_meta() } -> std::same_as<Meta>;
    };

    template <typename T>
    concept HasEventStep = requires(T unit, kernel::Event evt) {
        { unit.on_event(evt) } -> std::same_as<void>;
    };

    template <typename T>
    concept HasDrainStep = requires(T unit) {
        { unit.step() } -> std::convertible_to<bool>;
    };

    template <typename T>
    concept SsuUnit = HasStaticMeta<T> && (HasEventStep<T> || HasDrainStep<T>);

    template <typename T>
    struct eda_adapter {
        T* task{nullptr};

        static consteval Meta ssu_meta() noexcept {
            return Meta{
                .domain = ExecutionDomain::task_only,
                .trigger = TriggerKind::event,
                .budget = BudgetKind::single_step,
                .blocking = BlockingKind::non_blocking,
                .name = "kernel.eda",
            };
        }

        void on_event(kernel::Event evt) noexcept {
            if (task) {
                task->on_event(evt);
            }
        }
    };

    template <typename T>
    concept EdaLike = requires(T task, kernel::Event evt) {
        { task.on_event(evt) } -> std::same_as<void>;
    };

    template <EdaLike T>
    [[nodiscard]] constexpr auto as_event_unit(T& task) noexcept {
        return eda_adapter<T>{&task};
    }
}
