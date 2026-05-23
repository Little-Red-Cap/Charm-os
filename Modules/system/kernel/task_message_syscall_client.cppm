module;

#include <array>
#include <cstddef>
#include <string_view>

export module kernel.task_message_syscall_client;

export import kernel.task_message_syscall_frame_caller;
import semantic.core;
import util.core;

export namespace kernel {
    enum class TaskMessageSyscallClientTraceKind : util::u8 {
        reply = 0,
        timeout,
    };

    [[nodiscard]] constexpr const char* task_message_syscall_client_trace_kind_name(
        TaskMessageSyscallClientTraceKind kind) noexcept
    {
        switch (kind) {
        case TaskMessageSyscallClientTraceKind::reply:
            return "reply";
        case TaskMessageSyscallClientTraceKind::timeout:
            return "timeout";
        }
        return "unknown";
    }

    template <typename Caller>
    struct TaskMessageSyscallClientStepResult {
        bool progressed{false};
        bool completed{false};
        bool reply_consumed{false};
        bool timeout_consumed{false};
        TaskId owner{};
        util::u64 token{0};
        util::u64 request_sequence{0};
        typename Caller::reply_type reply{};
        TrapResult trap{
            .disposition = TrapDisposition::rejected,
            .error = TrapError::none,
            .value = 0,
        };
    };

    struct TaskMessageSyscallClientTraceEvent {
        util::u64 sequence{0};
        TaskMessageSyscallClientTraceKind kind{
            TaskMessageSyscallClientTraceKind::reply};
        EventId event_id{EventId::init};
        util::u64 event_value{0};
        TaskId owner{};
        util::u64 token{0};
        util::u64 request_sequence{0};
        bool ok{false};
        bool completed{false};
        bool reply_consumed{false};
        bool timeout_consumed{false};
        TaskId reply_from{};
        TaskId reply_to{};
        util::u64 reply_sequence{0};
        util::u64 reply_value{0};
        TrapDisposition disposition{TrapDisposition::rejected};
        TrapError error{TrapError::none};
    };

    static_assert(
        semantic::reflected_member_names_match_when_enabled<TaskMessageSyscallClientTraceEvent>(
            std::array<std::string_view, 17>{
                "sequence",
                "kind",
                "event_id",
                "event_value",
                "owner",
                "token",
                "request_sequence",
                "ok",
                "completed",
                "reply_consumed",
                "timeout_consumed",
                "reply_from",
                "reply_to",
                "reply_sequence",
                "reply_value",
                "disposition",
                "error",
            }));

    template <std::size_t Capacity>
    class TaskMessageSyscallClientTraceBuffer {
    public:
        using value_type = TaskMessageSyscallClientTraceEvent;

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

    struct TaskMessageSyscallClientWitness {
        util::u64 sequence{0};
        bool ready{false};
        bool has_trace{false};
        TaskMessageSyscallClientTraceKind kind{
            TaskMessageSyscallClientTraceKind::reply};
        TaskId owner{};
        util::u64 token{0};
        util::u64 request_sequence{0};
        bool completed{false};
        bool reply_consumed{false};
        bool timeout_consumed{false};
        TaskId reply_from{};
        TaskId reply_to{};
        util::u64 reply_sequence{0};
        util::u64 reply_value{0};
        TrapDisposition disposition{TrapDisposition::rejected};
        TrapError error{TrapError::none};
        bool has_lower_provenance{false};
        TaskMessageSyscallFrameCallerState lower_provenance{};

        [[nodiscard]] constexpr bool reply_branch_ok() const noexcept
        {
            return kind == TaskMessageSyscallClientTraceKind::reply &&
                   completed && reply_consumed && !timeout_consumed &&
                   reply_from != TaskId{} && reply_to == owner &&
                   reply_sequence == request_sequence;
        }

