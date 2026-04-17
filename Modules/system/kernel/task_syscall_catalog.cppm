module;

#include <array>

export module kernel.task_syscall_catalog;

export import kernel.task_syscall_api;
import util.core;

export namespace kernel {
    enum class TaskSyscallId : util::u16 {
        invalid = 0,
        yield = static_cast<util::u16>(TrapService::yield_current),
        sleep_until = static_cast<util::u16>(TrapService::sleep_until),
        debug_write = static_cast<util::u16>(TrapService::debug_write),
        capability_call = static_cast<util::u16>(TrapService::capability_call),
    };

    [[nodiscard]] constexpr const char* task_syscall_name(
        TaskSyscallId syscall) noexcept
    {
        switch (syscall) {
        case TaskSyscallId::invalid:
            return "invalid";
        case TaskSyscallId::yield:
            return "yield";
        case TaskSyscallId::sleep_until:
            return "sleep-until";
        case TaskSyscallId::debug_write:
            return "debug-write";
        case TaskSyscallId::capability_call:
            return "capability-call";
        }
        return "unknown";
    }

    enum class TaskSyscallViewKind : util::u8 {
        invalid = 0,
        yield,
        sleep_until,
        debug_write,
        capability_call,
        opaque,
    };

    [[nodiscard]] constexpr const char* task_syscall_view_kind_name(
        TaskSyscallViewKind kind) noexcept
    {
        switch (kind) {
        case TaskSyscallViewKind::invalid:
            return "invalid";
        case TaskSyscallViewKind::yield:
            return "yield";
        case TaskSyscallViewKind::sleep_until:
            return "sleep-until";
        case TaskSyscallViewKind::debug_write:
            return "debug-write";
        case TaskSyscallViewKind::capability_call:
            return "capability-call";
        case TaskSyscallViewKind::opaque:
            return "opaque";
        }
        return "unknown";
    }

    struct TaskSyscallCatalogEntry {
        TaskSyscallId syscall{TaskSyscallId::invalid};
        const char* syscall_name{"invalid"};
        TrapService trap_service{TrapService::invalid};
        const char* trap_service_name{"invalid"};
        TaskSyscallViewKind view_kind{TaskSyscallViewKind::invalid};
        util::u8 wire_argument_count{0};
        std::array<const char*, 4> wire_argument_names{};
        const char* result_name{"value"};
        bool supported{false};
    };

    [[nodiscard]] constexpr TrapService trap_service_from_task_syscall(
        TaskSyscallId syscall) noexcept
    {
        switch (syscall) {
        case TaskSyscallId::yield:
            return TrapService::yield_current;
        case TaskSyscallId::sleep_until:
            return TrapService::sleep_until;
        case TaskSyscallId::debug_write:
            return TrapService::debug_write;
        case TaskSyscallId::capability_call:
            return TrapService::capability_call;
        case TaskSyscallId::invalid:
            return TrapService::invalid;
        }
        return TrapService::invalid;
    }

    [[nodiscard]] constexpr TaskSyscallId task_syscall_from_trap_service(
        TrapService service) noexcept
    {
        switch (service) {
        case TrapService::yield_current:
            return TaskSyscallId::yield;
        case TrapService::sleep_until:
            return TaskSyscallId::sleep_until;
        case TrapService::debug_write:
            return TaskSyscallId::debug_write;
        case TrapService::capability_call:
            return TaskSyscallId::capability_call;
        case TrapService::invalid:
            return TaskSyscallId::invalid;
        }
        return TaskSyscallId::invalid;
    }

