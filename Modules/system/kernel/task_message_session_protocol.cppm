module;

#include <array>
#include <cstddef>

export module kernel.task_message_session_protocol;

export import kernel.task_message_session_endpoint;
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
