module;

#include <array>
#include <cstddef>

export module kernel.task_message_dispatch;

export import kernel.task_message_table;
import util.core;

export namespace kernel {
    struct TaskMessageDispatchResult {
        bool accepted{false};
        bool matched{false};
        bool handler_valid{false};
        bool handled{false};
        bool replied{false};
        RuntimeMailboxRequest request{};
        util::u64 reply_value{0};
    };

    struct TaskMessageDispatchTraceEvent {
        util::u64 sequence{0};
        TaskId from{};
        util::u64 label{0};
        const char* label_name{"unmapped"};
        util::u64 value{0};
        util::u64 request_sequence{0};
        bool accepted{false};
        bool matched{false};
        bool handler_valid{false};
        bool handled{false};
        bool replied{false};
        util::u64 reply_value{0};
    };

    [[nodiscard]] constexpr RuntimeMailboxRequest
    task_message_request_from_trace_event(
        const TaskMessageDispatchTraceEvent& event) noexcept
    {
        return RuntimeMailboxRequest{
            .from = event.from,
            .label = event.label,
            .value = event.value,
            .sequence = event.request_sequence,
        };
    }

    template <std::size_t Capacity>
    class TaskMessageDispatchTraceBuffer {
    public:
        using value_type = TaskMessageDispatchTraceEvent;

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

    template <typename Messages,
              typename Table,
              typename TraceBuffer = TaskMessageDispatchTraceBuffer<1>>
    class TaskMessageDispatcher {
    public:
        using message_type = Messages;
        using table_type = Table;
        using request_type = typename Messages::request_type;
        using trace_type = TraceBuffer;
        using result_type = TaskMessageDispatchResult;

        constexpr TaskMessageDispatcher() noexcept = default;

        constexpr explicit TaskMessageDispatcher(
            Messages messages,
            Table table,
            TraceBuffer* trace = nullptr) noexcept
            : messages_(messages), table_(table), trace_(trace)
        {
        }

        [[nodiscard]] bool valid() const noexcept
        {
            return messages_.valid();
        }

        [[nodiscard]] Messages& messages() noexcept
        {
            return messages_;
        }

        [[nodiscard]] const Messages& messages() const noexcept
        {
            return messages_;
        }

        [[nodiscard]] Table& table() noexcept
        {
            return table_;
        }

        [[nodiscard]] const Table& table() const noexcept
        {
            return table_;
        }

        void bind_messages(Messages messages) noexcept
        {
            messages_ = messages;
        }

        void bind_table(Table table) noexcept
        {
            table_ = table;
        }

        void bind_trace(TraceBuffer* trace) noexcept
        {
            trace_ = trace;
        }

        [[nodiscard]] result_type dispatch_request(
            const request_type& request) noexcept
        {
            const auto found = table_.lookup(request.label);
            const bool handler_valid =
                found.entry != nullptr && found.entry->handler.valid();
            const auto handled = table_.dispatch(request);
            const bool replied =
                handled.handled && messages_.reply(request, handled.reply_value);

            const auto result = result_type{
                .accepted = true,
                .matched = found.matched,
                .handler_valid = handler_valid,
                .handled = handled.handled,
                .replied = replied,
                .request = request,
                .reply_value = handled.reply_value,
            };
            trace_push(
                result,
                found.entry != nullptr ? found.entry->label_name : "unmapped");
            return result;
        }

        [[nodiscard]] result_type serve_once() noexcept
        {
            if (!valid()) {
                return result_type{};
            }

            request_type request{};
            if (!messages_.receive(request)) {
                return result_type{};
            }

            return dispatch_request(request);
        }

    private:
        void trace_push(const result_type& result,
                        const char* label_name) noexcept
        {
            if (trace_ == nullptr) {
                return;
            }

            ++sequence_;
            (void)trace_->push(typename TraceBuffer::value_type{
                .sequence = sequence_,
                .from = result.request.from,
                .label = result.request.label,
                .label_name = label_name,
                .value = result.request.value,
                .request_sequence = result.request.sequence,
                .accepted = result.accepted,
                .matched = result.matched,
                .handler_valid = result.handler_valid,
                .handled = result.handled,
                .replied = result.replied,
                .reply_value = result.reply_value,
            });
        }

        Messages messages_{};
        Table table_{};
        TraceBuffer* trace_{nullptr};
        util::u64 sequence_{0};
    };

    template <typename Messages, typename Table>
    [[nodiscard]] auto make_task_message_dispatcher(
        Messages messages,
        Table table) noexcept -> TaskMessageDispatcher<Messages, Table>
    {
        return TaskMessageDispatcher<Messages, Table>{messages, table};
    }

    template <typename Messages, typename Table, typename TraceBuffer>
    [[nodiscard]] auto make_task_message_dispatcher(
        Messages messages,
        Table table,
        TraceBuffer* trace) noexcept
        -> TaskMessageDispatcher<Messages, Table, TraceBuffer>
    {
        return TaskMessageDispatcher<Messages, Table, TraceBuffer>{
            messages,
            table,
            trace,
        };
    }
}
