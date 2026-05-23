module;

#include <array>
#include <cstddef>
#include <string_view>

export module kernel.task_message_syscall_pump;

export import kernel.task_message_syscall_client;
import semantic.core;
import util.core;

export namespace kernel {
    enum class TaskMessageSyscallPumpTraceKind : util::u8 {
        issue = 0,
        reply,
        timeout,
        completion_drop,
    };

    [[nodiscard]] constexpr const char* task_message_syscall_pump_trace_kind_name(
        TaskMessageSyscallPumpTraceKind kind) noexcept
    {
        switch (kind) {
        case TaskMessageSyscallPumpTraceKind::issue:
            return "issue";
        case TaskMessageSyscallPumpTraceKind::reply:
            return "reply";
        case TaskMessageSyscallPumpTraceKind::timeout:
            return "timeout";
        case TaskMessageSyscallPumpTraceKind::completion_drop:
            return "completion-drop";
        }
        return "unknown";
    }

    template <typename Tick>
    struct TaskMessageSyscallPumpRequest {
        TaskId owner{};
        util::u64 token{0};
        util::u64 request_sequence{0};
        TaskSyscallRequest request{};
        Tick wait_due{};
        bool explicit_owner{false};
        bool explicit_ids{false};
    };

    template <typename Tick>
    [[nodiscard]] constexpr auto make_task_message_syscall_pump_request(
        TaskSyscallRequest request,
        Tick wait_due) noexcept -> TaskMessageSyscallPumpRequest<Tick>
    {
        return TaskMessageSyscallPumpRequest<Tick>{
            .request = request,
            .wait_due = wait_due,
        };
    }

    template <typename Tick>
    [[nodiscard]] constexpr auto make_task_message_syscall_pump_request(
        TaskId owner,
        TaskSyscallRequest request,
        Tick wait_due) noexcept -> TaskMessageSyscallPumpRequest<Tick>
    {
        return TaskMessageSyscallPumpRequest<Tick>{
            .owner = owner,
            .request = request,
            .wait_due = wait_due,
            .explicit_owner = true,
        };
    }

    template <typename Tick>
    [[nodiscard]] constexpr auto make_task_message_syscall_pump_request(
        TaskId owner,
        util::u64 token,
        util::u64 request_sequence,
        TaskSyscallRequest request,
        Tick wait_due) noexcept -> TaskMessageSyscallPumpRequest<Tick>
    {
        return TaskMessageSyscallPumpRequest<Tick>{
            .owner = owner,
            .token = token,
            .request_sequence = request_sequence,
            .request = request,
            .wait_due = wait_due,
            .explicit_owner = true,
            .explicit_ids = true,
        };
    }

    template <typename Reply>
    struct TaskMessageSyscallPumpCompletion {
        bool timeout{false};
        TaskId owner{};
        util::u64 token{0};
        util::u64 request_sequence{0};
        Reply reply{};
        TrapResult trap{
            .disposition = TrapDisposition::rejected,
            .error = TrapError::none,
            .value = 0,
        };
    };

    template <typename Client>
    struct TaskMessageSyscallPumpStepResult {
        bool progressed{false};
        bool issued{false};
        bool completion_ready{false};
        bool completion_pushed{false};
        bool completion_dropped{false};
        TaskMessageSyscallPumpRequest<typename Client::tick_type> issued_request{};
        TaskMessageSyscallPumpCompletion<typename Client::reply_type> completion{};
        typename Client::result_type client{};
    };

    struct TaskMessageSyscallPumpTraceEvent {
        util::u64 sequence{0};
        TaskMessageSyscallPumpTraceKind kind{
            TaskMessageSyscallPumpTraceKind::issue};
        TaskId owner{};
        util::u64 token{0};
        util::u64 request_sequence{0};
        util::u64 wait_due{0};
        std::size_t pending_requests{0};
        std::size_t pending_completions{0};
        bool ok{false};
        bool timeout{false};
        util::u64 reply_value{0};
        TrapDisposition disposition{TrapDisposition::rejected};
        TrapError error{TrapError::none};
    };

    template <std::size_t Capacity>
    class TaskMessageSyscallPumpTraceBuffer {
    public:
        using value_type = TaskMessageSyscallPumpTraceEvent;

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

    static_assert(
        semantic::reflected_member_names_match_when_enabled<TaskMessageSyscallPumpTraceEvent>(
            std::array<std::string_view, 13>{
                "sequence",
                "kind",
                "owner",
                "token",
                "request_sequence",
                "wait_due",
                "pending_requests",
                "pending_completions",
                "ok",
                "timeout",
                "reply_value",
                "disposition",
                "error",
            }));