    [[nodiscard]] constexpr TaskSyscallCatalogEntry task_syscall_catalog_entry(
        TaskSyscallId syscall) noexcept
    {
        switch (syscall) {
        case TaskSyscallId::invalid:
            return TaskSyscallCatalogEntry{
                .syscall = syscall,
                .syscall_name = "invalid",
                .trap_service = TrapService::invalid,
                .trap_service_name = "invalid",
                .view_kind = TaskSyscallViewKind::invalid,
                .wire_argument_count = 0,
                .wire_argument_names = {},
                .result_name = "value",
                .supported = false,
            };
        case TaskSyscallId::yield: {
            const auto trap_entry =
                trap_service_catalog_entry(TrapService::yield_current);
            return TaskSyscallCatalogEntry{
                .syscall = syscall,
                .syscall_name = "yield",
                .trap_service = trap_entry.service,
                .trap_service_name = trap_entry.service_name,
                .view_kind = TaskSyscallViewKind::yield,
                .wire_argument_count = trap_entry.wire_argument_count,
                .wire_argument_names = trap_entry.wire_argument_names,
                .result_name = trap_entry.result_name,
                .supported = trap_entry.supported,
            };
        }
        case TaskSyscallId::sleep_until: {
            const auto trap_entry =
                trap_service_catalog_entry(TrapService::sleep_until);
            return TaskSyscallCatalogEntry{
                .syscall = syscall,
                .syscall_name = "sleep-until",
                .trap_service = trap_entry.service,
                .trap_service_name = trap_entry.service_name,
                .view_kind = TaskSyscallViewKind::sleep_until,
                .wire_argument_count = trap_entry.wire_argument_count,
                .wire_argument_names = trap_entry.wire_argument_names,
                .result_name = trap_entry.result_name,
                .supported = trap_entry.supported,
            };
        }
        case TaskSyscallId::debug_write: {
            const auto trap_entry =
                trap_service_catalog_entry(TrapService::debug_write);
            return TaskSyscallCatalogEntry{
                .syscall = syscall,
                .syscall_name = "debug-write",
                .trap_service = trap_entry.service,
                .trap_service_name = trap_entry.service_name,
                .view_kind = TaskSyscallViewKind::debug_write,
                .wire_argument_count = trap_entry.wire_argument_count,
                .wire_argument_names = trap_entry.wire_argument_names,
                .result_name = trap_entry.result_name,
                .supported = trap_entry.supported,
            };
        }
        case TaskSyscallId::capability_call: {
            const auto trap_entry =
                trap_service_catalog_entry(TrapService::capability_call);
            return TaskSyscallCatalogEntry{
                .syscall = syscall,
                .syscall_name = "capability-call",
                .trap_service = trap_entry.service,
                .trap_service_name = trap_entry.service_name,
                .view_kind = TaskSyscallViewKind::capability_call,
                .wire_argument_count = trap_entry.wire_argument_count,
                .wire_argument_names = trap_entry.wire_argument_names,
                .result_name = trap_entry.result_name,
                .supported = trap_entry.supported,
            };
        }
        }

        return TaskSyscallCatalogEntry{
            .syscall = syscall,
            .syscall_name = "unknown",
            .trap_service = TrapService::invalid,
            .trap_service_name = "invalid",
            .view_kind = TaskSyscallViewKind::opaque,
            .wire_argument_count = 0,
            .wire_argument_names = {},
            .result_name = "value",
            .supported = false,
        };
    }

    [[nodiscard]] constexpr TaskSyscallCatalogEntry task_syscall_catalog_entry(
        TrapService service) noexcept
    {
        switch (task_syscall_from_trap_service(service)) {
        case TaskSyscallId::yield:
            return task_syscall_catalog_entry(TaskSyscallId::yield);
        case TaskSyscallId::sleep_until:
            return task_syscall_catalog_entry(TaskSyscallId::sleep_until);
        case TaskSyscallId::debug_write:
            return task_syscall_catalog_entry(TaskSyscallId::debug_write);
        case TaskSyscallId::capability_call:
            return task_syscall_catalog_entry(TaskSyscallId::capability_call);
        case TaskSyscallId::invalid:
            break;
        }

        if (service == TrapService::invalid) {
            return task_syscall_catalog_entry(TaskSyscallId::invalid);
        }

        const auto trap_entry = trap_service_catalog_entry(service);
        return TaskSyscallCatalogEntry{
            .syscall = TaskSyscallId::invalid,
            .syscall_name = "unmapped",
            .trap_service = service,
            .trap_service_name = trap_entry.service_name,
            .view_kind = TaskSyscallViewKind::opaque,
            .wire_argument_count = trap_entry.wire_argument_count,
            .wire_argument_names = trap_entry.wire_argument_names,
            .result_name = trap_entry.result_name,
            .supported = false,
        };
    }

    using TaskSyscallSemanticField = TrapSemanticField;

    struct TaskSyscallSemanticProjection {
        TaskSyscallCatalogEntry descriptor{};
        std::array<TaskSyscallSemanticField, 4> fields{};
        util::u8 field_count{0};
        const char* result_name{"value"};
    };

    [[nodiscard]] constexpr TaskSyscallSemanticProjection
    task_syscall_semantic_projection(const TrapRequest& request) noexcept
    {
        const auto trap_projection = trap_semantic_projection(request);
        return TaskSyscallSemanticProjection{
            .descriptor = task_syscall_catalog_entry(request.service),
            .fields = trap_projection.fields,
            .field_count = trap_projection.field_count,
            .result_name = trap_projection.result_name,
        };
    }

    template <typename Tick>
    [[nodiscard]] constexpr TaskSyscallSemanticProjection
    task_syscall_semantic_projection(
        const RuntimeTrapTraceEvent<Tick>& event) noexcept
    {
        return task_syscall_semantic_projection(
            trap_request_from_trace_event(event));
    }
}
