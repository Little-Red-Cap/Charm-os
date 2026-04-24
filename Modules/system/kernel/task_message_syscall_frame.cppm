module;

#include <array>
#include <cstddef>

export module kernel.task_message_syscall_frame;

export import kernel.task_message_table;
export import kernel.task_syscall_frame;
import util.core;

export namespace kernel {
    inline constexpr util::u64 task_message_syscall_frame_request_label{
        0x53594652u};

    [[nodiscard]] constexpr const char*
    task_message_syscall_frame_request_label_name() noexcept
    {
        return "syscall-frame";
    }

    [[nodiscard]] constexpr RuntimeMailboxRequest
    make_task_message_syscall_frame_request(TaskId from,
                                            util::u64 token,
                                            util::u64 sequence = 0) noexcept
    {
        return RuntimeMailboxRequest{
            .from = from,
            .label = task_message_syscall_frame_request_label,
            .value = token,
            .sequence = sequence,
        };
    }

    [[nodiscard]] constexpr util::u64 task_message_syscall_frame_token(
        const RuntimeMailboxRequest& request) noexcept
    {
        return request.value;
    }

    template <typename Frame, std::size_t Capacity>
    class TaskMessageSyscallFrameStore {
    public:
        using frame_type = Frame;

        static_assert(Capacity > 0);

        [[nodiscard]] static consteval std::size_t capacity() noexcept
        {
            return Capacity;
        }

        [[nodiscard]] bool valid() const noexcept
        {
            return true;
        }

        [[nodiscard]] std::size_t pending() const noexcept
        {
            return size_;
        }

        [[nodiscard]] bool contains(TaskId owner, util::u64 token) const noexcept
        {
            return find_slot(owner, token) < Capacity;
        }

        [[nodiscard]] bool publish(TaskId owner,
                                   util::u64 token,
                                   const Frame& frame) noexcept
        {
            if (contains(owner, token)) {
                return false;
            }

            const auto index = find_free_slot();
            if (index >= Capacity) {
                return false;
            }

            slots_[index] = Slot{
                .engaged = true,
                .owner = owner,
                .token = token,
                .frame = frame,
            };
            ++size_;
            return true;
        }

        [[nodiscard]] Frame* find(TaskId owner, util::u64 token) noexcept
        {
            const auto index = find_slot(owner, token);
            if (index >= Capacity) {
                return nullptr;
            }

            return &slots_[index].frame;
        }

        [[nodiscard]] const Frame* find(TaskId owner,
                                        util::u64 token) const noexcept
        {
            const auto index = find_slot(owner, token);
            if (index >= Capacity) {
                return nullptr;
            }

            return &slots_[index].frame;
        }

        [[nodiscard]] bool take(TaskId owner,
                                util::u64 token,
                                Frame& out) noexcept
        {
            const auto index = find_slot(owner, token);
            if (index >= Capacity) {
                return false;
            }

            out = slots_[index].frame;
            erase_slot(index);
            return true;
        }

        [[nodiscard]] bool erase(TaskId owner, util::u64 token) noexcept
        {
            const auto index = find_slot(owner, token);
            if (index >= Capacity) {
                return false;
            }

            erase_slot(index);
            return true;
        }

    private:
        struct Slot {
            bool engaged{false};
            TaskId owner{};
            util::u64 token{0};
            Frame frame{};
        };

        [[nodiscard]] std::size_t find_slot(TaskId owner,
                                            util::u64 token) const noexcept
        {
            for (std::size_t index = 0; index < Capacity; ++index) {
                if (!slots_[index].engaged || slots_[index].owner != owner ||
                    slots_[index].token != token) {
                    continue;
                }

                return index;
            }

            return Capacity;
        }

        [[nodiscard]] std::size_t find_free_slot() const noexcept
        {
            for (std::size_t index = 0; index < Capacity; ++index) {
                if (!slots_[index].engaged) {
                    return index;
                }
            }

            return Capacity;
        }