    struct TaskMessageSyscallPumpWitness {
        util::u64 sequence{0};
        bool ready{false};
        bool has_trace{false};
        TaskMessageSyscallPumpTraceKind kind{
            TaskMessageSyscallPumpTraceKind::issue};
        TaskId owner{};
        util::u64 token{0};
        util::u64 request_sequence{0};
        util::u64 wait_due{0};
        std::size_t pending_requests{0};
        std::size_t pending_completions{0};
        bool terminal_ok{false};
        bool timeout{false};
        util::u64 reply_value{0};
        TrapDisposition disposition{TrapDisposition::rejected};
        TrapError error{TrapError::none};
        bool has_lower_provenance{false};
        TaskMessageSyscallClientWitness lower_provenance{};

        [[nodiscard]] constexpr bool issue_branch_ok() const noexcept
        {
            return kind == TaskMessageSyscallPumpTraceKind::issue &&
                   terminal_ok &&
                   owner != TaskId{} && token != 0u && request_sequence != 0u;
        }

        [[nodiscard]] constexpr bool reply_branch_ok() const noexcept
        {
            return kind == TaskMessageSyscallPumpTraceKind::reply &&
                   terminal_ok &&
                   !timeout;
        }

        [[nodiscard]] constexpr bool timeout_branch_ok() const noexcept
        {
            return kind == TaskMessageSyscallPumpTraceKind::timeout &&
                   terminal_ok &&
                   timeout;
        }

        [[nodiscard]] constexpr bool lower_route_consistent() const noexcept
        {
            if (!has_lower_provenance) {
                return true;
            }

            if (kind == TaskMessageSyscallPumpTraceKind::issue) {
                return true;
            }

            return lower_provenance.ready &&
                   lower_provenance.owner == owner &&
                   lower_provenance.token == token &&
                   lower_provenance.request_sequence == request_sequence &&
                   (lower_provenance.kind ==
                    (timeout ? TaskMessageSyscallClientTraceKind::timeout
                             : TaskMessageSyscallClientTraceKind::reply)) &&
                   lower_provenance.disposition == disposition &&
                   lower_provenance.error == error &&
                   lower_provenance.reply_value == reply_value;
        }

        [[nodiscard]] constexpr bool ok_branch() const noexcept
        {
            return issue_branch_ok() || reply_branch_ok() || timeout_branch_ok();
        }

        [[nodiscard]] constexpr bool completion_drop() const noexcept
        {
            return kind == TaskMessageSyscallPumpTraceKind::completion_drop;
        }

        [[nodiscard]] constexpr bool ok_value() const noexcept
        {
            return verdict() == semantic::Verdict::standing;
        }

        [[nodiscard]] constexpr bool ok() const noexcept
        {
            return ok_value();
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

            if (completion_drop()) {
                return semantic::Verdict::drifted;
            }

            if (!ok_branch()) {
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

            if (completion_drop()) {
                return semantic::FailureDomain::handoff;
            }

            if (kind == TaskMessageSyscallPumpTraceKind::issue) {
                if (!terminal_ok || owner == TaskId{} || token == 0u ||
                    request_sequence == 0u) {
                    return semantic::FailureDomain::selection;
                }
            } else {
                const bool completion_shape_mismatch =
                    !terminal_ok ||
                    (kind == TaskMessageSyscallPumpTraceKind::reply && timeout) ||
                    (kind == TaskMessageSyscallPumpTraceKind::timeout &&
                     !timeout);
                if (completion_shape_mismatch) {
                    return semantic::FailureDomain::handoff;
                }
            }

            if (!lower_route_consistent()) {
                return semantic::FailureDomain::route;
            }

            return semantic::FailureDomain::none;
        }

        [[nodiscard]] constexpr std::string_view summary_path() const noexcept
        {
            return "task-message-syscall-pump-witness.summary";
        }
    };

    struct TaskMessageSyscallPumpWitnessHandoffTarget {
        const TaskMessageSyscallPumpWitness* witness{nullptr};

        [[nodiscard]] constexpr std::string_view entry_name() const noexcept
        {
            return "task-message-syscall-pump-witness";
        }

        [[nodiscard]] constexpr std::string_view
        selected_summary_path() const noexcept
        {
            return witness != nullptr ? witness->summary_path()
                                      : std::string_view{
                                            "task-message-syscall-pump-witness.summary"};
        }
    };