        [[nodiscard]] constexpr bool timeout_branch_ok() const noexcept
        {
            return kind == TaskMessageSyscallClientTraceKind::timeout &&
                   completed && !reply_consumed && timeout_consumed &&
                   reply_from == TaskId{} && reply_to == owner &&
                   reply_sequence == 0u && reply_value == 0u;
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

            return lower_provenance.owner == owner &&
                   lower_provenance.token == token &&
                   lower_provenance.sequence == request_sequence;
        }

        [[nodiscard]] constexpr semantic::Verdict verdict() const noexcept
        {
            if (!ready) {
                return semantic::Verdict::collapsed;
            }

            if (!(reply_branch_ok() || timeout_branch_ok())) {
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

            const bool branch_selection_mismatch =
                (kind == TaskMessageSyscallClientTraceKind::reply &&
                 (!reply_consumed || timeout_consumed)) ||
                (kind == TaskMessageSyscallClientTraceKind::timeout &&
                 (reply_consumed || !timeout_consumed));
            if (branch_selection_mismatch) {
                return semantic::FailureDomain::selection;
            }

            const bool completion_shape_mismatch =
                !completed ||
                (kind == TaskMessageSyscallClientTraceKind::reply &&
                 (reply_from == TaskId{} || reply_to != owner ||
                  reply_sequence != request_sequence)) ||
                (kind == TaskMessageSyscallClientTraceKind::timeout &&
                 (reply_from != TaskId{} || reply_to != owner ||
                  reply_sequence != 0u || reply_value != 0u));
            if (completion_shape_mismatch) {
                return semantic::FailureDomain::handoff;
            }

            if (!lower_route_consistent()) {
                return semantic::FailureDomain::route;
            }

            return semantic::FailureDomain::none;
        }

        [[nodiscard]] constexpr std::string_view summary_path() const noexcept
        {
            return "task-message-syscall-client-witness.summary";
        }
    };

    struct TaskMessageSyscallClientWitnessHandoffTarget {
        const TaskMessageSyscallClientWitness* witness{nullptr};

        [[nodiscard]] constexpr std::string_view entry_name() const noexcept
        {
            return "task-message-syscall-client-witness";
        }

        [[nodiscard]] constexpr std::string_view
        selected_summary_path() const noexcept
        {
            return witness != nullptr ? witness->summary_path()
                                      : std::string_view{
                                            "task-message-syscall-client-witness.summary"};
        }
    };

    static_assert(
        semantic::reflected_member_names_match_when_enabled<TaskMessageSyscallClientWitness>(
            std::array<std::string_view, 18>{
                "sequence",
                "ready",
                "has_trace",
                "kind",
                "owner",
                "token",
                "request_sequence",
                "completed",
                "reply_consumed",
                "timeout_consumed",
                "reply_from",
                "reply_to",
                "reply_sequence",
                "reply_value",
                "disposition",
                "error",
                "has_lower_provenance",
                "lower_provenance",
            }));

    static_assert(semantic::WitnessCarrier<TaskMessageSyscallClientWitness>);
    static_assert(
        semantic::HandoffTarget<TaskMessageSyscallClientWitnessHandoffTarget>);

    template <typename Caller>
    [[nodiscard]] constexpr TaskMessageSyscallClientWitness
    task_message_syscall_client_witness(
        const TaskMessageSyscallClientStepResult<Caller>& result) noexcept
    {
        const auto ready =
            result.completed || result.reply_consumed || result.timeout_consumed;
        return TaskMessageSyscallClientWitness{
            .ready = ready,
            .kind = result.timeout_consumed
                        ? TaskMessageSyscallClientTraceKind::timeout
                        : TaskMessageSyscallClientTraceKind::reply,
            .owner = result.owner,
            .token = result.token,
            .request_sequence = result.request_sequence,
            .completed = result.completed,
            .reply_consumed = result.reply_consumed,
            .timeout_consumed = result.timeout_consumed,
            .reply_from = result.reply.from,
            .reply_to = result.timeout_consumed ? result.owner : result.reply.to,
            .reply_sequence = result.timeout_consumed ? 0u
                                                      : result.reply.sequence,
            .reply_value = result.timeout_consumed ? 0u : result.reply.value,
            .disposition = result.trap.disposition,
            .error = result.trap.error,
        };
    }

