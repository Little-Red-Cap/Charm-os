module;

#include <array>
#include <cstddef>

export module kernel.task_syscall_table;

export import kernel.task_syscall_dispatch;
import util.core;

export namespace kernel {
    inline constexpr util::u16 task_syscall_table_unmapped_slot =
        static_cast<util::u16>(0xFFFFu);

    struct TaskSyscallHandler {
        void* self{nullptr};
        TrapResult (*dispatch_fn)(void* self,
                                  TaskSyscallRequest request) noexcept {nullptr};

        [[nodiscard]] bool valid() const noexcept
        {
            return dispatch_fn != nullptr;
        }

        [[nodiscard]] TrapResult dispatch(TaskSyscallRequest request) const
            noexcept
        {
            if (!valid()) {
                return TrapResult{
                    .disposition = TrapDisposition::rejected,
                    .error = TrapError::unbound_adapter,
                    .value = 0,
                };
            }

            return dispatch_fn(self, request);
        }
    };

    namespace detail {
        template <typename Target>
        [[nodiscard]] TrapResult task_syscall_handler_dispatch_adapter(
            void* self,
            TaskSyscallRequest request) noexcept
        {
            return static_cast<Target*>(self)->dispatch(request);
        }
    }

    template <typename Target>
    [[nodiscard]] auto make_task_syscall_handler(Target& target) noexcept
        -> TaskSyscallHandler
    {
        return TaskSyscallHandler{
            .self = &target,
            .dispatch_fn =
                &detail::task_syscall_handler_dispatch_adapter<Target>,
        };
    }

    struct TaskSyscallHandlerEntry {
        TaskSyscallCatalogEntry descriptor{};
        TaskSyscallHandler handler{};
    };

    [[nodiscard]] constexpr TaskSyscallHandlerEntry task_syscall_handler_entry(
        TaskSyscallId syscall,
        TaskSyscallHandler handler = {}) noexcept
    {
        return TaskSyscallHandlerEntry{
            .descriptor = task_syscall_catalog_entry(syscall),
            .handler = handler,
        };
    }

    [[nodiscard]] constexpr TaskSyscallHandlerEntry task_syscall_handler_entry(
        TaskSyscallCatalogEntry descriptor,
        TaskSyscallHandler handler = {}) noexcept
    {
        return TaskSyscallHandlerEntry{
            .descriptor = descriptor,
            .handler = handler,
        };
    }

    struct TaskSyscallTableLookup {
        const TaskSyscallHandlerEntry* entry{nullptr};
        util::u16 slot{task_syscall_table_unmapped_slot};
        bool matched{false};
    };

    struct TaskSyscallTableTraceEvent {
        util::u64 sequence{0};
        TaskSyscallId syscall{TaskSyscallId::invalid};
        TrapService trap_service{TrapService::invalid};
        util::u16 slot{task_syscall_table_unmapped_slot};
        bool matched{false};
        bool handler_valid{false};
        TrapDisposition disposition{TrapDisposition::rejected};
        TrapError error{TrapError::unsupported_service};
        util::u64 arg0{0};
        util::u64 arg1{0};
        util::u64 arg2{0};
        util::u64 arg3{0};
        util::u64 value{0};
    };

    [[nodiscard]] constexpr TaskSyscallRequest
    task_syscall_request_from_trace_event(
        const TaskSyscallTableTraceEvent& event) noexcept
    {
        return TaskSyscallRequest{
            .syscall = event.syscall,
            .arg0 = event.arg0,
            .arg1 = event.arg1,
            .arg2 = event.arg2,
            .arg3 = event.arg3,
        };
    }

    [[nodiscard]] constexpr TaskSyscallSemanticProjection
    task_syscall_semantic_projection(
        const TaskSyscallTableTraceEvent& event) noexcept
    {
        return task_syscall_semantic_projection(
            task_syscall_request_from_trace_event(event));
    }

    template <std::size_t Capacity>
    class TaskSyscallTableTraceBuffer {
    public:
        using value_type = TaskSyscallTableTraceEvent;

        static_assert(Capacity > 0);

        [[nodiscard]] bool push(const value_type& event) noexcept
        {
            events_[head_] = event;
            head_ = (head_ + 1u) % Capacity;
            if (size_ < Capacity) {
                ++size_;
            }
            return true;
        }

        [[nodiscard]] std::size_t size() const noexcept
        {
            return size_;
        }

        [[nodiscard]] const value_type* at(std::size_t index) const noexcept
        {
            if (index >= size_) {
                return nullptr;
            }

            const auto first = (head_ + Capacity - size_) % Capacity;
            return &events_[(first + index) % Capacity];
        }

    private:
        std::array<value_type, Capacity> events_{};
        std::size_t head_{0};
        std::size_t size_{0};
    };

    template <std::size_t Capacity,
              typename TraceBuffer = TaskSyscallTableTraceBuffer<1>>
    class TaskSyscallTable {
    public:
        using entry_type = TaskSyscallHandlerEntry;
        using trace_type = TraceBuffer;

        constexpr TaskSyscallTable() noexcept = default;