    static_assert(
        semantic::reflected_member_names_match_when_enabled<TaskMessageSyscallPumpWitness>(
            std::array<std::string_view, 17>{
                "sequence",
                "ready",
                "has_trace",
                "kind",
                "owner",
                "token",
                "request_sequence",
                "wait_due",
                "pending_requests",
                "pending_completions",
                "terminal_ok",
                "timeout",
                "reply_value",
                "disposition",
                "error",
                "has_lower_provenance",
                "lower_provenance",
            }));

    static_assert(semantic::WitnessCarrier<TaskMessageSyscallPumpWitness>);
    static_assert(
        semantic::HandoffTarget<TaskMessageSyscallPumpWitnessHandoffTarget>);

    [[nodiscard]] constexpr TaskMessageSyscallPumpWitness
    task_message_syscall_pump_witness(
        const TaskMessageSyscallPumpTraceEvent& event) noexcept
    {
        return TaskMessageSyscallPumpWitness{
            .sequence = event.sequence,
            .ready = event.sequence != 0u,
            .has_trace = true,
            .kind = event.kind,
            .owner = event.owner,
            .token = event.token,
            .request_sequence = event.request_sequence,
            .wait_due = event.wait_due,
            .pending_requests = event.pending_requests,
            .pending_completions = event.pending_completions,
            .terminal_ok = event.ok,
            .timeout = event.timeout,
            .reply_value = event.reply_value,
            .disposition = event.disposition,
            .error = event.error,
        };
    }

    [[nodiscard]] constexpr TaskMessageSyscallPumpWitness
    task_message_syscall_pump_witness(
        const TaskMessageSyscallPumpTraceEvent& event,
        const TaskMessageSyscallClientWitness& lower) noexcept
    {
        auto witness = task_message_syscall_pump_witness(event);
        witness.has_lower_provenance = true;
        witness.lower_provenance = lower;
        return witness;
    }

    template <std::size_t Capacity>
    [[nodiscard]] constexpr TaskMessageSyscallPumpWitness
    task_message_syscall_pump_witness(
        const TaskMessageSyscallPumpTraceBuffer<Capacity>& trace) noexcept
    {
        const auto* terminal =
            trace.size() == 0u ? nullptr : trace.at(trace.size() - 1u);
        if (terminal == nullptr) {
            return TaskMessageSyscallPumpWitness{};
        }

        return task_message_syscall_pump_witness(*terminal);
    }

    template <std::size_t Capacity>
    [[nodiscard]] constexpr TaskMessageSyscallPumpWitness
    task_message_syscall_pump_witness(
        const TaskMessageSyscallPumpTraceBuffer<Capacity>& trace,
        const TaskMessageSyscallClientWitness& lower) noexcept
    {
        auto witness = task_message_syscall_pump_witness(trace);
        if (!witness.ready) {
            return witness;
        }

        witness.has_lower_provenance = true;
        witness.lower_provenance = lower;
        return witness;
    }

    [[nodiscard]] constexpr bool task_message_syscall_pump_witness_ready(
        const TaskMessageSyscallPumpWitness& witness) noexcept
    {
        return witness.ready;
    }

    [[nodiscard]] constexpr TaskMessageSyscallPumpWitnessHandoffTarget
    task_message_syscall_pump_witness_handoff_target(
        const TaskMessageSyscallPumpWitness& witness) noexcept
    {
        return TaskMessageSyscallPumpWitnessHandoffTarget{
            .witness = &witness,
        };
    }