    template <typename Caller>
    [[nodiscard]] constexpr TaskMessageSyscallClientWitness
    task_message_syscall_client_witness(
        const TaskMessageSyscallClientStepResult<Caller>& result,
        const TaskMessageSyscallFrameCallerState& lower) noexcept
    {
        auto witness = task_message_syscall_client_witness(result);
        witness.has_lower_provenance = true;
        witness.lower_provenance = lower;
        return witness;
    }

    [[nodiscard]] constexpr TaskMessageSyscallClientWitness
    task_message_syscall_client_witness(
        const TaskMessageSyscallClientTraceEvent& event) noexcept
    {
        return TaskMessageSyscallClientWitness{
            .sequence = event.sequence,
            .ready = event.sequence != 0u,
            .has_trace = true,
            .kind = event.kind,
            .owner = event.owner,
            .token = event.token,
            .request_sequence = event.request_sequence,
            .completed = event.completed,
            .reply_consumed = event.reply_consumed,
            .timeout_consumed = event.timeout_consumed,
            .reply_from = event.reply_from,
            .reply_to = event.reply_to,
            .reply_sequence = event.reply_sequence,
            .reply_value = event.reply_value,
            .disposition = event.disposition,
            .error = event.error,
        };
    }

    [[nodiscard]] constexpr TaskMessageSyscallClientWitness
    task_message_syscall_client_witness(
        const TaskMessageSyscallClientTraceEvent& event,
        const TaskMessageSyscallFrameCallerState& lower) noexcept
    {
        auto witness = task_message_syscall_client_witness(event);
        witness.has_lower_provenance = true;
        witness.lower_provenance = lower;
        return witness;
    }

    template <std::size_t Capacity>
    [[nodiscard]] constexpr TaskMessageSyscallClientWitness
    task_message_syscall_client_witness(
        const TaskMessageSyscallClientTraceBuffer<Capacity>& trace) noexcept
    {
        const auto* terminal =
            trace.size() == 0u ? nullptr : trace.at(trace.size() - 1u);
        if (terminal == nullptr) {
            return TaskMessageSyscallClientWitness{};
        }

        return task_message_syscall_client_witness(*terminal);
    }

    template <std::size_t Capacity>
    [[nodiscard]] constexpr TaskMessageSyscallClientWitness
    task_message_syscall_client_witness(
        const TaskMessageSyscallClientTraceBuffer<Capacity>& trace,
        const TaskMessageSyscallFrameCallerState& lower) noexcept
    {
        auto witness = task_message_syscall_client_witness(trace);
        if (!witness.ready) {
            return witness;
        }

        witness.has_lower_provenance = true;
        witness.lower_provenance = lower;
        return witness;
    }

    [[nodiscard]] constexpr bool task_message_syscall_client_witness_ready(
        const TaskMessageSyscallClientWitness& witness) noexcept
    {
        return witness.ready;
    }

    [[nodiscard]] constexpr TaskMessageSyscallClientWitnessHandoffTarget
    task_message_syscall_client_witness_handoff_target(
        const TaskMessageSyscallClientWitness& witness) noexcept
    {
        return TaskMessageSyscallClientWitnessHandoffTarget{
            .witness = &witness,
        };
    }

    template <typename Caller,
              typename TraceBuffer = TaskMessageSyscallClientTraceBuffer<1>>
    class TaskMessageSyscallClient {
    public:
        using caller_type = Caller;
        using trace_type = TraceBuffer;
        using tick_type = typename Caller::tick_type;
        using reply_type = typename Caller::reply_type;
        using result_type = TaskMessageSyscallClientStepResult<Caller>;

