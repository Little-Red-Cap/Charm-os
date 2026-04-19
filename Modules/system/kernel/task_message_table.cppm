module;

#include <array>
#include <cstddef>

export module kernel.task_message_table;

export import kernel.task_message_api;
import util.core;

export namespace kernel {
    inline constexpr util::u16 task_message_table_unmapped_slot =
        static_cast<util::u16>(0xFFFFu);

    struct TaskMessageHandleResult {
        bool handled{false};
        util::u64 reply_value{0};
    };

    [[nodiscard]] constexpr TaskMessageHandleResult handled_task_message(
        util::u64 reply_value = 0) noexcept
    {
        return TaskMessageHandleResult{
            .handled = true,
            .reply_value = reply_value,
        };
    }

    [[nodiscard]] constexpr TaskMessageHandleResult unhandled_task_message()
        noexcept
    {
        return TaskMessageHandleResult{};
    }

    struct TaskMessageHandler {
        void* self{nullptr};
        TaskMessageHandleResult (*dispatch_fn)(
            void* self,
            const RuntimeMailboxRequest& request) noexcept {nullptr};

        [[nodiscard]] bool valid() const noexcept
        {
            return dispatch_fn != nullptr;
        }

        [[nodiscard]] TaskMessageHandleResult dispatch(
            const RuntimeMailboxRequest& request) const noexcept
        {
            if (!valid()) {
                return unhandled_task_message();
            }

            return dispatch_fn(self, request);
        }
    };

    namespace detail {
        template <typename Target>
        [[nodiscard]] TaskMessageHandleResult
        task_message_handler_dispatch_adapter(
            void* self,
            const RuntimeMailboxRequest& request) noexcept
        {
            return static_cast<Target*>(self)->dispatch(request);
        }
    }

    template <typename Target>
    [[nodiscard]] auto make_task_message_handler(Target& target) noexcept
        -> TaskMessageHandler
    {
        return TaskMessageHandler{
            .self = &target,
            .dispatch_fn = &detail::task_message_handler_dispatch_adapter<Target>,
        };
    }

    struct TaskMessageHandlerEntry {
        util::u64 label{0};
        const char* label_name{"message"};
        TaskMessageHandler handler{};
    };

    [[nodiscard]] constexpr TaskMessageHandlerEntry task_message_handler_entry(
        util::u64 label,
        TaskMessageHandler handler = {}) noexcept
    {
        return TaskMessageHandlerEntry{
            .label = label,
            .label_name = "message",
            .handler = handler,
        };
    }

    [[nodiscard]] constexpr TaskMessageHandlerEntry task_message_handler_entry(
        util::u64 label,
        const char* label_name,
        TaskMessageHandler handler = {}) noexcept
    {
        return TaskMessageHandlerEntry{
            .label = label,
            .label_name = label_name,
            .handler = handler,
        };
    }

    struct TaskMessageTableLookup {
        const TaskMessageHandlerEntry* entry{nullptr};
        util::u16 slot{task_message_table_unmapped_slot};
        bool matched{false};
    };

    struct TaskMessageTableTraceEvent {
        util::u64 sequence{0};
        TaskId from{};
        util::u64 label{0};
        const char* label_name{"unmapped"};
        util::u64 value{0};
        util::u64 request_sequence{0};
        util::u16 slot{task_message_table_unmapped_slot};
        bool matched{false};
        bool handler_valid{false};
        bool handled{false};
        util::u64 reply_value{0};
    };

    [[nodiscard]] constexpr RuntimeMailboxRequest
    task_message_request_from_trace_event(
        const TaskMessageTableTraceEvent& event) noexcept
    {
        return RuntimeMailboxRequest{
            .from = event.from,
            .label = event.label,
            .value = event.value,
            .sequence = event.request_sequence,
        };
    }

    template <std::size_t Capacity>
    class TaskMessageTableTraceBuffer {
    public:
        using value_type = TaskMessageTableTraceEvent;

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
              typename TraceBuffer = TaskMessageTableTraceBuffer<1>>
    class TaskMessageTable {
    public:
        using entry_type = TaskMessageHandlerEntry;
        using trace_type = TraceBuffer;

        constexpr TaskMessageTable() noexcept = default;

        constexpr explicit TaskMessageTable(
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

        [[nodiscard]] TaskMessageTableLookup lookup(util::u64 label) const
            noexcept
        {
            for (std::size_t index = 0; index < Capacity; ++index) {
                if (entries_[index].label != label) {
                    continue;
                }

                return TaskMessageTableLookup{
                    .entry = &entries_[index],
                    .slot = static_cast<util::u16>(index),
                    .matched = true,
                };
            }

            return TaskMessageTableLookup{};
        }

        [[nodiscard]] TaskMessageHandleResult dispatch(
            const RuntimeMailboxRequest& request) noexcept
        {
            const auto found = lookup(request.label);
            if (!found.matched || found.entry == nullptr) {
                const auto result = unhandled_task_message();
                trace_push(request, found, false, result);
                return result;
            }

            if (!found.entry->handler.valid()) {
                const auto result = unhandled_task_message();
                trace_push(request, found, false, result);
                return result;
            }

            const auto result = found.entry->handler.dispatch(request);
            trace_push(request, found, true, result);
            return result;
        }

        [[nodiscard]] TaskMessageHandleResult dispatch(
            TaskId from,
            util::u64 label,
            util::u64 value,
            util::u64 sequence = 0) noexcept
        {
            return dispatch(RuntimeMailboxRequest{
                .from = from,
                .label = label,
                .value = value,
                .sequence = sequence,
            });
        }

    private:
        void trace_push(const RuntimeMailboxRequest& request,
                        TaskMessageTableLookup found,
                        bool handler_valid,
                        const TaskMessageHandleResult& result) noexcept
        {
            if (trace_ == nullptr) {
                return;
            }

            ++sequence_;
            (void)trace_->push(typename TraceBuffer::value_type{
                .sequence = sequence_,
                .from = request.from,
                .label = request.label,
                .label_name = found.entry != nullptr ? found.entry->label_name
                                                     : "unmapped",
                .value = request.value,
                .request_sequence = request.sequence,
                .slot = found.slot,
                .matched = found.matched,
                .handler_valid = handler_valid,
                .handled = result.handled,
                .reply_value = result.reply_value,
            });
        }

        std::array<entry_type, Capacity> entries_{};
        TraceBuffer* trace_{nullptr};
        util::u64 sequence_{0};
    };

    template <std::size_t Capacity>
    [[nodiscard]] auto make_task_message_table(
        std::array<TaskMessageHandlerEntry, Capacity> entries) noexcept
        -> TaskMessageTable<Capacity>
    {
        return TaskMessageTable<Capacity>{entries};
    }

    template <std::size_t Capacity, typename TraceBuffer>
    [[nodiscard]] auto make_task_message_table(
        std::array<TaskMessageHandlerEntry, Capacity> entries,
        TraceBuffer* trace) noexcept -> TaskMessageTable<Capacity, TraceBuffer>
    {
        return TaskMessageTable<Capacity, TraceBuffer>{entries, trace};
    }
}