    template <typename Client>
    [[nodiscard]] constexpr TaskMessageSyscallPumpWitness
    task_message_syscall_pump_witness(
        const TaskMessageSyscallPumpStepResult<Client>& result) noexcept
    {
        auto witness = TaskMessageSyscallPumpWitness{};
        if (result.completion_dropped) {
            witness.ready = true;
            witness.kind = TaskMessageSyscallPumpTraceKind::completion_drop;
            witness.owner = result.completion.owner;
            witness.token = result.completion.token;
            witness.request_sequence = result.completion.request_sequence;
            witness.terminal_ok = false;
            witness.timeout = result.completion.timeout;
            witness.reply_value = result.completion.trap.value;
            witness.disposition = result.completion.trap.disposition;
            witness.error = result.completion.trap.error;
        } else if (result.completion_ready) {
            witness.ready = true;
            witness.kind = result.completion.timeout
                               ? TaskMessageSyscallPumpTraceKind::timeout
                               : TaskMessageSyscallPumpTraceKind::reply;
            witness.owner = result.completion.owner;
            witness.token = result.completion.token;
            witness.request_sequence = result.completion.request_sequence;
            witness.terminal_ok = result.completion_pushed;
            witness.timeout = result.completion.timeout;
            witness.reply_value = result.completion.trap.value;
            witness.disposition = result.completion.trap.disposition;
            witness.error = result.completion.trap.error;
        } else if (result.issued) {
            witness.ready = true;
            witness.kind = TaskMessageSyscallPumpTraceKind::issue;
            witness.owner = result.issued_request.owner;
            witness.token = result.issued_request.token;
            witness.request_sequence = result.issued_request.request_sequence;
            witness.wait_due =
                static_cast<util::u64>(result.issued_request.wait_due);
            witness.terminal_ok = true;
        }

        const auto lower = task_message_syscall_client_witness(result.client);
        if (lower.ready) {
            witness.has_lower_provenance = true;
            witness.lower_provenance = lower;
        }

        return witness;
    }

    template <typename Client>
    [[nodiscard]] constexpr TaskMessageSyscallPumpWitness
    task_message_syscall_pump_witness(
        const TaskMessageSyscallPumpStepResult<Client>& result,
        const TaskMessageSyscallClientWitness& lower) noexcept
    {
        auto witness = task_message_syscall_pump_witness(result);
        witness.has_lower_provenance = true;
        witness.lower_provenance = lower;
        return witness;
    }

    template <typename Client,
              std::size_t RequestCapacity = 4,
              std::size_t CompletionCapacity = 4,
              typename TraceBuffer = TaskMessageSyscallPumpTraceBuffer<1>>
    class TaskMessageSyscallPump {
    public:
        using client_type = Client;
        using trace_type = TraceBuffer;
        using tick_type = typename Client::tick_type;
        using reply_type = typename Client::reply_type;
        using request_type = TaskMessageSyscallPumpRequest<tick_type>;
        using completion_type = TaskMessageSyscallPumpCompletion<reply_type>;
        using result_type = TaskMessageSyscallPumpStepResult<Client>;

        static_assert(RequestCapacity > 0);
        static_assert(CompletionCapacity > 0);

        constexpr TaskMessageSyscallPump() noexcept = default;

        constexpr explicit TaskMessageSyscallPump(
            Client client,
            TraceBuffer* trace = nullptr) noexcept
            : client_(client), trace_(trace)
        {
        }

        [[nodiscard]] bool valid() const noexcept
        {
            return client_.valid();
        }

        [[nodiscard]] bool busy() const noexcept
        {
            return client_.busy();
        }

        [[nodiscard]] Client& client() noexcept
        {
            return client_;
        }

        [[nodiscard]] const Client& client() const noexcept
        {
            return client_;
        }

        [[nodiscard]] std::size_t pending_requests() const noexcept
        {
            return request_count_;
        }

        [[nodiscard]] std::size_t pending_completions() const noexcept
        {
            return completion_count_;
        }

        void bind_client(Client client) noexcept
        {
            client_ = client;
        }

        void bind_trace(TraceBuffer* trace) noexcept
        {
            trace_ = trace;
        }

        void bind_cursors(util::u64 next_token,
                          util::u64 next_sequence) noexcept
        {
            client_.bind_cursors(next_token, next_sequence);
        }

        [[nodiscard]] bool enqueue(request_type request) noexcept
        {
            return push_request(request);
        }

        [[nodiscard]] bool enqueue(TaskSyscallRequest request,
                                   tick_type wait_due) noexcept
        {
            return enqueue(
                make_task_message_syscall_pump_request(request, wait_due));
        }

        [[nodiscard]] bool enqueue(TaskId owner,
                                   TaskSyscallRequest request,
                                   tick_type wait_due) noexcept
        {
            return enqueue(make_task_message_syscall_pump_request(
                owner, request, wait_due));
        }

        [[nodiscard]] bool enqueue(TaskId owner,
                                   util::u64 token,
                                   util::u64 request_sequence,
                                   TaskSyscallRequest request,
                                   tick_type wait_due) noexcept
        {
            return enqueue(make_task_message_syscall_pump_request(
                owner, token, request_sequence, request, wait_due));
        }

        [[nodiscard]] bool enqueue_yield(tick_type wait_due) noexcept
        {
            return enqueue(make_task_syscall_yield_request(), wait_due);
        }