        constexpr TaskMessageSyscallClient() noexcept = default;

        constexpr explicit TaskMessageSyscallClient(Caller caller,
                                                    TraceBuffer* trace = nullptr) noexcept
            : caller_(caller), trace_(trace)
        {
        }

        [[nodiscard]] bool valid() const noexcept
        {
            return caller_.valid();
        }

        [[nodiscard]] bool busy() const noexcept
        {
            return caller_.busy();
        }

        [[nodiscard]] Caller& caller() noexcept
        {
            return caller_;
        }

        [[nodiscard]] const Caller& caller() const noexcept
        {
            return caller_;
        }

        [[nodiscard]] const TaskMessageSyscallFrameCallerState& state() const
            noexcept
        {
            return caller_.state();
        }

        [[nodiscard]] util::u64 next_token() const noexcept
        {
            return caller_.next_token();
        }

        [[nodiscard]] util::u64 next_sequence() const noexcept
        {
            return caller_.next_sequence();
        }

        void bind_caller(Caller caller) noexcept
        {
            caller_ = caller;
        }

        void bind_trace(TraceBuffer* trace) noexcept
        {
            trace_ = trace;
        }

        void bind_cursors(util::u64 next_token,
                          util::u64 next_sequence) noexcept
        {
            caller_.bind_cursors(next_token, next_sequence);
        }

        [[nodiscard]] Event reply_event() const noexcept
        {
            if (!valid()) {
                return make_runtime_mailbox_reply_event();
            }

            return caller_.messages().mailbox().reply_event();
        }

        [[nodiscard]] Event reply_timeout_event() const noexcept
        {
            if (!valid()) {
                return make_runtime_mailbox_reply_timeout_event();
            }

            return caller_.messages().mailbox().reply_timeout_event();
        }

        [[nodiscard]] bool begin(TaskSyscallRequest request,
                                 tick_type wait_due) noexcept
        {
            return caller_.begin(request, wait_due);
        }

        [[nodiscard]] bool begin(TaskId owner,
                                 TaskSyscallRequest request,
                                 tick_type wait_due) noexcept
        {
            return caller_.begin(owner, request, wait_due);
        }

        [[nodiscard]] bool begin(TaskId owner,
                                 util::u64 token,
                                 util::u64 sequence,
                                 TaskSyscallRequest request,
                                 tick_type wait_due) noexcept
        {
            return caller_.begin(owner, token, sequence, request, wait_due);
        }

        [[nodiscard]] bool sys_yield(tick_type wait_due) noexcept
        {
            return caller_.yield_current(wait_due);
        }

        [[nodiscard]] bool sys_yield(TaskId owner,
                                     tick_type wait_due) noexcept
        {
            return caller_.yield_current(owner, wait_due);
        }

        template <typename Tick>
        [[nodiscard]] bool sys_sleep_until(TrapSleepUntilView<Tick> sleep,
                                           tick_type wait_due) noexcept
        {
            return caller_.sleep_until(sleep, wait_due);
        }

        template <typename Tick>
        [[nodiscard]] bool sys_sleep_until(TaskId owner,
                                           TrapSleepUntilView<Tick> sleep,
                                           tick_type wait_due) noexcept
        {
            return caller_.sleep_until(owner, sleep, wait_due);
        }

        [[nodiscard]] bool sys_debug_write(TrapDebugWriteView write,
                                           tick_type wait_due) noexcept
        {
            return caller_.debug_write(write, wait_due);
        }

        [[nodiscard]] bool sys_debug_write(TaskId owner,
                                           TrapDebugWriteView write,
                                           tick_type wait_due) noexcept
        {
            return caller_.debug_write(owner, write, wait_due);
        }

        [[nodiscard]] bool sys_capability_call(
            TrapCapabilityCallView capability,
            tick_type wait_due) noexcept
        {
            return caller_.capability_call(capability, wait_due);
        }

