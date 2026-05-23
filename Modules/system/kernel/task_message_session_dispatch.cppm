module;

#include <array>
#include <cstddef>
#include <string_view>

export module kernel.task_message_session_dispatch;

export import kernel.task_message_session_api;
export import kernel.task_syscall_dispatch;
import semantic.core;
import util.core;

export namespace kernel {
    inline constexpr util::u16 task_message_session_service_unmapped_slot =
        static_cast<util::u16>(0xFFFFu);
    inline constexpr util::u16 task_message_session_unmapped_slot =
        static_cast<util::u16>(0xFFFFu);

    struct TaskMessageSessionOpenDispatchView {
        util::u64 service_id{0};
        util::u64 session_handle{0};
        util::u64 payload{0};
    };

    struct TaskMessageSessionRequestDispatchView {
        util::u64 service_id{0};
        util::u64 session_handle{0};
        util::u64 operation{0};
        util::u64 payload{0};
    };

    struct TaskMessageSessionCloseDispatchView {
        util::u64 service_id{0};
        util::u64 session_handle{0};
        util::u64 reason{0};
    };

    struct TaskMessageSessionHandler {
        void* self{nullptr};
        TrapResult (*open_fn)(
            void* self,
            TaskMessageSessionOpenDispatchView open) noexcept {nullptr};
        TrapResult (*request_fn)(
            void* self,
            TaskMessageSessionRequestDispatchView request) noexcept {nullptr};
        TrapResult (*close_fn)(
            void* self,
            TaskMessageSessionCloseDispatchView close) noexcept {nullptr};

        [[nodiscard]] bool valid() const noexcept
        {
            return open_fn != nullptr && request_fn != nullptr &&
                   close_fn != nullptr;
        }

        [[nodiscard]] TrapResult open(
            TaskMessageSessionOpenDispatchView open) const noexcept
        {
            if (!valid()) {
                return TrapResult{
                    .disposition = TrapDisposition::rejected,
                    .error = TrapError::unbound_adapter,
                    .value = 0,
                };
            }

            return open_fn(self, open);
        }

        [[nodiscard]] TrapResult request(
            TaskMessageSessionRequestDispatchView request_view) const noexcept
        {
            if (!valid()) {
                return TrapResult{
                    .disposition = TrapDisposition::rejected,
                    .error = TrapError::unbound_adapter,
                    .value = 0,
                };
            }

            return request_fn(self, request_view);
        }

        [[nodiscard]] TrapResult close(
            TaskMessageSessionCloseDispatchView close_view) const noexcept
        {
            if (!valid()) {
                return TrapResult{
                    .disposition = TrapDisposition::rejected,
                    .error = TrapError::unbound_adapter,
                    .value = 0,
                };
            }

            return close_fn(self, close_view);
        }
    };

    namespace detail {
        template <typename Target>
        [[nodiscard]] TrapResult task_message_session_open_adapter(
            void* self,
            TaskMessageSessionOpenDispatchView open) noexcept
        {
            return static_cast<Target*>(self)->open(open);
        }

        template <typename Target>
        [[nodiscard]] TrapResult task_message_session_request_adapter(
            void* self,
            TaskMessageSessionRequestDispatchView request) noexcept
        {
            return static_cast<Target*>(self)->request(request);
        }

        template <typename Target>
        [[nodiscard]] TrapResult task_message_session_close_adapter(
            void* self,
            TaskMessageSessionCloseDispatchView close) noexcept
        {
            return static_cast<Target*>(self)->close(close);
        }
    }

    template <typename Target>
    [[nodiscard]] auto make_task_message_session_handler(Target& target) noexcept
        -> TaskMessageSessionHandler
    {
        return TaskMessageSessionHandler{
            .self = &target,
            .open_fn = &detail::task_message_session_open_adapter<Target>,
            .request_fn =
                &detail::task_message_session_request_adapter<Target>,
            .close_fn = &detail::task_message_session_close_adapter<Target>,
        };
    }

