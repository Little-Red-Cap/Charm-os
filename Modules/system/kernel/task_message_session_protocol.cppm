module;

#include <array>
#include <cstddef>
#include <string_view>

export module kernel.task_message_session_protocol;

export import kernel.task_message_session_endpoint;
import semantic.core;
import util.core;

export namespace kernel {
    inline constexpr util::u16 task_message_session_protocol_unmapped_slot =
        static_cast<util::u16>(0xFFFFu);

    enum class TaskMessageSessionProtocolTraceKind : util::u8 {
        request = 0,
        close,
    };

    [[nodiscard]] constexpr const char*
    task_message_session_protocol_trace_kind_name(
        TaskMessageSessionProtocolTraceKind kind) noexcept
    {
        switch (kind) {
        case TaskMessageSessionProtocolTraceKind::request:
            return "request";
        case TaskMessageSessionProtocolTraceKind::close:
            return "close";
        }
        return "unknown";
    }

    struct TaskMessageSessionProtocolRequestHandler {
        void* self{nullptr};
        TrapResult (*dispatch_fn)(
            void* self,
            TaskMessageSessionEndpointRequestView request) noexcept {nullptr};

        [[nodiscard]] bool valid() const noexcept
        {
            return dispatch_fn != nullptr;
        }

        [[nodiscard]] TrapResult dispatch(
            TaskMessageSessionEndpointRequestView request) const noexcept
        {
            if (!valid()) {
                return task_message_session_endpoint_unbound_adapter();
            }

            return dispatch_fn(self, request);
        }
    };

    namespace detail {
        template <typename Target>
        [[nodiscard]] TrapResult
        task_message_session_protocol_request_handler_adapter(
            void* self,
            TaskMessageSessionEndpointRequestView request) noexcept
        {
            return static_cast<Target*>(self)->dispatch(request);
        }
    }

    template <typename Target>
    [[nodiscard]] auto make_task_message_session_protocol_request_handler(
        Target& target) noexcept -> TaskMessageSessionProtocolRequestHandler
    {
        return TaskMessageSessionProtocolRequestHandler{
            .self = &target,
            .dispatch_fn =
                &detail::
                    task_message_session_protocol_request_handler_adapter<Target>,
        };
    }

    struct TaskMessageSessionProtocolCloseHandler {
        void* self{nullptr};
        TrapResult (*dispatch_fn)(
            void* self,
            TaskMessageSessionEndpointCloseView close) noexcept {nullptr};

        [[nodiscard]] bool valid() const noexcept
        {
            return dispatch_fn != nullptr;
        }

        [[nodiscard]] TrapResult dispatch(
            TaskMessageSessionEndpointCloseView close) const noexcept
        {
            if (!valid()) {
                return task_message_session_endpoint_handled(0);
            }

            return dispatch_fn(self, close);
        }
    };

    namespace detail {
        template <typename Target>
        [[nodiscard]] TrapResult
        task_message_session_protocol_close_handler_adapter(
            void* self,
            TaskMessageSessionEndpointCloseView close) noexcept
        {
            return static_cast<Target*>(self)->dispatch(close);
        }
    }

    template <typename Target>
    [[nodiscard]] auto make_task_message_session_protocol_close_handler(
        Target& target) noexcept -> TaskMessageSessionProtocolCloseHandler
    {
        return TaskMessageSessionProtocolCloseHandler{
            .self = &target,
            .dispatch_fn =
                &detail::
                    task_message_session_protocol_close_handler_adapter<Target>,
        };
    }

    struct TaskMessageSessionProtocolEntry {
        util::u64 operation{0};
        const char* operation_name{"operation"};
        TaskMessageSessionProtocolRequestHandler handler{};
    };

    [[nodiscard]] constexpr TaskMessageSessionProtocolEntry
    task_message_session_protocol_entry(
        util::u64 operation,
        TaskMessageSessionProtocolRequestHandler handler = {}) noexcept
    {
        return TaskMessageSessionProtocolEntry{
            .operation = operation,
            .operation_name = "operation",
            .handler = handler,
        };
    }