        [[nodiscard]] bool sys_capability_call(
            TaskId owner,
            TrapCapabilityCallView capability,
            tick_type wait_due) noexcept
        {
            return caller_.capability_call(owner, capability, wait_due);
        }

        [[nodiscard]] result_type step(Event event) noexcept
        {
            auto result = result_type{};
            const auto pending =
                busy() ? caller_.state() : TaskMessageSyscallFrameCallerState{};
            result.owner = pending.owner;
            result.token = pending.token;
            result.request_sequence = pending.sequence;

            if (event_matches(event, reply_event())) {
                if (busy()) {
                    result.reply_consumed =
                        caller_.receive_reply(result.reply, result.trap);
                }
                result.progressed = result.reply_consumed;
                result.completed = result.reply_consumed;
                trace_reply(event, result);
                return result;
            }

            if (event_matches(event, reply_timeout_event())) {
                if (busy()) {
                    result.timeout_consumed =
                        caller_.consume_reply_timeout(event);
                }
                result.progressed = result.timeout_consumed;
                result.completed = result.timeout_consumed;
                trace_timeout(event, result);
                return result;
            }

            return result;
        }

        [[nodiscard]] bool cancel_pending() noexcept
        {
            return caller_.cancel_pending();
        }

    private:
        [[nodiscard]] static constexpr bool event_matches(Event lhs,
                                                          Event rhs) noexcept
        {
            return lhs.id == rhs.id && payload_u64(lhs) == payload_u64(rhs);
        }

        void trace_reply(Event event, const result_type& result) noexcept
        {
            trace_push(TaskMessageSyscallClientTraceEvent{
                .kind = TaskMessageSyscallClientTraceKind::reply,
                .event_id = event.id,
                .event_value = payload_u64(event),
                .owner = result.owner,
                .token = result.token,
                .request_sequence = result.request_sequence,
                .ok = result.reply_consumed,
                .completed = result.completed,
                .reply_consumed = result.reply_consumed,
                .timeout_consumed = false,
                .reply_from = result.reply.from,
                .reply_to = result.reply.to,
                .reply_sequence = result.reply.sequence,
                .reply_value = result.reply.value,
                .disposition = result.trap.disposition,
                .error = result.trap.error,
            });
        }

        void trace_timeout(Event event, const result_type& result) noexcept
        {
            trace_push(TaskMessageSyscallClientTraceEvent{
                .kind = TaskMessageSyscallClientTraceKind::timeout,
                .event_id = event.id,
                .event_value = payload_u64(event),
                .owner = result.owner,
                .token = result.token,
                .request_sequence = result.request_sequence,
                .ok = result.timeout_consumed,
                .completed = result.completed,
                .reply_consumed = false,
                .timeout_consumed = result.timeout_consumed,
                .reply_from = TaskId{},
                .reply_to = result.owner,
                .reply_sequence = 0u,
                .reply_value = 0,
                .disposition = result.trap.disposition,
                .error = result.trap.error,
            });
        }

        void trace_push(const TaskMessageSyscallClientTraceEvent& event) noexcept
        {
            if (trace_ == nullptr) {
                return;
            }

            auto traced = event;
            traced.sequence = ++sequence_;
            (void)trace_->push(traced);
        }

        Caller caller_{};
        TraceBuffer* trace_{nullptr};
        util::u64 sequence_{0};
    };

    template <typename Caller>
    [[nodiscard]] auto make_task_message_syscall_client(Caller caller) noexcept
        -> TaskMessageSyscallClient<Caller>
    {
        return TaskMessageSyscallClient<Caller>{caller};
    }

    template <typename Caller, typename TraceBuffer>
    [[nodiscard]] auto make_task_message_syscall_client(
        Caller caller,
        TraceBuffer* trace) noexcept
        -> TaskMessageSyscallClient<Caller, TraceBuffer>
    {
        return TaskMessageSyscallClient<Caller, TraceBuffer>{caller, trace};
    }
}
