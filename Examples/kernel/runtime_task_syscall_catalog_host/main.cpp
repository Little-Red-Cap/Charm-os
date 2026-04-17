#include <cstdint>
#include <cstdio>
#include <string_view>

import kernel.task_syscall_catalog;
import kernel.eda;

namespace demo {
    using namespace std::literals;

    [[nodiscard]] constexpr bool same_text(const char* actual,
                                           std::string_view expected) noexcept
    {
        return actual != nullptr && std::string_view{actual} == expected;
    }

    [[nodiscard]] constexpr bool probe_round_trip_mapping() noexcept
    {
        return kernel::trap_service_from_task_syscall(
                   kernel::TaskSyscallId::yield) ==
                   kernel::TrapService::yield_current &&
               kernel::trap_service_from_task_syscall(
                   kernel::TaskSyscallId::sleep_until) ==
                   kernel::TrapService::sleep_until &&
               kernel::trap_service_from_task_syscall(
                   kernel::TaskSyscallId::debug_write) ==
                   kernel::TrapService::debug_write &&
               kernel::trap_service_from_task_syscall(
                   kernel::TaskSyscallId::capability_call) ==
                   kernel::TrapService::capability_call &&
               kernel::task_syscall_from_trap_service(
                   kernel::TrapService::yield_current) ==
                   kernel::TaskSyscallId::yield &&
               kernel::task_syscall_from_trap_service(
                   kernel::TrapService::sleep_until) ==
                   kernel::TaskSyscallId::sleep_until &&
               kernel::task_syscall_from_trap_service(
                   kernel::TrapService::debug_write) ==
                   kernel::TaskSyscallId::debug_write &&
               kernel::task_syscall_from_trap_service(
                   kernel::TrapService::capability_call) ==
                   kernel::TaskSyscallId::capability_call &&
               kernel::trap_service_from_task_syscall(
                   static_cast<kernel::TaskSyscallId>(99u)) ==
                   kernel::TrapService::invalid &&
               kernel::task_syscall_from_trap_service(
                   static_cast<kernel::TrapService>(99u)) ==
                   kernel::TaskSyscallId::invalid;
    }

    static_assert(probe_round_trip_mapping());

    [[nodiscard]] bool probe_catalog_alignment() noexcept
    {
        const auto mapped_yield =
            kernel::task_syscall_catalog_entry(kernel::TaskSyscallId::yield);
        const auto mapped_capability = kernel::task_syscall_catalog_entry(
            kernel::TaskSyscallId::capability_call);
        const auto mapped_from_service = kernel::task_syscall_catalog_entry(
            kernel::TrapService::yield_current);
        const auto capability_service = kernel::trap_service_catalog_entry(
            kernel::TrapService::capability_call);
        const auto unknown_syscall =
            kernel::task_syscall_catalog_entry(
                static_cast<kernel::TaskSyscallId>(99u));
        const auto unknown_service =
            kernel::task_syscall_catalog_entry(
                static_cast<kernel::TrapService>(99u));

        return same_text(kernel::task_syscall_name(kernel::TaskSyscallId::yield),
                         "yield"sv) &&
               same_text(kernel::task_syscall_view_kind_name(
                             kernel::TaskSyscallViewKind::sleep_until),
                         "sleep-until"sv) &&
               mapped_yield.syscall == kernel::TaskSyscallId::yield &&
               same_text(mapped_yield.syscall_name, "yield"sv) &&
               mapped_yield.trap_service ==
                   kernel::TrapService::yield_current &&
               same_text(mapped_yield.trap_service_name,
                         "yield-current"sv) &&
               mapped_yield.view_kind ==
                   kernel::TaskSyscallViewKind::yield &&
               mapped_yield.wire_argument_count == 0u &&
               same_text(mapped_yield.result_name, "accepted"sv) &&
               mapped_yield.supported &&
               mapped_from_service.syscall == mapped_yield.syscall &&
               mapped_from_service.trap_service ==
                   mapped_yield.trap_service &&
               same_text(mapped_from_service.syscall_name, "yield"sv) &&
               mapped_capability.wire_argument_count ==
                   capability_service.wire_argument_count &&
               same_text(mapped_capability.wire_argument_names[0],
                         "capability-id"sv) &&
               same_text(mapped_capability.wire_argument_names[1],
                         "operation"sv) &&
               same_text(mapped_capability.wire_argument_names[2],
                         "payload"sv) &&
               same_text(mapped_capability.result_name, "result"sv) &&
               mapped_capability.supported &&
               unknown_syscall.syscall ==
                   static_cast<kernel::TaskSyscallId>(99u) &&
               same_text(unknown_syscall.syscall_name, "unknown"sv) &&
               unknown_syscall.trap_service == kernel::TrapService::invalid &&
               unknown_syscall.view_kind ==
                   kernel::TaskSyscallViewKind::opaque &&
               !unknown_syscall.supported &&
               unknown_service.syscall == kernel::TaskSyscallId::invalid &&
               same_text(unknown_service.syscall_name, "unmapped"sv) &&
               unknown_service.trap_service ==
                   static_cast<kernel::TrapService>(99u) &&
               same_text(unknown_service.trap_service_name, "unknown"sv) &&
               unknown_service.view_kind ==
                   kernel::TaskSyscallViewKind::opaque &&
               !unknown_service.supported;
    }