    [[nodiscard]] constexpr TaskMessageSessionProtocolEntry
    task_message_session_protocol_entry(
        util::u64 operation,
        const char* operation_name,
        TaskMessageSessionProtocolRequestHandler handler = {}) noexcept
    {
        return TaskMessageSessionProtocolEntry{
            .operation = operation,
            .operation_name = operation_name,
            .handler = handler,
        };
    }

    template <typename Target>
    [[nodiscard]] auto task_message_session_protocol_entry(
        util::u64 operation,
        Target& target) noexcept -> TaskMessageSessionProtocolEntry
    {
        return task_message_session_protocol_entry(
            operation,
            make_task_message_session_protocol_request_handler(target));
    }

    template <typename Target>
    [[nodiscard]] auto task_message_session_protocol_entry(
        util::u64 operation,
        const char* operation_name,
        Target& target) noexcept -> TaskMessageSessionProtocolEntry
    {
        return task_message_session_protocol_entry(
            operation,
            operation_name,
            make_task_message_session_protocol_request_handler(target));
    }

    struct TaskMessageSessionProtocolLookup {
        const TaskMessageSessionProtocolEntry* entry{nullptr};
        util::u16 slot{task_message_session_protocol_unmapped_slot};
        bool matched{false};
    };

    struct TaskMessageSessionProtocolDispatchResult {
        TaskMessageSessionProtocolTraceKind kind{
            TaskMessageSessionProtocolTraceKind::request};
        TaskMessageSessionEndpoint endpoint{};
        util::u64 operation{0};
        const char* operation_name{"unmapped"};
        util::u64 payload{0};
        util::u16 slot{task_message_session_protocol_unmapped_slot};
        bool matched{false};
        bool handler_valid{false};
        bool close_handler_valid{false};
        TrapResult trap{task_message_session_endpoint_unsupported()};
    };

    struct TaskMessageSessionProtocolTraceEvent {
        util::u64 sequence{0};
        TaskMessageSessionProtocolTraceKind kind{
            TaskMessageSessionProtocolTraceKind::request};
        util::u64 service_id{0};
        const char* service_name{"session-service"};
        util::u64 session_handle{0};
        util::u64 open_payload{0};
        util::u16 channel_slot{task_message_session_channel_unmapped_slot};
        util::u64 operation{0};
        const char* operation_name{"unmapped"};
        util::u64 payload{0};
        util::u16 slot{task_message_session_protocol_unmapped_slot};
        bool matched{false};
        bool handler_valid{false};
        bool close_handler_valid{false};
        TrapDisposition disposition{TrapDisposition::unsupported};
        TrapError error{TrapError::unsupported_service};
        util::u64 value{0};
    };

    static_assert(
        semantic::reflected_member_names_match_when_enabled<
            TaskMessageSessionProtocolTraceEvent>(
            std::array<std::string_view, 17>{
                "sequence",
                "kind",
                "service_id",
                "service_name",
                "session_handle",
                "open_payload",
                "channel_slot",
                "operation",
                "operation_name",
                "payload",
                "slot",
                "matched",
                "handler_valid",
                "close_handler_valid",
                "disposition",
                "error",
                "value",
            }));

    struct TaskMessageSessionProtocolWitness {
        util::u64 sequence{0};
        bool ready{false};
        bool has_trace{false};
        TaskMessageSessionProtocolTraceKind kind{
            TaskMessageSessionProtocolTraceKind::request};
        util::u64 service_id{0};
        util::u64 session_handle{0};
        util::u64 open_payload{0};
        util::u16 channel_slot{task_message_session_channel_unmapped_slot};
        util::u64 operation{0};
        util::u64 payload{0};
        util::u16 slot{task_message_session_protocol_unmapped_slot};
        bool matched{false};
        bool handler_valid{false};
        bool close_handler_valid{false};
        TrapDisposition disposition{TrapDisposition::unsupported};
        TrapError error{TrapError::unsupported_service};
        util::u64 value{0};

        [[nodiscard]] constexpr bool request_handler_branch_ok() const noexcept
        {
            return kind == TaskMessageSessionProtocolTraceKind::request &&
                   matched && handler_valid &&
                   slot != task_message_session_protocol_unmapped_slot;
        }