    struct TaskMessageSessionHandlerEntry {
        util::u64 service_id{0};
        const char* service_name{"session-service"};
        TaskMessageSessionHandler handler{};
    };

    [[nodiscard]] constexpr TaskMessageSessionHandlerEntry
    task_message_session_handler_entry(
        util::u64 service_id,
        TaskMessageSessionHandler handler = {}) noexcept
    {
        return TaskMessageSessionHandlerEntry{
            .service_id = service_id,
            .service_name = "session-service",
            .handler = handler,
        };
    }

    [[nodiscard]] constexpr TaskMessageSessionHandlerEntry
    task_message_session_handler_entry(
        util::u64 service_id,
        const char* service_name,
        TaskMessageSessionHandler handler = {}) noexcept
    {
        return TaskMessageSessionHandlerEntry{
            .service_id = service_id,
            .service_name = service_name,
            .handler = handler,
        };
    }

    struct TaskMessageSessionServiceLookup {
        const TaskMessageSessionHandlerEntry* entry{nullptr};
        util::u16 slot{task_message_session_service_unmapped_slot};
        bool matched{false};
    };

    struct TaskMessageSessionSlot {
        bool active{false};
        util::u64 service_id{0};
        util::u64 session_handle{0};
        util::u16 service_slot{task_message_session_service_unmapped_slot};
    };

    struct TaskMessageSessionSlotLookup {
        const TaskMessageSessionSlot* slot_entry{nullptr};
        util::u16 slot{task_message_session_unmapped_slot};
        bool matched{false};
    };

    struct TaskMessageSessionDispatchResult {
        TaskSyscallRequest request{
            .syscall = TaskSyscallId::capability_call,
        };
        TaskMessageSessionActionKind action{
            TaskMessageSessionActionKind::none};
        util::u64 capability_id{0};
        util::u64 operation{0};
        util::u64 payload{0};
        util::u64 service_id{0};
        util::u64 session_handle{0};
        util::u16 service_slot{task_message_session_service_unmapped_slot};
        util::u16 session_slot{task_message_session_unmapped_slot};
        bool matched{false};
        bool handler_valid{false};
        bool session_found{false};
        bool session_allocated{false};
        bool session_closed{false};
        TrapResult trap{
            .disposition = TrapDisposition::unsupported,
            .error = TrapError::unsupported_service,
            .value = 0,
        };
    };

    struct TaskMessageSessionDispatchTraceEvent {
        util::u64 sequence{0};
        TaskSyscallId syscall{TaskSyscallId::invalid};
        TaskMessageSessionActionKind action{
            TaskMessageSessionActionKind::none};
        util::u64 capability_id{0};
        util::u64 operation{0};
        util::u64 payload{0};
        util::u64 service_id{0};
        const char* service_name{"unmapped"};
        util::u64 session_handle{0};
        util::u16 service_slot{task_message_session_service_unmapped_slot};
        util::u16 session_slot{task_message_session_unmapped_slot};
        bool matched{false};
        bool handler_valid{false};
        bool session_found{false};
        bool session_allocated{false};
        bool session_closed{false};
        TrapDisposition disposition{TrapDisposition::unsupported};
        TrapError error{TrapError::unsupported_service};
        util::u64 value{0};
    };

    static_assert(
        semantic::reflected_member_names_match_when_enabled<
            TaskMessageSessionDispatchTraceEvent>(
            std::array<std::string_view, 19>{
                "sequence",
                "syscall",
                "action",
                "capability_id",
                "operation",
                "payload",
                "service_id",
                "service_name",
                "session_handle",
                "service_slot",
                "session_slot",
                "matched",
                "handler_valid",
                "session_found",
                "session_allocated",
                "session_closed",
                "disposition",
                "error",
                "value",
            }));