        void erase_slot(std::size_t index) noexcept
        {
            if (index >= Capacity || !slots_[index].engaged) {
                return;
            }

            slots_[index] = Slot{};
            --size_;
        }

        std::array<Slot, Capacity> slots_{};
        std::size_t size_{0};
    };

    template <typename Frame>
    struct TaskMessageSyscallFrameResultAdapter {
        void* ctx{nullptr};
        bool (*result_ready)(void* ctx,
                             const Frame& frame,
                             const TrapResult& result) noexcept {nullptr};
    };

    template <typename Frame>
    [[nodiscard]] constexpr bool task_message_syscall_frame_result_adapter_ready(
        const TaskMessageSyscallFrameResultAdapter<Frame>& adapter) noexcept
    {
        return adapter.result_ready != nullptr;
    }

    struct TaskMessageSyscallFrameBridgeResult {
        RuntimeMailboxRequest request{};
        util::u64 token{0};
        bool slot_found{false};
        bool port_valid{false};
        bool result_ready{false};
        TrapResult trap{};
        TaskMessageHandleResult message{};
    };

    struct TaskMessageSyscallFrameBridgeTraceEvent {
        util::u64 sequence{0};
        TaskId from{};
        util::u64 label{0};
        util::u64 token{0};
        util::u64 request_sequence{0};
        bool slot_found{false};
        bool port_valid{false};
        bool result_ready{false};
        TrapDisposition disposition{TrapDisposition::rejected};
        TrapError error{TrapError::none};
        util::u64 reply_value{0};
        bool handled{false};
    };

    [[nodiscard]] constexpr RuntimeMailboxRequest
    task_message_request_from_trace_event(
        const TaskMessageSyscallFrameBridgeTraceEvent& event) noexcept
    {
        return RuntimeMailboxRequest{
            .from = event.from,
            .label = event.label,
            .value = event.token,
            .sequence = event.request_sequence,
        };
    }

    template <std::size_t Capacity>
    class TaskMessageSyscallFrameBridgeTraceBuffer {
    public:
        using value_type = TaskMessageSyscallFrameBridgeTraceEvent;

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

    template <typename Frames,
              typename FramePort,
              typename ResultAdapter =
                  TaskMessageSyscallFrameResultAdapter<
                      typename Frames::frame_type>,
              typename TraceBuffer =
                  TaskMessageSyscallFrameBridgeTraceBuffer<1>>
    class TaskMessageSyscallFrameBridge {
    public:
        using frames_type = Frames;
        using frame_type = typename Frames::frame_type;
        using port_type = FramePort;
        using result_adapter_type = ResultAdapter;
        using trace_type = TraceBuffer;
        using result_type = TaskMessageSyscallFrameBridgeResult;

        constexpr TaskMessageSyscallFrameBridge() noexcept = default;

        constexpr explicit TaskMessageSyscallFrameBridge(
            Frames& frames,
            FramePort port,
            ResultAdapter result_adapter = {},
            TraceBuffer* trace = nullptr) noexcept
            : frames_(&frames), port_(port), result_adapter_(result_adapter),
              trace_(trace)
        {
        }

        [[nodiscard]] bool valid() const noexcept
        {
            return frames_ != nullptr && port_.valid();
        }

        [[nodiscard]] Frames& frames() noexcept
        {
            return *frames_;
        }

        [[nodiscard]] const Frames& frames() const noexcept
        {
            return *frames_;
        }

        [[nodiscard]] const FramePort& port() const noexcept
        {
            return port_;
        }

        [[nodiscard]] const ResultAdapter& result_adapter() const noexcept
        {
            return result_adapter_;
        }

        void bind_frames(Frames& frames) noexcept
        {
            frames_ = &frames;
        }

        void bind_port(FramePort port) noexcept
        {
            port_ = port;
        }