        [[nodiscard]] constexpr bool request_unmapped_branch_ok() const noexcept
        {
            return kind == TaskMessageSessionProtocolTraceKind::request &&
                   !matched && !handler_valid &&
                   slot == task_message_session_protocol_unmapped_slot &&
                   disposition == TrapDisposition::unsupported &&
                   error == TrapError::unsupported_service;
        }

        [[nodiscard]] constexpr bool request_unbound_branch_ok() const noexcept
        {
            return kind == TaskMessageSessionProtocolTraceKind::request &&
                   matched && !handler_valid &&
                   slot != task_message_session_protocol_unmapped_slot &&
                   disposition == TrapDisposition::rejected &&
                   error == TrapError::unbound_adapter;
        }

        [[nodiscard]] constexpr bool close_branch_ok() const noexcept
        {
            const bool default_close_ok =
                !close_handler_valid &&
                disposition == TrapDisposition::handled &&
                error == TrapError::none && value == 0u;
            return kind == TaskMessageSessionProtocolTraceKind::close &&
                   operation == task_message_session_close_operation &&
                   slot == task_message_session_protocol_unmapped_slot &&
                   !matched && !handler_valid &&
                   (close_handler_valid || default_close_ok);
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

            if (request_handler_branch_ok() ||
                request_unmapped_branch_ok() ||
                request_unbound_branch_ok() || close_branch_ok()) {
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

            if (kind == TaskMessageSessionProtocolTraceKind::request) {
                if (!matched ||
                    slot == task_message_session_protocol_unmapped_slot) {
                    return semantic::FailureDomain::selection;
                }

                if (!handler_valid) {
                    return semantic::FailureDomain::handoff;
                }

                return semantic::FailureDomain::route;
            }

            if (kind == TaskMessageSessionProtocolTraceKind::close) {
                return semantic::FailureDomain::handoff;
            }

            return semantic::FailureDomain::input;
        }

        [[nodiscard]] constexpr std::string_view summary_path() const noexcept
        {
            return "task-message-session-protocol-witness.summary";
        }
    };

    struct TaskMessageSessionProtocolWitnessHandoffTarget {
        const TaskMessageSessionProtocolWitness* witness{nullptr};

        [[nodiscard]] constexpr std::string_view entry_name() const noexcept
        {
            return "task-message-session-protocol-witness";
        }

        [[nodiscard]] constexpr std::string_view
        selected_summary_path() const noexcept
        {
            return witness != nullptr ? witness->summary_path()
                                      : std::string_view{
                                            "task-message-session-protocol-witness.summary"};
        }
    };

    static_assert(
        semantic::reflected_member_names_match_when_enabled<
            TaskMessageSessionProtocolWitness>(
            std::array<std::string_view, 17>{
                "sequence",
                "ready",
                "has_trace",
                "kind",
                "service_id",
                "session_handle",
                "open_payload",
                "channel_slot",
                "operation",
                "payload",
                "slot",
                "matched",
                "handler_valid",
                "close_handler_valid",
                "disposition",
                "error",
                "value",
            }));

    static_assert(semantic::WitnessCarrier<TaskMessageSessionProtocolWitness>);
    static_assert(
        semantic::HandoffTarget<
            TaskMessageSessionProtocolWitnessHandoffTarget>);

    [[nodiscard]] constexpr TaskMessageSessionProtocolWitness
    task_message_session_protocol_witness(
        const TaskMessageSessionProtocolDispatchResult& result) noexcept
    {
        return TaskMessageSessionProtocolWitness{
            .ready = true,
            .kind = result.kind,
            .service_id = result.endpoint.service_id,
            .session_handle = result.endpoint.session_handle,
            .open_payload = result.endpoint.open_payload,
            .channel_slot = result.endpoint.channel_slot,
            .operation = result.operation,
            .payload = result.payload,
            .slot = result.slot,
            .matched = result.matched,
            .handler_valid = result.handler_valid,
            .close_handler_valid = result.close_handler_valid,
            .disposition = result.trap.disposition,
            .error = result.trap.error,
            .value = result.trap.value,
        };
    }