    struct TaskMessageSessionDispatchWitness {
        util::u64 sequence{0};
        bool ready{false};
        bool has_trace{false};
        TaskSyscallId syscall{TaskSyscallId::invalid};
        TaskMessageSessionActionKind action{
            TaskMessageSessionActionKind::none};
        util::u64 capability_id{0};
        util::u64 operation{0};
        util::u64 payload{0};
        util::u64 service_id{0};
        util::u64 session_handle{0};
        util::u16 service_slot{task_message_session_service_unmapped_slot};
        util::u16 session_slot{task_message_session_unmapped_slot};
        bool matched{false};
        bool handler_valid{false};
        bool session_found{false};
        bool session_allocated{false};
        bool session_closed{false};
        TrapDisposition disposition{TrapDisposition::unsupported};
        TrapError error{TrapError::unsupported_service};
        util::u64 value{0};

        [[nodiscard]] constexpr bool unsupported_syscall_branch_ok()
            const noexcept
        {
            return syscall != TaskSyscallId::capability_call &&
                   action == TaskMessageSessionActionKind::none &&
                   disposition == TrapDisposition::unsupported &&
                   error == TrapError::unsupported_service;
        }

        [[nodiscard]] constexpr bool service_missing_branch_ok() const noexcept
        {
            return action == TaskMessageSessionActionKind::open &&
                   !matched && !handler_valid &&
                   service_slot == task_message_session_service_unmapped_slot &&
                   disposition == TrapDisposition::unsupported &&
                   error == TrapError::unsupported_service;
        }

        [[nodiscard]] constexpr bool service_unbound_branch_ok() const noexcept
        {
            return action == TaskMessageSessionActionKind::open &&
                   matched && !handler_valid &&
                   service_slot != task_message_session_service_unmapped_slot &&
                   disposition == TrapDisposition::rejected &&
                   error == TrapError::unbound_adapter;
        }

        [[nodiscard]] constexpr bool session_missing_branch_ok() const noexcept
        {
            return (action == TaskMessageSessionActionKind::request ||
                    action == TaskMessageSessionActionKind::close) &&
                   !session_found &&
                   session_slot == task_message_session_unmapped_slot &&
                   disposition == TrapDisposition::rejected &&
                   error == TrapError::invalid_argument;
        }

        [[nodiscard]] constexpr bool handler_route_ok() const noexcept
        {
            return matched && handler_valid &&
                   service_slot != task_message_session_service_unmapped_slot;
        }

        [[nodiscard]] constexpr bool open_branch_ok() const noexcept
        {
            if (action != TaskMessageSessionActionKind::open) {
                return false;
            }

            const bool allocated_shape =
                session_allocated &&
                session_slot != task_message_session_unmapped_slot &&
                session_handle != 0u && disposition == TrapDisposition::handled &&
                error == TrapError::none && value == session_handle;
            const bool allocation_rejected_shape =
                !session_allocated &&
                session_slot == task_message_session_unmapped_slot &&
                disposition == TrapDisposition::rejected &&
                error == TrapError::invalid_argument;

            return handler_route_ok() &&
                   (allocated_shape || allocation_rejected_shape);
        }

        [[nodiscard]] constexpr bool request_branch_ok() const noexcept
        {
            return action == TaskMessageSessionActionKind::request &&
                   session_found && !session_allocated && !session_closed &&
                   session_handle == capability_id && handler_route_ok();
        }

        [[nodiscard]] constexpr bool close_branch_ok() const noexcept
        {
            return action == TaskMessageSessionActionKind::close &&
                   session_found && !session_allocated && handler_route_ok();
        }

        [[nodiscard]] constexpr bool ok() const noexcept
        {
            return verdict() == semantic::Verdict::standing;
        }

        [[nodiscard]] constexpr semantic::Result result() const noexcept
        {
            return verdict() == semantic::Verdict::standing
                       ? semantic::Result::ok
                       : semantic::Result::failed;
        }