        void bind_result_adapter(ResultAdapter result_adapter) noexcept
        {
            result_adapter_ = result_adapter;
        }

        void bind_trace(TraceBuffer* trace) noexcept
        {
            trace_ = trace;
        }

        [[nodiscard]] result_type dispatch_message(
            const RuntimeMailboxRequest& request) noexcept
        {
            auto result = result_type{
                .request = request,
                .token = task_message_syscall_frame_token(request),
                .port_valid = port_.valid(),
                .trap =
                    TrapResult{
                        .disposition = TrapDisposition::rejected,
                        .error = TrapError::unbound_bridge,
                        .value = 0,
                    },
            };

            if (frames_ == nullptr) {
                trace_push(result);
                return result;
            }

            auto* frame = frames().find(request.from, result.token);
            result.slot_found = frame != nullptr;
            if (frame == nullptr) {
                result.trap = TrapResult{
                    .disposition = TrapDisposition::rejected,
                    .error = TrapError::invalid_argument,
                    .value = 0,
                };
                trace_push(result);
                return result;
            }

            if (!result.port_valid) {
                result.trap = TrapResult{
                    .disposition = TrapDisposition::rejected,
                    .error = TrapError::unbound_adapter,
                    .value = 0,
                };
                trace_push(result);
                return result;
            }

            result.trap = port_.dispatch_frame(*frame);
            result.result_ready = result_completed(*frame, result.trap);
            if (result.result_ready) {
                result.message = handled_task_message(result.trap.value);
            }
            trace_push(result);
            return result;
        }

        [[nodiscard]] TaskMessageHandleResult dispatch(
            const RuntimeMailboxRequest& request) noexcept
        {
            return dispatch_message(request).message;
        }

    private:
        [[nodiscard]] bool result_completed(
            const frame_type& frame,
            const TrapResult& result) const noexcept
        {
            if (task_message_syscall_frame_result_adapter_ready(
                    result_adapter_)) {
                return result_adapter_.result_ready(
                    result_adapter_.ctx, frame, result);
            }

            return result.ok();
        }

        void trace_push(const result_type& result) noexcept
        {
            if (trace_ == nullptr) {
                return;
            }

            ++sequence_;
            (void)trace_->push(typename TraceBuffer::value_type{
                .sequence = sequence_,
                .from = result.request.from,
                .label = result.request.label,
                .token = result.token,
                .request_sequence = result.request.sequence,
                .slot_found = result.slot_found,
                .port_valid = result.port_valid,
                .result_ready = result.result_ready,
                .disposition = result.trap.disposition,
                .error = result.trap.error,
                .reply_value = result.trap.value,
                .handled = result.message.handled,
            });
        }

        Frames* frames_{nullptr};
        FramePort port_{};
        ResultAdapter result_adapter_{};
        TraceBuffer* trace_{nullptr};
        util::u64 sequence_{0};
    };

    template <typename Frames, typename FramePort>
    [[nodiscard]] auto make_task_message_syscall_frame_bridge(
        Frames& frames,
        FramePort port) noexcept
        -> TaskMessageSyscallFrameBridge<Frames, FramePort>
    {
        return TaskMessageSyscallFrameBridge<Frames, FramePort>{
            frames,
            port,
        };
    }

    template <typename Frames,
              typename FramePort,
              typename ResultAdapter,
              typename TraceBuffer>
    [[nodiscard]] auto make_task_message_syscall_frame_bridge(
        Frames& frames,
        FramePort port,
        ResultAdapter result_adapter,
        TraceBuffer* trace) noexcept
        -> TaskMessageSyscallFrameBridge<Frames,
                                         FramePort,
                                         ResultAdapter,
                                         TraceBuffer>
    {
        return TaskMessageSyscallFrameBridge<Frames,
                                             FramePort,
                                             ResultAdapter,
                                             TraceBuffer>{
            frames,
            port,
            result_adapter,
            trace,
        };
    }
}