    [[nodiscard]] constexpr TaskMessageSessionProtocolWitness
    task_message_session_protocol_witness(
        const TaskMessageSessionProtocolTraceEvent& event) noexcept
    {
        return TaskMessageSessionProtocolWitness{
            .sequence = event.sequence,
            .ready = event.sequence != 0u,
            .has_trace = true,
            .kind = event.kind,
            .service_id = event.service_id,
            .session_handle = event.session_handle,
            .open_payload = event.open_payload,
            .channel_slot = event.channel_slot,
            .operation = event.operation,
            .payload = event.payload,
            .slot = event.slot,
            .matched = event.matched,
            .handler_valid = event.handler_valid,
            .close_handler_valid = event.close_handler_valid,
            .disposition = event.disposition,
            .error = event.error,
            .value = event.value,
        };
    }

    [[nodiscard]] constexpr bool task_message_session_protocol_witness_ready(
        const TaskMessageSessionProtocolWitness& witness) noexcept
    {
        return witness.ready;
    }

    [[nodiscard]] constexpr TaskMessageSessionProtocolWitnessHandoffTarget
    task_message_session_protocol_witness_handoff_target(
        const TaskMessageSessionProtocolWitness& witness) noexcept
    {
        return TaskMessageSessionProtocolWitnessHandoffTarget{
            .witness = &witness,
        };
    }

    template <std::size_t Capacity>
    class TaskMessageSessionProtocolTraceBuffer {
    public:
        using value_type = TaskMessageSessionProtocolTraceEvent;

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
    [[nodiscard]] constexpr TaskMessageSessionProtocolWitness
    task_message_session_protocol_witness(
        const TaskMessageSessionProtocolTraceBuffer<Capacity>& trace) noexcept
    {
        const auto* terminal =
            trace.size() == 0u ? nullptr : trace.at(trace.size() - 1u);
        if (terminal == nullptr) {
            return TaskMessageSessionProtocolWitness{};
        }

        return task_message_session_protocol_witness(*terminal);
    }

    template <std::size_t Capacity,
              typename TraceBuffer =
                  TaskMessageSessionProtocolTraceBuffer<1>>
    class TaskMessageSessionProtocol {
    public:
        using entry_type = TaskMessageSessionProtocolEntry;
        using trace_type = TraceBuffer;
        using result_type = TaskMessageSessionProtocolDispatchResult;

        static_assert(Capacity > 0);

        constexpr TaskMessageSessionProtocol() noexcept = default;

        constexpr explicit TaskMessageSessionProtocol(
            std::array<entry_type, Capacity> entries,
            TaskMessageSessionProtocolCloseHandler close_handler = {},
            TraceBuffer* trace = nullptr) noexcept
            : entries_(entries), close_handler_(close_handler), trace_(trace)
        {
        }