    [[nodiscard]] bool probe_semantic_projection() noexcept
    {
        const auto yielded = kernel::task_syscall_semantic_projection(
            kernel::make_yield_current_trap_request(
                kernel::TrapYieldCurrentView{},
                kernel::TrapOrigin::user_task));
        const auto slept = kernel::task_syscall_semantic_projection(
            kernel::make_sleep_until_trap_request<std::uint64_t>(
                kernel::TrapSleepUntilView<std::uint64_t>{
                    .due = 64u,
                },
                kernel::TrapOrigin::user_task));
        const kernel::RuntimeTrapTraceEvent<std::uint64_t> capability_event{
            .stamp = 7u,
            .service = kernel::TrapService::capability_call,
            .origin = kernel::TrapOrigin::user_task,
            .task = kernel::TaskId{3u},
            .task_valid = true,
            .disposition = kernel::TrapDisposition::handled,
            .error = kernel::TrapError::none,
            .arg0 = 7u,
            .arg1 = 2u,
            .arg2 = 33u,
            .value = 42u,
        };
        const auto capability =
            kernel::task_syscall_semantic_projection(capability_event);

        return yielded.descriptor.syscall == kernel::TaskSyscallId::yield &&
               same_text(yielded.descriptor.syscall_name, "yield"sv) &&
               yielded.field_count == 0u &&
               same_text(yielded.result_name, "accepted"sv) &&
               slept.descriptor.syscall ==
                   kernel::TaskSyscallId::sleep_until &&
               slept.field_count == 1u &&
               same_text(slept.fields[0].name, "due"sv) &&
               slept.fields[0].value == 64u &&
               same_text(slept.result_name, "due"sv) &&
               capability.descriptor.syscall ==
                   kernel::TaskSyscallId::capability_call &&
               same_text(capability.descriptor.syscall_name,
                         "capability-call"sv) &&
               capability.field_count == 3u &&
               same_text(capability.fields[0].name, "capability-id"sv) &&
               capability.fields[0].value == 7u &&
               same_text(capability.fields[1].name, "operation"sv) &&
               capability.fields[1].value == 2u &&
               same_text(capability.fields[2].name, "payload"sv) &&
               capability.fields[2].value == 33u &&
               same_text(capability.result_name, "result"sv);
    }
}

int main()
{
    constexpr bool mapping_ok = demo::probe_round_trip_mapping();
    const bool catalog_ok = demo::probe_catalog_alignment();
    const bool projection_ok = demo::probe_semantic_projection();
    const bool ok = mapping_ok && catalog_ok && projection_ok;

    std::printf(
        "[runtime-task-syscall-catalog-demo] ok=%d mapping=%d catalog=%d projection=%d\n",
        ok ? 1 : 0,
        mapping_ok ? 1 : 0,
        catalog_ok ? 1 : 0,
        projection_ok ? 1 : 0);
    return ok ? 0 : 1;
}