        constexpr explicit TaskSyscallTable(
            std::array<entry_type, Capacity> entries,
            TraceBuffer* trace = nullptr) noexcept
            : entries_(entries), trace_(trace)
        {
        }

        [[nodiscard]] static consteval std::size_t capacity() noexcept
        {
            return Capacity;
        }

        void bind_trace(TraceBuffer* trace) noexcept
        {
            trace_ = trace;
        }

        void bind_entry(std::size_t index, entry_type entry) noexcept
        {
            if (index >= Capacity) {
                return;
            }

            entries_[index] = entry;
        }

        [[nodiscard]] const entry_type* entry(std::size_t index) const noexcept
        {
            if (index >= Capacity) {
                return nullptr;
            }

            return &entries_[index];
        }

        [[nodiscard]] TaskSyscallTableLookup lookup(
            TaskSyscallId syscall) const noexcept
        {
            if (syscall == TaskSyscallId::invalid) {
                return TaskSyscallTableLookup{};
            }

            for (std::size_t index = 0; index < Capacity; ++index) {
                if (entries_[index].descriptor.syscall != syscall) {
                    continue;
                }

                return TaskSyscallTableLookup{
                    .entry = &entries_[index],
                    .slot = static_cast<util::u16>(index),
                    .matched = true,
                };
            }

            return TaskSyscallTableLookup{};
        }

        [[nodiscard]] TrapResult dispatch(TaskSyscallRequest request) noexcept
        {
            const auto found = lookup(request.syscall);
            if (!found.matched || found.entry == nullptr) {
                const auto result = TrapResult{
                    .disposition = TrapDisposition::unsupported,
                    .error = TrapError::unsupported_service,
                    .value = 0,
                };
                trace_push(request, TrapService::invalid, found, false, result);
                return result;
            }

            if (!found.entry->handler.valid()) {
                const auto result = TrapResult{
                    .disposition = TrapDisposition::rejected,
                    .error = TrapError::unbound_adapter,
                    .value = 0,
                };
                trace_push(request,
                           found.entry->descriptor.trap_service,
                           found,
                           false,
                           result);
                return result;
            }

            const auto result = found.entry->handler.dispatch(request);
            trace_push(request,
                       found.entry->descriptor.trap_service,
                       found,
                       true,
                       result);
            return result;
        }

        [[nodiscard]] TrapResult dispatch(const TrapRequest& request) noexcept
        {
            return dispatch(task_syscall_request_from_trap_request(request));
        }

        [[nodiscard]] TrapResult yield() noexcept
        {
            return dispatch(make_task_syscall_yield_request());
        }

        template <typename Tick>
        [[nodiscard]] TrapResult sleep_until(Tick due) noexcept
        {
            return dispatch(make_task_syscall_sleep_until_request(
                TrapSleepUntilView<Tick>{
                    .due = due,
                }));
        }

        [[nodiscard]] TrapResult debug_write(util::u64 value) noexcept
        {
            return dispatch(make_task_syscall_debug_write_request(
                TrapDebugWriteView{
                    .value = value,
                }));
        }

        [[nodiscard]] TrapResult capability_call(util::u64 capability_id,
                                                 util::u64 operation,
                                                 util::u64 payload = 0) noexcept
        {
            return dispatch(make_task_syscall_capability_call_request(
                TrapCapabilityCallView{
                    .capability_id = capability_id,
                    .operation = operation,
                    .payload = payload,
                }));
        }

    private:
        void trace_push(TaskSyscallRequest request,
                        TrapService trap_service,
                        TaskSyscallTableLookup found,
                        bool handler_valid,
                        const TrapResult& result) noexcept
        {
            if (trace_ == nullptr) {
                return;
            }

            ++sequence_;
            (void)trace_->push(typename TraceBuffer::value_type{
                .sequence = sequence_,
                .syscall = request.syscall,
                .trap_service = trap_service,
                .slot = found.slot,
                .matched = found.matched,
                .handler_valid = handler_valid,
                .disposition = result.disposition,
                .error = result.error,
                .arg0 = request.arg0,
                .arg1 = request.arg1,
                .arg2 = request.arg2,
                .arg3 = request.arg3,
                .value = result.value,
            });
        }

        std::array<entry_type, Capacity> entries_{};
        TraceBuffer* trace_{nullptr};
        util::u64 sequence_{0};
    };

    template <std::size_t Capacity>
    [[nodiscard]] auto make_task_syscall_table(
        std::array<TaskSyscallHandlerEntry, Capacity> entries) noexcept
        -> TaskSyscallTable<Capacity>
    {
        return TaskSyscallTable<Capacity>{entries};
    }

    template <std::size_t Capacity, typename TraceBuffer>
    [[nodiscard]] auto make_task_syscall_table(
        std::array<TaskSyscallHandlerEntry, Capacity> entries,
        TraceBuffer* trace) noexcept -> TaskSyscallTable<Capacity, TraceBuffer>
    {
        return TaskSyscallTable<Capacity, TraceBuffer>{entries, trace};
    }
}