        [[nodiscard]] constexpr semantic::Verdict verdict() const noexcept
        {
            if (!ready) {
                return semantic::Verdict::collapsed;
            }

            if (unsupported_syscall_branch_ok() ||
                service_missing_branch_ok() ||
                service_unbound_branch_ok() ||
                session_missing_branch_ok() || open_branch_ok() ||
                request_branch_ok() || close_branch_ok()) {
                return semantic::Verdict::standing;
            }

            return semantic::Verdict::drifted;
        }

        [[nodiscard]] constexpr semantic::FailureDomain
        failure_domain() const noexcept
        {
            if (!ready) {
                return semantic::FailureDomain::input;
            }

            if (verdict() == semantic::Verdict::standing) {
                return semantic::FailureDomain::none;
            }

            if (unsupported_syscall_branch_ok() ||
                service_missing_branch_ok()) {
                return semantic::FailureDomain::selection;
            }

            if (service_unbound_branch_ok()) {
                return semantic::FailureDomain::handoff;
            }

            if (session_missing_branch_ok()) {
                return semantic::FailureDomain::route;
            }

            if (!handler_valid) {
                return semantic::FailureDomain::handoff;
            }

            return semantic::FailureDomain::route;
        }

        [[nodiscard]] constexpr std::string_view summary_path() const noexcept
        {
            return "task-message-session-dispatch-witness.summary";
        }
    };

    struct TaskMessageSessionDispatchWitnessHandoffTarget {
        const TaskMessageSessionDispatchWitness* witness{nullptr};

        [[nodiscard]] constexpr std::string_view entry_name() const noexcept
        {
            return "task-message-session-dispatch-witness";
        }

        [[nodiscard]] constexpr std::string_view
        selected_summary_path() const noexcept
        {
            return witness != nullptr ? witness->summary_path()
                                      : std::string_view{
                                            "task-message-session-dispatch-witness.summary"};
        }
    };

    static_assert(
        semantic::reflected_member_names_match_when_enabled<
            TaskMessageSessionDispatchWitness>(
            std::array<std::string_view, 20>{
                "sequence",
                "ready",
                "has_trace",
                "syscall",
                "action",
                "capability_id",
                "operation",
                "payload",
                "service_id",
                "session_handle",
                "service_slot",
                "session_slot",
                "matched",
                "handler_valid",
                "session_found",
                "session_allocated",
                "session_closed",
                "disposition",
                "error",
                "value",
            }));

    static_assert(semantic::WitnessCarrier<TaskMessageSessionDispatchWitness>);
    static_assert(
        semantic::HandoffTarget<
            TaskMessageSessionDispatchWitnessHandoffTarget>);

    [[nodiscard]] constexpr TaskSyscallRequest
    task_message_session_request_from_trace_event(
        const TaskMessageSessionDispatchTraceEvent& event) noexcept
    {
        return TaskSyscallRequest{
            .syscall = event.syscall,
            .arg0 = event.capability_id,
            .arg1 = event.operation,
            .arg2 = event.payload,
        };
    }

    [[nodiscard]] constexpr TaskMessageSessionDispatchWitness
    task_message_session_dispatch_witness(
        const TaskMessageSessionDispatchResult& result) noexcept
    {
        return TaskMessageSessionDispatchWitness{
            .ready = true,
            .syscall = result.request.syscall,
            .action = result.action,
            .capability_id = result.capability_id,
            .operation = result.operation,
            .payload = result.payload,
            .service_id = result.service_id,
            .session_handle = result.session_handle,
            .service_slot = result.service_slot,
            .session_slot = result.session_slot,
            .matched = result.matched,
            .handler_valid = result.handler_valid,
            .session_found = result.session_found,
            .session_allocated = result.session_allocated,
            .session_closed = result.session_closed,
            .disposition = result.trap.disposition,
            .error = result.trap.error,
            .value = result.trap.value,
        };
    }