        [[nodiscard]] bool enqueue_yield(TaskId owner,
                                         tick_type wait_due) noexcept
        {
            return enqueue(owner, make_task_syscall_yield_request(), wait_due);
        }

        template <typename Tick>
        [[nodiscard]] bool enqueue_sleep_until(
            TrapSleepUntilView<Tick> sleep,
            tick_type wait_due) noexcept
        {
            return enqueue(make_task_syscall_sleep_until_request(sleep), wait_due);
        }

        template <typename Tick>
        [[nodiscard]] bool enqueue_sleep_until(
            TaskId owner,
            TrapSleepUntilView<Tick> sleep,
            tick_type wait_due) noexcept
        {
            return enqueue(owner,
                           make_task_syscall_sleep_until_request(sleep),
                           wait_due);
        }

        [[nodiscard]] bool enqueue_debug_write(TrapDebugWriteView write,
                                               tick_type wait_due) noexcept
        {
            return enqueue(make_task_syscall_debug_write_request(write), wait_due);
        }

        [[nodiscard]] bool enqueue_debug_write(TaskId owner,
                                               TrapDebugWriteView write,
                                               tick_type wait_due) noexcept
        {
            return enqueue(owner,
                           make_task_syscall_debug_write_request(write),
                           wait_due);
        }

        [[nodiscard]] bool enqueue_capability_call(
            TrapCapabilityCallView capability,
            tick_type wait_due) noexcept
        {
            return enqueue(make_task_syscall_capability_call_request(capability),
                           wait_due);
        }

        [[nodiscard]] bool enqueue_capability_call(
            TaskId owner,
            TrapCapabilityCallView capability,
            tick_type wait_due) noexcept
        {
            return enqueue(owner,
                           make_task_syscall_capability_call_request(capability),
                           wait_due);
        }

        [[nodiscard]] bool kick() noexcept
        {
            request_type issued_request{};
            return issue_next(issued_request);
        }

        [[nodiscard]] result_type step(Event event) noexcept
        {
            auto result = result_type{};
            if (!valid()) {
                return result;
            }

            result.client = client_.step(event);
            if (result.client.reply_consumed || result.client.timeout_consumed) {
                result.completion_ready = true;
                result.completion = completion_from_client(result.client);
                result.completion_pushed = push_completion(result.completion);
                result.completion_dropped = !result.completion_pushed;
                result.progressed = true;
                trace_completion(result);
            }

            if (!busy()) {
                result.issued = issue_next(result.issued_request);
                result.progressed = result.progressed || result.issued;
            }

            return result;
        }

        [[nodiscard]] bool receive_completion(completion_type& out) noexcept
        {
            return pop_completion(out);
        }

    private:
        [[nodiscard]] completion_type completion_from_client(
            const typename Client::result_type& result) const noexcept
        {
            return completion_type{
                .timeout = result.timeout_consumed,
                .owner = result.owner,
                .token = result.token,
                .request_sequence = result.request_sequence,
                .reply = result.reply,
                .trap = result.trap,
            };
        }

        [[nodiscard]] bool issue_next(request_type& issued_request) noexcept
        {
            if (busy() || request_count_ == 0u) {
                return false;
            }

            request_type request{};
            if (!peek_request(request)) {
                return false;
            }

            const auto ok = issue_request(request);
            if (!ok) {
                trace_issue(request, ok);
                return false;
            }

            issued_request = request;
            const auto active = client_.state();
            issued_request.owner = active.owner;
            issued_request.token = active.token;
            issued_request.request_sequence = active.sequence;
            trace_issue(issued_request, true);
            request_type popped_request{};
            (void)pop_request(popped_request);
            return true;
        }

        [[nodiscard]] bool issue_request(const request_type& request) noexcept
        {
            if (!request.explicit_owner) {
                return client_.begin(request.request, request.wait_due);
            }

            if (request.explicit_ids) {
                return client_.begin(request.owner,
                                     request.token,
                                     request.request_sequence,
                                     request.request,
                                     request.wait_due);
            }

            return client_.begin(request.owner, request.request, request.wait_due);
        }

        [[nodiscard]] bool push_request(request_type request) noexcept
        {
            if (request_count_ >= RequestCapacity) {
                return false;
            }

            requests_[request_tail_] = request;
            request_tail_ = (request_tail_ + 1u) % RequestCapacity;
            ++request_count_;
            return true;
        }

        [[nodiscard]] bool peek_request(request_type& out) const noexcept
        {
            if (request_count_ == 0u) {
                return false;
            }

            out = requests_[request_head_];
            return true;
        }

