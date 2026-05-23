#include <array>
#include <cstdint>
#include <cstdio>
#include <string_view>

import kernel.task_syscall_table;
import semantic.core;

namespace demo {
    using namespace std::literals;

    struct FakeDispatchSurfaceState {
        bool bound{true};
        std::uint32_t yield_calls{0};
        std::uint32_t sleep_calls{0};
        std::uint32_t debug_calls{0};
        std::uint32_t capability_calls{0};
        std::uint64_t last_due{0};
        std::uint64_t last_capability_id{0};
        std::uint64_t last_capability_operation{0};
        std::uint64_t last_capability_payload{0};
    };

    struct FakeDispatchSurface {
        using tick_type = std::uint64_t;

        FakeDispatchSurfaceState* state{nullptr};

        [[nodiscard]] bool valid() const noexcept
        {
            return state != nullptr && state->bound;
        }

        [[nodiscard]] kernel::TrapResult yield_current(
            kernel::TrapYieldCurrentView) const noexcept
        {
            if (!valid()) {
                return unbound_bridge_result();
            }

            ++state->yield_calls;
            return handled_result(1u);
        }

        [[nodiscard]] kernel::TrapResult sleep_current_until(
            kernel::TrapSleepUntilView<tick_type> sleep) const noexcept
        {
            if (!valid()) {
                return unbound_bridge_result();
            }

            ++state->sleep_calls;
            state->last_due = sleep.due;
            return handled_result(sleep.due);
        }

        [[nodiscard]] kernel::TrapResult debug_write(
            kernel::TrapDebugWriteView) const noexcept
        {
            if (!valid()) {
                return unbound_bridge_result();
            }

            ++state->debug_calls;
            return handled_result(0u);
        }

        [[nodiscard]] kernel::TrapResult capability_call(
            kernel::TrapCapabilityCallView capability) const noexcept
        {
            if (!valid()) {
                return unbound_bridge_result();
            }

            ++state->capability_calls;
            state->last_capability_id = capability.capability_id;
            state->last_capability_operation = capability.operation;
            state->last_capability_payload = capability.payload;
            return handled_result(capability.capability_id +
                                  capability.operation +
                                  capability.payload);
        }

    private:
        [[nodiscard]] static constexpr kernel::TrapResult handled_result(
            std::uint64_t value) noexcept
        {
            return kernel::TrapResult{
                .disposition = kernel::TrapDisposition::handled,
                .error = kernel::TrapError::none,
                .value = value,
            };
        }

        [[nodiscard]] static constexpr kernel::TrapResult unbound_bridge_result()
            noexcept
        {
            return kernel::TrapResult{
                .disposition = kernel::TrapDisposition::rejected,
                .error = kernel::TrapError::unbound_bridge,
                .value = 0,
            };
        }
    };

    struct DirectDebugHandlerState {
        std::uint32_t calls{0};
        std::uint64_t last_value{0};
    };

    struct DirectDebugHandler {
        DirectDebugHandlerState* state{nullptr};

        [[nodiscard]] kernel::TrapResult dispatch(
            kernel::TaskSyscallRequest request) const noexcept
        {
            if (state == nullptr) {
                return kernel::TrapResult{
                    .disposition = kernel::TrapDisposition::rejected,
                    .error = kernel::TrapError::unbound_adapter,
                    .value = 0,
                };
            }

            if (request.syscall != kernel::TaskSyscallId::debug_write) {
                return kernel::TrapResult{
                    .disposition = kernel::TrapDisposition::rejected,
                    .error = kernel::TrapError::invalid_argument,
                    .value = 0,
                };
            }

            ++state->calls;
            state->last_value = request.arg0;
            return kernel::TrapResult{
                .disposition = kernel::TrapDisposition::handled,
                .error = kernel::TrapError::none,
                .value = request.arg0,
            };
        }
    };

    using DispatchTrace = kernel::TaskSyscallDispatchTraceBuffer<8>;
    using DispatchBridge =
        kernel::TaskSyscallDispatcher<FakeDispatchSurface, DispatchTrace>;
    using TableTrace = kernel::TaskSyscallTableTraceBuffer<8>;
    using StaticTable = kernel::TaskSyscallTable<4, TableTrace>;

    [[nodiscard]] constexpr bool same_text(const char* actual,
                                           std::string_view expected) noexcept
    {
        return actual != nullptr && std::string_view{actual} == expected;
    }