    [[nodiscard]] constexpr TaskMessageSessionDispatchWitness
    task_message_session_dispatch_witness(
        const TaskMessageSessionDispatchTraceEvent& event) noexcept
    {
        return TaskMessageSessionDispatchWitness{
            .sequence = event.sequence,
            .ready = event.sequence != 0u,
            .has_trace = true,
            .syscall = event.syscall,
            .action = event.action,
            .capability_id = event.capability_id,
            .operation = event.operation,
            .payload = event.payload,
            .service_id = event.service_id,
            .session_handle = event.session_handle,
            .service_slot = event.service_slot,
            .session_slot = event.session_slot,
            .matched = event.matched,
            .handler_valid = event.handler_valid,
            .session_found = event.session_found,
            .session_allocated = event.session_allocated,
            .session_closed = event.session_closed,
            .disposition = event.disposition,
            .error = event.error,
            .value = event.value,
        };
    }

    [[nodiscard]] constexpr bool task_message_session_dispatch_witness_ready(
        const TaskMessageSessionDispatchWitness& witness) noexcept
    {
        return witness.ready;
    }

    [[nodiscard]] constexpr TaskMessageSessionDispatchWitnessHandoffTarget
    task_message_session_dispatch_witness_handoff_target(
        const TaskMessageSessionDispatchWitness& witness) noexcept
    {
        return TaskMessageSessionDispatchWitnessHandoffTarget{
            .witness = &witness,
        };
    }

    template <std::size_t Capacity>
    class TaskMessageSessionDispatchTraceBuffer {
    public:
        using value_type = TaskMessageSessionDispatchTraceEvent;

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

    template <std::size_t Capacity>
    [[nodiscard]] constexpr TaskMessageSessionDispatchWitness
    task_message_session_dispatch_witness(
        const TaskMessageSessionDispatchTraceBuffer<Capacity>& trace) noexcept
    {
        const auto* terminal =
            trace.size() == 0u ? nullptr : trace.at(trace.size() - 1u);
        if (terminal == nullptr) {
            return TaskMessageSessionDispatchWitness{};
        }

        return task_message_session_dispatch_witness(*terminal);
    }

    template <std::size_t ServiceCapacity,
              std::size_t SessionCapacity,
              typename TraceBuffer =
                  TaskMessageSessionDispatchTraceBuffer<1>>
    class TaskMessageSessionDispatcher {
    public:
        using entry_type = TaskMessageSessionHandlerEntry;
        using slot_type = TaskMessageSessionSlot;
        using trace_type = TraceBuffer;
        using result_type = TaskMessageSessionDispatchResult;

        static_assert(ServiceCapacity > 0);
        static_assert(SessionCapacity > 0);

        constexpr TaskMessageSessionDispatcher() noexcept = default;

        constexpr explicit TaskMessageSessionDispatcher(
            std::array<entry_type, ServiceCapacity> entries,
            TraceBuffer* trace = nullptr) noexcept
            : entries_(entries), trace_(trace)
        {
        }

        [[nodiscard]] bool valid() const noexcept
        {
            return true;
        }

        [[nodiscard]] static consteval std::size_t service_capacity() noexcept
        {
            return ServiceCapacity;
        }

        [[nodiscard]] static consteval std::size_t session_capacity() noexcept
        {
            return SessionCapacity;
        }

        void bind_trace(TraceBuffer* trace) noexcept
        {
            trace_ = trace;
        }

        void bind_entry(std::size_t index, entry_type entry) noexcept
        {
            if (index >= ServiceCapacity) {
                return;
            }

            entries_[index] = entry;
        }

        void bind_next_session_handle(util::u64 next_session_handle) noexcept
        {
            next_session_handle_ = next_session_handle;
        }

        void reset_sessions() noexcept
        {
            slots_ = {};
        }