        [[nodiscard]] bool pop_request(request_type& out) noexcept
        {
            if (request_count_ == 0u) {
                return false;
            }

            out = requests_[request_head_];
            request_head_ = (request_head_ + 1u) % RequestCapacity;
            --request_count_;
            return true;
        }

        [[nodiscard]] bool push_completion(completion_type completion) noexcept
        {
            if (completion_count_ >= CompletionCapacity) {
                return false;
            }

            completions_[completion_tail_] = completion;
            completion_tail_ = (completion_tail_ + 1u) % CompletionCapacity;
            ++completion_count_;
            return true;
        }

        [[nodiscard]] bool pop_completion(completion_type& out) noexcept
        {
            if (completion_count_ == 0u) {
                return false;
            }

            out = completions_[completion_head_];
            completion_head_ = (completion_head_ + 1u) % CompletionCapacity;
            --completion_count_;
            return true;
        }

        void trace_issue(const request_type& request, bool ok) noexcept
        {
            trace_push(TaskMessageSyscallPumpTraceEvent{
                .kind = TaskMessageSyscallPumpTraceKind::issue,
                .owner = request.owner,
                .token = request.token,
                .request_sequence = request.request_sequence,
                .wait_due = static_cast<util::u64>(request.wait_due),
                .pending_requests = request_count_,
                .pending_completions = completion_count_,
                .ok = ok,
            });
        }

        void trace_completion(const result_type& result) noexcept
        {
            const auto kind = result.completion.timeout
                                  ? TaskMessageSyscallPumpTraceKind::timeout
                                  : TaskMessageSyscallPumpTraceKind::reply;
            trace_push(TaskMessageSyscallPumpTraceEvent{
                .kind = kind,
                .owner = result.completion.owner,
                .token = result.completion.token,
                .request_sequence = result.completion.request_sequence,
                .pending_requests = request_count_,
                .pending_completions = completion_count_,
                .ok = result.completion_pushed,
                .timeout = result.completion.timeout,
                .reply_value = result.completion.trap.value,
                .disposition = result.completion.trap.disposition,
                .error = result.completion.trap.error,
            });

            if (!result.completion_dropped) {
                return;
            }

            trace_push(TaskMessageSyscallPumpTraceEvent{
                .kind = TaskMessageSyscallPumpTraceKind::completion_drop,
                .owner = result.completion.owner,
                .token = result.completion.token,
                .request_sequence = result.completion.request_sequence,
                .pending_requests = request_count_,
                .pending_completions = completion_count_,
                .ok = false,
                .timeout = result.completion.timeout,
                .reply_value = result.completion.trap.value,
                .disposition = result.completion.trap.disposition,
                .error = result.completion.trap.error,
            });
        }

        void trace_push(const TaskMessageSyscallPumpTraceEvent& event) noexcept
        {
            if (trace_ == nullptr) {
                return;
            }

            auto traced = event;
            traced.sequence = ++sequence_;
            (void)trace_->push(traced);
        }

        Client client_{};
        TraceBuffer* trace_{nullptr};
        std::array<request_type, RequestCapacity> requests_{};
        std::size_t request_head_{0};
        std::size_t request_tail_{0};
        std::size_t request_count_{0};
        std::array<completion_type, CompletionCapacity> completions_{};
        std::size_t completion_head_{0};
        std::size_t completion_tail_{0};
        std::size_t completion_count_{0};
        util::u64 sequence_{0};
    };

    template <typename Client,
              std::size_t RequestCapacity = 4,
              std::size_t CompletionCapacity = 4>
    [[nodiscard]] auto make_task_message_syscall_pump(
        Client client) noexcept
        -> TaskMessageSyscallPump<Client, RequestCapacity, CompletionCapacity>
    {
        return TaskMessageSyscallPump<Client, RequestCapacity, CompletionCapacity>{
            client,
        };
    }

    template <typename Client,
              std::size_t RequestCapacity = 4,
              std::size_t CompletionCapacity = 4,
              typename TraceBuffer>
    [[nodiscard]] auto make_task_message_syscall_pump(
        Client client,
        TraceBuffer* trace) noexcept
        -> TaskMessageSyscallPump<Client,
                                  RequestCapacity,
                                  CompletionCapacity,
                                  TraceBuffer>
    {
        return TaskMessageSyscallPump<Client,
                                      RequestCapacity,
                                      CompletionCapacity,
                                      TraceBuffer>{client, trace};
    }
}
