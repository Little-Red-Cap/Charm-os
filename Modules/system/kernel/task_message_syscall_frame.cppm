module;

#include <array>
#include <cstddef>
#include <string_view>

export module kernel.task_message_syscall_frame;

export import kernel.task_message_table;
export import kernel.task_syscall_frame;
import semantic.core;
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

    static_assert(
        semantic::reflected_member_names_match_when_enabled<TaskMessageSyscallFrameBridgeTraceEvent>(
            std::array<std::string_view, 12>{
                "sequence",
                "from",
                "label",
                "token",
                "request_sequence",
                "slot_found",
                "port_valid",
                "result_ready",
                "disposition",
                "error",
                "reply_value",
                "handled",
            }));

    struct TaskMessageSyscallFrameWitness {
        util::u64 sequence{0};
        bool ready{false};
        bool has_trace{false};
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
        bool has_lower_provenance{false};
        TaskSyscallFrameWitness lower_provenance{};

        [[nodiscard]] constexpr bool published_frame_branch_ok() const noexcept
        {
            return slot_found && port_valid && result_ready && handled;
        }

        [[nodiscard]] constexpr bool missing_frame_branch_ok() const noexcept
        {
            return !slot_found && port_valid && !result_ready &&
                   disposition == TrapDisposition::rejected &&
                   error == TrapError::invalid_argument && !handled;
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

        [[nodiscard]] constexpr bool lower_route_consistent() const noexcept
        {
            if (!has_lower_provenance) {
                return true;
            }

            return lower_provenance.has_terminal &&
                   lower_provenance.terminal_disposition == disposition &&
                   lower_provenance.terminal_error == error &&
                   lower_provenance.terminal_value == reply_value;
        }

        [[nodiscard]] constexpr semantic::Verdict verdict() const noexcept
        {
            if (!ready) {
                return semantic::Verdict::collapsed;
            }

            if (!(published_frame_branch_ok() || missing_frame_branch_ok())) {
                return semantic::Verdict::drifted;
            }

            if (!lower_route_consistent()) {
                return semantic::Verdict::drifted;
            }

            return semantic::Verdict::standing;
        }

        [[nodiscard]] constexpr semantic::FailureDomain
        failure_domain() const noexcept
        {
            if (!ready) {
                return semantic::FailureDomain::input;
            }

            if (!port_valid) {
                return semantic::FailureDomain::route;
            }

            const bool published_shape_mismatch =
                slot_found &&
                !(port_valid && result_ready && handled);
            const bool missing_shape_mismatch =
                !slot_found &&
                !(port_valid && !result_ready &&
                  disposition == TrapDisposition::rejected &&
                  error == TrapError::invalid_argument && !handled);

            if (published_shape_mismatch || missing_shape_mismatch) {
                if ((handled && !result_ready) ||
                    (!handled && result_ready) ||
                    (slot_found && disposition == TrapDisposition::rejected &&
                     error == TrapError::invalid_argument)) {
                    return semantic::FailureDomain::handoff;
                }

                return slot_found ? semantic::FailureDomain::route
                                  : semantic::FailureDomain::selection;
            }

            if (!lower_route_consistent()) {
                return semantic::FailureDomain::route;
            }

            return semantic::FailureDomain::none;
        }

        [[nodiscard]] constexpr std::string_view summary_path() const noexcept
        {
            return "task-message-syscall-frame-witness.summary";
        }
    };

    struct TaskMessageSyscallFrameWitnessHandoffTarget {
        const TaskMessageSyscallFrameWitness* witness{nullptr};

        [[nodiscard]] constexpr std::string_view entry_name() const noexcept
        {
            return "task-message-syscall-frame-witness";
        }

        [[nodiscard]] constexpr std::string_view
        selected_summary_path() const noexcept
        {
            return witness != nullptr ? witness->summary_path()
                                      : std::string_view{
                                            "task-message-syscall-frame-witness.summary"};
        }
    };

    static_assert(
        semantic::reflected_member_names_match_when_enabled<TaskMessageSyscallFrameWitness>(
            std::array<std::string_view, 16>{
                "sequence",
                "ready",
                "has_trace",
                "from",
                "label",
                "token",
                "request_sequence",
                "slot_found",
                "port_valid",
                "result_ready",
                "disposition",
                "error",
                "reply_value",
                "handled",
                "has_lower_provenance",
                "lower_provenance",
            }));

    static_assert(semantic::WitnessCarrier<TaskMessageSyscallFrameWitness>);
    static_assert(
        semantic::HandoffTarget<TaskMessageSyscallFrameWitnessHandoffTarget>);

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

    [[nodiscard]] constexpr TaskMessageSyscallFrameWitness
    task_message_syscall_frame_witness(
        const TaskMessageSyscallFrameBridgeResult& result) noexcept
    {
        return TaskMessageSyscallFrameWitness{
            .ready = true,
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
        };
    }

    [[nodiscard]] constexpr TaskMessageSyscallFrameWitness
    task_message_syscall_frame_witness(
        const TaskMessageSyscallFrameBridgeResult& result,
        const TaskSyscallFrameWitness& lower) noexcept
    {
        auto witness = task_message_syscall_frame_witness(result);
        witness.has_lower_provenance = true;
        witness.lower_provenance = lower;
        return witness;
    }

    [[nodiscard]] constexpr TaskMessageSyscallFrameWitness
    task_message_syscall_frame_witness(
        const TaskMessageSyscallFrameBridgeTraceEvent& event) noexcept
    {
        return TaskMessageSyscallFrameWitness{
            .sequence = event.sequence,
            .ready = event.sequence != 0u,
            .has_trace = true,
            .from = event.from,
            .label = event.label,
            .token = event.token,
            .request_sequence = event.request_sequence,
            .slot_found = event.slot_found,
            .port_valid = event.port_valid,
            .result_ready = event.result_ready,
            .disposition = event.disposition,
            .error = event.error,
            .reply_value = event.reply_value,
            .handled = event.handled,
        };
    }

    [[nodiscard]] constexpr TaskMessageSyscallFrameWitness
    task_message_syscall_frame_witness(
        const TaskMessageSyscallFrameBridgeTraceEvent& event,
        const TaskSyscallFrameWitness& lower) noexcept
    {
        auto witness = task_message_syscall_frame_witness(event);
        witness.has_lower_provenance = true;
        witness.lower_provenance = lower;
        return witness;
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

    template <std::size_t Capacity>
    [[nodiscard]] constexpr TaskMessageSyscallFrameWitness
    task_message_syscall_frame_witness(
        const TaskMessageSyscallFrameBridgeTraceBuffer<Capacity>& trace) noexcept
    {
        const auto* terminal =
            trace.size() == 0u ? nullptr : trace.at(trace.size() - 1u);
        if (terminal == nullptr) {
            return TaskMessageSyscallFrameWitness{};
        }

        return task_message_syscall_frame_witness(*terminal);
    }

    template <std::size_t Capacity>
    [[nodiscard]] constexpr TaskMessageSyscallFrameWitness
    task_message_syscall_frame_witness(
        const TaskMessageSyscallFrameBridgeTraceBuffer<Capacity>& trace,
        const TaskSyscallFrameWitness& lower) noexcept
    {
        auto witness = task_message_syscall_frame_witness(trace);
        if (!witness.ready) {
            return witness;
        }

        witness.has_lower_provenance = true;
        witness.lower_provenance = lower;
        return witness;
    }

    [[nodiscard]] constexpr bool task_message_syscall_frame_witness_ready(
        const TaskMessageSyscallFrameWitness& witness) noexcept
    {
        return witness.ready;
    }

    [[nodiscard]] constexpr TaskMessageSyscallFrameWitnessHandoffTarget
    task_message_syscall_frame_witness_handoff_target(
        const TaskMessageSyscallFrameWitness& witness) noexcept
    {
        return TaskMessageSyscallFrameWitnessHandoffTarget{
            .witness = &witness,
        };
    }

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