        [[nodiscard]] const entry_type* entry(std::size_t index) const noexcept
        {
            if (index >= ServiceCapacity) {
                return nullptr;
            }

            return &entries_[index];
        }

        [[nodiscard]] const slot_type* session(std::size_t index) const noexcept
        {
            if (index >= SessionCapacity) {
                return nullptr;
            }

            return &slots_[index];
        }

        [[nodiscard]] std::size_t active_sessions() const noexcept
        {
            std::size_t active = 0;
            for (const auto& slot : slots_) {
                if (slot.active) {
                    ++active;
                }
            }
            return active;
        }

        [[nodiscard]] TaskMessageSessionServiceLookup lookup_service(
            util::u64 service_id) const noexcept
        {
            for (std::size_t index = 0; index < ServiceCapacity; ++index) {
                if (entries_[index].service_id != service_id) {
                    continue;
                }

                return TaskMessageSessionServiceLookup{
                    .entry = &entries_[index],
                    .slot = static_cast<util::u16>(index),
                    .matched = true,
                };
            }

            return TaskMessageSessionServiceLookup{};
        }

        [[nodiscard]] TaskMessageSessionSlotLookup lookup_session(
            util::u64 session_handle) const noexcept
        {
            for (std::size_t index = 0; index < SessionCapacity; ++index) {
                if (!slots_[index].active ||
                    slots_[index].session_handle != session_handle) {
                    continue;
                }

                return TaskMessageSessionSlotLookup{
                    .slot_entry = &slots_[index],
                    .slot = static_cast<util::u16>(index),
                    .matched = true,
                };
            }

            return TaskMessageSessionSlotLookup{};
        }

        [[nodiscard]] result_type open_session(
            util::u64 service_id,
            util::u64 payload = 0) noexcept
        {
            return dispatch_capability_call(TrapCapabilityCallView{
                .capability_id = service_id,
                .operation = task_message_session_open_operation,
                .payload = payload,
            });
        }

        [[nodiscard]] result_type request_session(
            util::u64 session_handle,
            util::u64 operation,
            util::u64 payload = 0) noexcept
        {
            return dispatch_capability_call(TrapCapabilityCallView{
                .capability_id = session_handle,
                .operation = operation,
                .payload = payload,
            });
        }

        [[nodiscard]] result_type close_session(
            util::u64 session_handle,
            util::u64 reason = 0) noexcept
        {
            return dispatch_capability_call(TrapCapabilityCallView{
                .capability_id = session_handle,
                .operation = task_message_session_close_operation,
                .payload = reason,
            });
        }

        [[nodiscard]] result_type dispatch_capability_call(
            TrapCapabilityCallView capability) noexcept
        {
            auto result = result_type{
                .request = make_task_syscall_capability_call_request(capability),
                .capability_id = capability.capability_id,
                .operation = capability.operation,
                .payload = capability.payload,
            };

            if (capability.operation == task_message_session_open_operation) {
                return dispatch_open(result);
            }

            if (capability.operation == task_message_session_close_operation) {
                return dispatch_close(result);
            }

            return dispatch_request(result);
        }

        [[nodiscard]] result_type dispatch_request(
            TaskSyscallRequest request) noexcept
        {
            if (request.syscall != TaskSyscallId::capability_call) {
                auto result = result_type{
                    .request = request,
                    .trap = unsupported_result(),
                };
                trace_push(result);
                return result;
            }

            return dispatch_capability_call(TrapCapabilityCallView{
                .capability_id = request.arg0,
                .operation = request.arg1,
                .payload = request.arg2,
            });
        }

        [[nodiscard]] TrapResult dispatch(TaskSyscallRequest request) noexcept
        {
            return dispatch_request(request).trap;
        }

        [[nodiscard]] TrapResult capability_call(
            TrapCapabilityCallView capability) noexcept
        {
            return dispatch_capability_call(capability).trap;
        }