        [[nodiscard]] bool valid() const noexcept
        {
            return true;
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

        void bind_close_handler(
            TaskMessageSessionProtocolCloseHandler close_handler) noexcept
        {
            close_handler_ = close_handler;
        }

        [[nodiscard]] const entry_type* entry(std::size_t index) const noexcept
        {
            if (index >= Capacity) {
                return nullptr;
            }

            return &entries_[index];
        }

        [[nodiscard]] TaskMessageSessionProtocolLookup lookup(
            util::u64 operation) const noexcept
        {
            for (std::size_t index = 0; index < Capacity; ++index) {
                if (entries_[index].operation != operation) {
                    continue;
                }

                return TaskMessageSessionProtocolLookup{
                    .entry = &entries_[index],
                    .slot = static_cast<util::u16>(index),
                    .matched = true,
                };
            }

            return TaskMessageSessionProtocolLookup{};
        }

        [[nodiscard]] result_type dispatch_request(
            TaskMessageSessionEndpointRequestView request) noexcept
        {
            auto result = result_type{
                .kind = TaskMessageSessionProtocolTraceKind::request,
                .endpoint = request.endpoint,
                .operation = request.operation,
                .payload = request.payload,
            };

            const auto found = lookup(request.operation);
            result.slot = found.slot;
            result.matched = found.matched;
            result.operation_name = found.entry != nullptr
                                        ? found.entry->operation_name
                                        : "unmapped";
            if (!found.matched || found.entry == nullptr) {
                result.trap = task_message_session_endpoint_unsupported();
                trace_push(result);
                return result;
            }

            result.handler_valid = found.entry->handler.valid();
            if (!result.handler_valid) {
                result.trap = task_message_session_endpoint_unbound_adapter();
                trace_push(result);
                return result;
            }

            result.trap = found.entry->handler.dispatch(request);
            trace_push(result);
            return result;
        }

        [[nodiscard]] result_type dispatch_close(
            TaskMessageSessionEndpointCloseView close) noexcept
        {
            auto result = result_type{
                .kind = TaskMessageSessionProtocolTraceKind::close,
                .endpoint = close.endpoint,
                .operation = task_message_session_close_operation,
                .operation_name = "close",
                .payload = close.reason,
                .close_handler_valid = close_handler_.valid(),
            };

            result.trap = close_handler_.dispatch(close);
            trace_push(result);
            return result;
        }

        [[nodiscard]] TrapResult request(
            TaskMessageSessionEndpointRequestView request) noexcept
        {
            return dispatch_request(request).trap;
        }

        [[nodiscard]] TrapResult close(
            TaskMessageSessionEndpointCloseView close) noexcept
        {
            return dispatch_close(close).trap;
        }

    private:
        void trace_push(const result_type& result) noexcept
        {
            if (trace_ == nullptr) {
                return;
            }

            (void)trace_->push(typename TraceBuffer::value_type{
                .sequence = ++sequence_,
                .kind = result.kind,
                .service_id = result.endpoint.service_id,
                .service_name = result.endpoint.service_name,
                .session_handle = result.endpoint.session_handle,
                .open_payload = result.endpoint.open_payload,
                .channel_slot = result.endpoint.channel_slot,
                .operation = result.operation,
                .operation_name = result.operation_name,
                .payload = result.payload,
                .slot = result.slot,
                .matched = result.matched,
                .handler_valid = result.handler_valid,
                .close_handler_valid = result.close_handler_valid,
                .disposition = result.trap.disposition,
                .error = result.trap.error,
                .value = result.trap.value,
            });
        }

        std::array<entry_type, Capacity> entries_{};
        TaskMessageSessionProtocolCloseHandler close_handler_{};
        TraceBuffer* trace_{nullptr};
        util::u64 sequence_{0};
    };

    template <std::size_t Capacity>
    [[nodiscard]] auto make_task_message_session_protocol(
        std::array<TaskMessageSessionProtocolEntry, Capacity>
            entries) noexcept -> TaskMessageSessionProtocol<Capacity>
    {
        return TaskMessageSessionProtocol<Capacity>{entries};
    }

    template <std::size_t Capacity, typename TraceBuffer>
    [[nodiscard]] auto make_task_message_session_protocol(
        std::array<TaskMessageSessionProtocolEntry, Capacity> entries,
        TraceBuffer* trace) noexcept
        -> TaskMessageSessionProtocol<Capacity, TraceBuffer>
    {
        return TaskMessageSessionProtocol<Capacity, TraceBuffer>{
            entries,
            {},
            trace,
        };
    }

    template <std::size_t Capacity>
    [[nodiscard]] auto make_task_message_session_protocol(
        std::array<TaskMessageSessionProtocolEntry, Capacity> entries,
        TaskMessageSessionProtocolCloseHandler close_handler) noexcept
        -> TaskMessageSessionProtocol<Capacity>
    {
        return TaskMessageSessionProtocol<Capacity>{entries, close_handler};
    }

    template <std::size_t Capacity, typename TraceBuffer>
    [[nodiscard]] auto make_task_message_session_protocol(
        std::array<TaskMessageSessionProtocolEntry, Capacity> entries,
        TaskMessageSessionProtocolCloseHandler close_handler,
        TraceBuffer* trace) noexcept
        -> TaskMessageSessionProtocol<Capacity, TraceBuffer>
    {
        return TaskMessageSessionProtocol<Capacity, TraceBuffer>{
            entries,
            close_handler,
            trace,
        };
    }
}