    [[nodiscard]] constexpr bool trap_result_matches(
        const kernel::TrapResult& result,
        kernel::TrapDisposition disposition,
        kernel::TrapError error,
        std::uint64_t value = 0u) noexcept
    {
        return result.disposition == disposition && result.error == error &&
               result.value == value;
    }

    [[nodiscard]] bool probe_lookup_and_table_dispatch() noexcept
    {
        FakeDispatchSurfaceState dispatch_state{};
        DirectDebugHandlerState debug_state{};
        DispatchTrace dispatch_trace{};
        TableTrace table_trace{};
        DispatchBridge bridge{FakeDispatchSurface{
                                  .state = &dispatch_state,
                              },
                              &dispatch_trace};
        DirectDebugHandler debug_handler{
            .state = &debug_state,
        };
        auto table = kernel::make_task_syscall_table(
            std::array<kernel::TaskSyscallHandlerEntry, 4>{
                kernel::task_syscall_handler_entry(
                    kernel::TaskSyscallId::yield,
                    kernel::make_task_syscall_handler(bridge)),
                kernel::task_syscall_handler_entry(
                    kernel::TaskSyscallId::sleep_until,
                    kernel::make_task_syscall_handler(bridge)),
                kernel::task_syscall_handler_entry(
                    kernel::TaskSyscallId::debug_write,
                    kernel::make_task_syscall_handler(debug_handler)),
                kernel::task_syscall_handler_entry(
                    kernel::TaskSyscallId::capability_call,
                    kernel::make_task_syscall_handler(bridge)),
            },
            &table_trace);

        const auto yield_slot = table.lookup(kernel::TaskSyscallId::yield);
        const auto debug_slot = table.lookup(kernel::TaskSyscallId::debug_write);
        const auto yielded = table.yield();
        const auto slept = table.sleep_until(55u);
        const auto debugged = table.debug_write(0xCCu);
        const auto called = table.dispatch(kernel::TaskSyscallRequest{
            .syscall = kernel::TaskSyscallId::capability_call,
            .arg0 = 7u,
            .arg1 = 2u,
            .arg2 = 33u,
        });

        const auto* first = table_trace.at(0u);
        const auto* third = table_trace.at(2u);
        const auto* fourth = table_trace.at(3u);
        const auto* dispatch_first = dispatch_trace.at(0u);
        const auto* dispatch_third = dispatch_trace.at(2u);
        if (first == nullptr || third == nullptr || fourth == nullptr ||
            dispatch_first == nullptr || dispatch_third == nullptr) {
            return false;
        }

        const auto third_projection =
            kernel::task_syscall_semantic_projection(*third);
        const auto fourth_witness = kernel::task_syscall_table_witness(table_trace);
        const auto third_witness = kernel::task_syscall_table_witness(*third);
        const auto fourth_handoff =
            kernel::task_syscall_table_witness_handoff_target(fourth_witness);

        return yield_slot.matched && yield_slot.slot == 0u &&
               yield_slot.entry != nullptr &&
               debug_slot.matched && debug_slot.slot == 2u &&
               debug_slot.entry != nullptr &&
               trap_result_matches(yielded,
                                   kernel::TrapDisposition::handled,
                                   kernel::TrapError::none,
                                   1u) &&
               trap_result_matches(slept,
                                   kernel::TrapDisposition::handled,
                                   kernel::TrapError::none,
                                   55u) &&
               trap_result_matches(debugged,
                                   kernel::TrapDisposition::handled,
                                   kernel::TrapError::none,
                                   0xCCu) &&
               trap_result_matches(called,
                                   kernel::TrapDisposition::handled,
                                   kernel::TrapError::none,
                                   42u) &&
               dispatch_state.yield_calls == 1u &&
               dispatch_state.sleep_calls == 1u &&
               dispatch_state.debug_calls == 0u &&
               dispatch_state.capability_calls == 1u &&
               dispatch_state.last_due == 55u &&
               dispatch_state.last_capability_id == 7u &&
               dispatch_state.last_capability_operation == 2u &&
               dispatch_state.last_capability_payload == 33u &&
               debug_state.calls == 1u &&
               debug_state.last_value == 0xCCu &&
               dispatch_trace.size() == 3u &&
               table_trace.size() == 4u &&
               first->matched && first->handler_valid &&
               first->slot == 0u &&
               first->trap_service == kernel::TrapService::yield_current &&
               dispatch_first->syscall == kernel::TaskSyscallId::yield &&
               third->matched && third->handler_valid &&
               third->slot == 2u &&
               third->trap_service == kernel::TrapService::debug_write &&
               same_text(third_projection.descriptor.syscall_name,
                         "debug-write"sv) &&
               third_projection.field_count == 1u &&
               same_text(third_projection.fields[0].name, "value"sv) &&
               third_projection.fields[0].value == 0xCCu &&
               fourth->matched && fourth->handler_valid &&
               fourth->slot == 3u &&
               dispatch_third->syscall ==
                   kernel::TaskSyscallId::capability_call &&
               kernel::task_syscall_table_witness_ready(fourth_witness) &&
               fourth_witness.ok() &&
               fourth_witness.verdict() == semantic::Verdict::standing &&
               fourth_witness.result() == semantic::Result::ok &&
               fourth_witness.failure_domain() ==
                   semantic::FailureDomain::none &&
               third_witness.verdict() == semantic::Verdict::standing &&
               std::string_view{fourth_handoff.entry_name()} ==
                   "task-syscall-table-witness"sv &&
               std::string_view{fourth_handoff.selected_summary_path()} ==
                   fourth_witness.summary_path();
    }