        [[nodiscard]] TrapResult capability_call(util::u64 capability_id,
                                                 util::u64 operation,
                                                 util::u64 payload = 0) noexcept
        {
            return capability_call(TrapCapabilityCallView{
                .capability_id = capability_id,
                .operation = operation,
                .payload = payload,
            });
        }

    private:
        struct FreeSlotLookup {
            util::u16 slot{task_message_session_unmapped_slot};
            bool found{false};
        };

        [[nodiscard]] static constexpr TrapResult unsupported_result() noexcept
        {
            return TrapResult{
                .disposition = TrapDisposition::unsupported,
                .error = TrapError::unsupported_service,
                .value = 0,
            };
        }

        [[nodiscard]] static constexpr TrapResult invalid_argument_result()
            noexcept
        {
            return TrapResult{
                .disposition = TrapDisposition::rejected,
                .error = TrapError::invalid_argument,
                .value = 0,
            };
        }

        [[nodiscard]] static constexpr TrapResult unbound_adapter_result()
            noexcept
        {
            return TrapResult{
                .disposition = TrapDisposition::rejected,
                .error = TrapError::unbound_adapter,
                .value = 0,
            };
        }

        [[nodiscard]] FreeSlotLookup first_free_session_slot() const noexcept
        {
            for (std::size_t index = 0; index < SessionCapacity; ++index) {
                if (slots_[index].active) {
                    continue;
                }

                return FreeSlotLookup{
                    .slot = static_cast<util::u16>(index),
                    .found = true,
                };
            }

            return FreeSlotLookup{};
        }

        [[nodiscard]] result_type dispatch_open(result_type result) noexcept
        {
            result.action = TaskMessageSessionActionKind::open;
            result.service_id = result.capability_id;

            const auto service = lookup_service(result.service_id);
            result.service_slot = service.slot;
            result.matched = service.matched;
            if (!service.matched || service.entry == nullptr) {
                result.trap = unsupported_result();
                trace_push(result);
                return result;
            }

            result.handler_valid = service.entry->handler.valid();
            if (!result.handler_valid) {
                result.trap = unbound_adapter_result();
                trace_push(result);
                return result;
            }

            const auto free_slot = first_free_session_slot();
            result.session_slot = free_slot.slot;
            if (!free_slot.found) {
                result.trap = invalid_argument_result();
                trace_push(result);
                return result;
            }

            result.session_handle = next_session_handle_++;
            result.trap = service.entry->handler.open(
                TaskMessageSessionOpenDispatchView{
                    .service_id = result.service_id,
                    .session_handle = result.session_handle,
                    .payload = result.payload,
                });

            if (result.trap.ok()) {
                auto& slot = slots_[free_slot.slot];
                slot.active = true;
                slot.service_id = result.service_id;
                slot.session_handle = result.session_handle;
                slot.service_slot = service.slot;
                result.session_allocated = true;
                result.trap.value = result.session_handle;
            }

            trace_push(result);
            return result;
        }

        [[nodiscard]] result_type dispatch_request(result_type result) noexcept
        {
            result.action = TaskMessageSessionActionKind::request;
            result.session_handle = result.capability_id;

            const auto session_lookup = lookup_session(result.session_handle);
            result.session_slot = session_lookup.slot;
            result.session_found = session_lookup.matched;
            if (!session_lookup.matched || session_lookup.slot_entry == nullptr) {
                result.trap = invalid_argument_result();
                trace_push(result);
                return result;
            }

            result.service_id = session_lookup.slot_entry->service_id;
            result.service_slot = session_lookup.slot_entry->service_slot;
            const auto* service_entry = entry(result.service_slot);
            result.matched = service_entry != nullptr;
            result.handler_valid =
                service_entry != nullptr && service_entry->handler.valid();
            if (!result.handler_valid || service_entry == nullptr) {
                result.trap = unbound_adapter_result();
                trace_push(result);
                return result;
            }

            result.trap = service_entry->handler.request(
                TaskMessageSessionRequestDispatchView{
                    .service_id = result.service_id,
                    .session_handle = result.session_handle,
                    .operation = result.operation,
                    .payload = result.payload,
                });
            trace_push(result);
            return result;
        }

        [[nodiscard]] result_type dispatch_close(result_type result) noexcept
        {
            result.action = TaskMessageSessionActionKind::close;
            result.session_handle = result.capability_id;

            const auto session_lookup = lookup_session(result.session_handle);
            result.session_slot = session_lookup.slot;
            result.session_found = session_lookup.matched;
            if (!session_lookup.matched || session_lookup.slot_entry == nullptr) {
                result.trap = invalid_argument_result();
                trace_push(result);
                return result;
            }

            result.service_id = session_lookup.slot_entry->service_id;
            result.service_slot = session_lookup.slot_entry->service_slot;
            const auto* service_entry = entry(result.service_slot);
            result.matched = service_entry != nullptr;
            result.handler_valid =
                service_entry != nullptr && service_entry->handler.valid();
            if (!result.handler_valid || service_entry == nullptr) {
                result.trap = unbound_adapter_result();
                trace_push(result);
                return result;
            }

            result.trap = service_entry->handler.close(
                TaskMessageSessionCloseDispatchView{
                    .service_id = result.service_id,
                    .session_handle = result.session_handle,
                    .reason = result.payload,
                });
            if (result.trap.ok()) {
                slots_[session_lookup.slot] = {};
                result.session_closed = true;
            }

            trace_push(result);
            return result;
        }

        void trace_push(const result_type& result) noexcept
        {
            if (trace_ == nullptr) {
                return;
            }

            const auto* service_entry = entry(result.service_slot);
            ++sequence_;
            (void)trace_->push(typename TraceBuffer::value_type{
                .sequence = sequence_,
                .syscall = result.request.syscall,
                .action = result.action,
                .capability_id = result.capability_id,
                .operation = result.operation,
                .payload = result.payload,
                .service_id = result.service_id,
                .service_name = service_entry != nullptr
                                    ? service_entry->service_name
                                    : "unmapped",
                .session_handle = result.session_handle,
                .service_slot = result.service_slot,
                .session_slot = result.session_slot,
                .matched = result.matched,
                .handler_valid = result.handler_valid,
                .session_found = result.session_found,
                .session_allocated = result.session_allocated,
                .session_closed = result.session_closed,
                .disposition = result.trap.disposition,
                .error = result.trap.error,
                .value = result.trap.value,
            });
        }

        std::array<entry_type, ServiceCapacity> entries_{};
        std::array<slot_type, SessionCapacity> slots_{};
        TraceBuffer* trace_{nullptr};
        util::u64 sequence_{0};
        util::u64 next_session_handle_{1};
    };

    template <std::size_t ServiceCapacity, std::size_t SessionCapacity>
    [[nodiscard]] auto make_task_message_session_dispatcher(
        std::array<TaskMessageSessionHandlerEntry, ServiceCapacity>
            entries) noexcept
        -> TaskMessageSessionDispatcher<ServiceCapacity, SessionCapacity>
    {
        return TaskMessageSessionDispatcher<ServiceCapacity, SessionCapacity>{
            entries,
        };
    }

    template <std::size_t ServiceCapacity,
              std::size_t SessionCapacity,
              typename TraceBuffer>
    [[nodiscard]] auto make_task_message_session_dispatcher(
        std::array<TaskMessageSessionHandlerEntry, ServiceCapacity> entries,
        TraceBuffer* trace) noexcept
        -> TaskMessageSessionDispatcher<ServiceCapacity,
                                        SessionCapacity,
                                        TraceBuffer>
    {
        return TaskMessageSessionDispatcher<ServiceCapacity,
                                            SessionCapacity,
                                            TraceBuffer>{entries, trace};
    }
}