    [[nodiscard]] bool probe_unbound_and_missing_entry() noexcept
    {
        TableTrace trace{};
        auto table = kernel::make_task_syscall_table(
            std::array<kernel::TaskSyscallHandlerEntry, 2>{
                kernel::task_syscall_handler_entry(kernel::TaskSyscallId::yield),
                kernel::task_syscall_handler_entry(
                    kernel::TaskSyscallId::debug_write),
            },
            &trace);

        const auto yield_lookup = table.lookup(kernel::TaskSyscallId::yield);
        const auto sleep_lookup =
            table.lookup(kernel::TaskSyscallId::sleep_until);
        const auto unbound = table.yield();
        const auto missing = table.sleep_until(12u);

        const auto* first = trace.at(0u);
        const auto* second = trace.at(1u);
        if (first == nullptr || second == nullptr) {
            return false;
        }

        const auto second_projection =
            kernel::task_syscall_semantic_projection(*second);
        const auto first_witness = kernel::task_syscall_table_witness(*first);
        const auto second_witness = kernel::task_syscall_table_witness(trace);

        return yield_lookup.matched && yield_lookup.slot == 0u &&
               !sleep_lookup.matched &&
               sleep_lookup.slot == kernel::task_syscall_table_unmapped_slot &&
               trap_result_matches(unbound,
                                   kernel::TrapDisposition::rejected,
                                   kernel::TrapError::unbound_adapter) &&
               trap_result_matches(missing,
                                   kernel::TrapDisposition::unsupported,
                                   kernel::TrapError::unsupported_service) &&
               trace.size() == 2u &&
               first->matched && !first->handler_valid &&
               first->slot == 0u &&
               first->error == kernel::TrapError::unbound_adapter &&
               second->slot == kernel::task_syscall_table_unmapped_slot &&
               !second->matched && !second->handler_valid &&
               second->trap_service == kernel::TrapService::invalid &&
               second->error == kernel::TrapError::unsupported_service &&
               same_text(second_projection.descriptor.syscall_name,
                         "sleep-until"sv) &&
               second_projection.field_count == 1u &&
               same_text(second_projection.fields[0].name, "due"sv) &&
               second_projection.fields[0].value == 12u &&
               first_witness.verdict() == semantic::Verdict::drifted &&
               first_witness.failure_domain() ==
                   semantic::FailureDomain::handoff &&
               second_witness.verdict() == semantic::Verdict::drifted &&
               second_witness.failure_domain() ==
                   semantic::FailureDomain::selection;
    }
}

int main()
{
    const bool lookup_ok = demo::probe_lookup_and_table_dispatch();
    const bool error_ok = demo::probe_unbound_and_missing_entry();
    const bool ok = lookup_ok && error_ok;

    std::printf(
        "[runtime-task-syscall-table-demo] ok=%d lookup=%d error=%d\n",
        ok ? 1 : 0,
        lookup_ok ? 1 : 0,
        error_ok ? 1 : 0);
    return ok ? 0 : 1;
}
