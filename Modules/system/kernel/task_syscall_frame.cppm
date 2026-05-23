module;

#include <array>
#include <cstddef>
#include <string_view>

export module kernel.task_syscall_frame;

export import kernel.task_syscall_table;
export import kernel.runtime_trap_ingress;
import semantic.core;
import util.core;

export namespace kernel {
    enum class TaskSyscallFrameStage : util::u8 {
        decode = 0,
        dispatch,
        writeback,
    };

    [[nodiscard]] constexpr const char* task_syscall_frame_stage_name(
        TaskSyscallFrameStage stage) noexcept
    {
        switch (stage) {
        case TaskSyscallFrameStage::decode:
            return "decode";
        case TaskSyscallFrameStage::dispatch:
            return "dispatch";
        case TaskSyscallFrameStage::writeback:
            return "writeback";
        }
        return "unknown";
    }

    struct TaskSyscallFrameView {
        TaskSyscallId syscall{TaskSyscallId::invalid};
        util::u64 arg0{0};
        util::u64 arg1{0};
        util::u64 arg2{0};
        util::u64 arg3{0};
    };

    static_assert(
        semantic::reflected_member_names_match_when_enabled<TaskSyscallFrameView>(
        std::array<std::string_view, 5>{
            "syscall",
            "arg0",
            "arg1",
            "arg2",
            "arg3"}));

    [[nodiscard]] constexpr bool task_syscall_frame_view_ready(
        const TaskSyscallFrameView& view) noexcept
    {
        return view.syscall != TaskSyscallId::invalid;
    }

    [[nodiscard]] constexpr TaskSyscallFrameView
    task_syscall_frame_view_from_request(TaskSyscallRequest request) noexcept
    {
        return TaskSyscallFrameView{
            .syscall = request.syscall,
            .arg0 = request.arg0,
            .arg1 = request.arg1,
            .arg2 = request.arg2,
            .arg3 = request.arg3,
        };
    }

    [[nodiscard]] constexpr TaskSyscallRequest task_syscall_request_from_frame_view(
        const TaskSyscallFrameView& view) noexcept
    {
        return TaskSyscallRequest{
            .syscall = view.syscall,
            .arg0 = view.arg0,
            .arg1 = view.arg1,
            .arg2 = view.arg2,
            .arg3 = view.arg3,
        };
    }

    [[nodiscard]] constexpr bool task_syscall_frame_view_decode(
        const TrapRequest& request,
        TaskSyscallFrameView& out) noexcept
    {
        const auto syscall = task_syscall_from_trap_service(request.service);
        if (syscall == TaskSyscallId::invalid) {
            return false;
        }

        out = TaskSyscallFrameView{
            .syscall = syscall,
            .arg0 = request.arg0,
            .arg1 = request.arg1,
            .arg2 = request.arg2,
            .arg3 = request.arg3,
        };
        return true;
    }

    [[nodiscard]] constexpr bool task_syscall_frame_view_decode(
        const TrapFrameView& frame,
        TaskSyscallFrameView& out) noexcept
    {
        return task_syscall_frame_view_decode(
            trap_request_from_frame(frame), out);
    }

    [[nodiscard]] constexpr TaskSyscallSemanticProjection
    task_syscall_semantic_projection(const TaskSyscallFrameView& view) noexcept
    {
        return task_syscall_semantic_projection(
            task_syscall_request_from_frame_view(view));
    }

    template <typename Frame>
    struct TaskSyscallFrameAdapter {
        void* ctx{nullptr};
        bool (*capture)(void* ctx,
                        const Frame& frame,
                        TaskSyscallFrameView& out) noexcept {nullptr};
        bool (*apply_result)(void* ctx,
                             Frame& frame,
                             const TrapResult& result) noexcept {nullptr};
    };

    template <typename Frame>
    [[nodiscard]] constexpr bool task_syscall_frame_adapter_ready(
        const TaskSyscallFrameAdapter<Frame>& adapter) noexcept
    {
        return adapter.capture != nullptr && adapter.apply_result != nullptr;
    }

    template <typename Frame>
    class TaskSyscallFrameIngressAdapter {
    public:
        using frame_type = Frame;
        using trap_adapter_type = RuntimeTrapFrameAdapter<Frame>;

        constexpr TaskSyscallFrameIngressAdapter() noexcept = default;

        constexpr explicit TaskSyscallFrameIngressAdapter(
            trap_adapter_type trap_adapter) noexcept
            : trap_adapter_(trap_adapter)
        {
        }

        [[nodiscard]] bool valid() const noexcept
        {
            return runtime_trap_frame_adapter_ready(trap_adapter_);
        }

        [[nodiscard]] const trap_adapter_type& trap_adapter() const noexcept
        {
            return trap_adapter_;
        }

        void bind_trap_adapter(trap_adapter_type trap_adapter) noexcept
        {
            trap_adapter_ = trap_adapter;
        }

        [[nodiscard]] bool capture(const Frame& frame,
                                   TaskSyscallFrameView& out) const noexcept
        {
            if (!valid()) {
                return false;
            }

            TrapFrameView trap_frame{};
            if (!trap_adapter_.capture(trap_adapter_.ctx, frame, trap_frame)) {
                return false;
            }

            return task_syscall_frame_view_decode(trap_frame, out);
        }

        [[nodiscard]] bool apply_result(
            Frame& frame,
            const TrapResult& result) const noexcept
        {
            if (!valid()) {
                return false;
            }

            return trap_adapter_.apply_result(
                trap_adapter_.ctx, frame, result);
        }

    private:
        trap_adapter_type trap_adapter_{};
    };

    template <typename Frame>
    [[nodiscard]] constexpr bool task_syscall_frame_ingress_adapter_ready(
        const TaskSyscallFrameIngressAdapter<Frame>& adapter) noexcept
    {
        return adapter.valid();
    }

    struct TaskSyscallFrameTraceEvent {
        util::u64 sequence{0};
        TaskSyscallFrameStage stage{TaskSyscallFrameStage::decode};
        TaskSyscallId syscall{TaskSyscallId::invalid};
        TrapService trap_service{TrapService::invalid};
        TrapDisposition disposition{TrapDisposition::rejected};
        TrapError error{TrapError::none};
        util::u64 arg0{0};
        util::u64 arg1{0};
        util::u64 arg2{0};
        util::u64 arg3{0};
        util::u64 value{0};
        bool ok{false};
    };

    static_assert(
        semantic::reflected_member_names_match_when_enabled<TaskSyscallFrameTraceEvent>(
            std::array<std::string_view, 12>{
                "sequence",
                "stage",
                "syscall",
                "trap_service",
                "disposition",
                "error",
                "arg0",
                "arg1",
                "arg2",
                "arg3",
                "value",
                "ok"}));

    struct TaskSyscallFrameForensicSnapshot {
        using value_type = TaskSyscallFrameTraceEvent;

        bool has_terminal{false};
        value_type terminal{};
        bool has_decode{false};
        value_type decode{};
        bool has_dispatch{false};
        value_type dispatch{};
        bool has_writeback{false};
        value_type writeback{};
        bool has_last_failure{false};
        value_type last_failure{};

        [[nodiscard]] constexpr util::u64 sequence() const noexcept
        {
            return has_terminal ? terminal.sequence : 0u;
        }
    };

    struct TaskSyscallFrameWitness {
        util::u64 sequence{0};
        bool ready{false};
        bool terminal_ok{false};
        bool has_trace{false};
        bool has_terminal{false};
        bool has_decode{false};
        bool has_dispatch{false};
        bool has_writeback{false};
        TaskSyscallFrameStage terminal_stage{TaskSyscallFrameStage::decode};
        TaskSyscallId terminal_syscall{TaskSyscallId::invalid};
        TrapService terminal_trap_service{TrapService::invalid};
        TrapDisposition terminal_disposition{TrapDisposition::rejected};
        TrapError terminal_error{TrapError::none};
        util::u64 terminal_value{0};
        bool has_last_failure{false};
        util::u64 last_failure_sequence{0};
        TaskSyscallFrameStage last_failure_stage{
            TaskSyscallFrameStage::decode};
        TaskSyscallId last_failure_syscall{TaskSyscallId::invalid};
        TrapService last_failure_trap_service{TrapService::invalid};
        TrapDisposition last_failure_disposition{TrapDisposition::rejected};
        TrapError last_failure_error{TrapError::none};
        util::u64 last_failure_value{0};

        [[nodiscard]] constexpr bool ok() const noexcept
        {
            return ready && terminal_ok;
        }

        [[nodiscard]] constexpr bool last_failure_is_prior_attempt()
            const noexcept
        {
            return has_last_failure && last_failure_sequence != 0u &&
                   last_failure_sequence != sequence;
        }

        [[nodiscard]] constexpr semantic::Result result() const noexcept
        {
            return verdict() == semantic::Verdict::standing
                       ? semantic::Result::ok
                       : semantic::Result::failed;
        }

        [[nodiscard]] constexpr semantic::Verdict verdict() const noexcept
        {
            if (!has_terminal || !has_decode || !has_dispatch ||
                !has_writeback) {
                return semantic::Verdict::collapsed;
            }

            if (!terminal_ok &&
                (terminal_stage == TaskSyscallFrameStage::decode ||
                 terminal_stage == TaskSyscallFrameStage::writeback)) {
                return semantic::Verdict::collapsed;
            }

            if (terminal_ok && last_failure_is_prior_attempt()) {
                return semantic::Verdict::drifted;
            }

            if (terminal_ok) {
                return semantic::Verdict::standing;
            }

            return semantic::Verdict::drifted;
        }

        [[nodiscard]] constexpr semantic::FailureDomain
        failure_domain() const noexcept
        {
            const bool terminal_failed = has_terminal && !terminal_ok;
            const bool prior_failure = last_failure_is_prior_attempt();
            if (terminal_failed || prior_failure) {
                const auto stage = terminal_failed ? terminal_stage
                                                   : last_failure_stage;
                const auto error = terminal_failed ? terminal_error
                                                   : last_failure_error;

                if (stage == TaskSyscallFrameStage::decode &&
                    error != TrapError::none) {
                    return semantic::FailureDomain::selection;
                }

                if (stage == TaskSyscallFrameStage::dispatch &&
                    error != TrapError::none) {
                    return semantic::FailureDomain::route;
                }

                if (stage == TaskSyscallFrameStage::writeback &&
                    error != TrapError::none) {
                    return semantic::FailureDomain::handoff;
                }
            }

            if (!ready && has_trace) {
                return semantic::FailureDomain::input;
            }

            return semantic::FailureDomain::none;
        }

        [[nodiscard]] constexpr std::string_view summary_path() const noexcept
        {
            return "task-syscall-frame-witness.summary";
        }
    };

    struct TaskSyscallFrameWitnessHandoffTarget {
        const TaskSyscallFrameWitness* witness{nullptr};

        [[nodiscard]] constexpr std::string_view entry_name() const noexcept
        {
            return "task-syscall-frame-witness";
        }

        [[nodiscard]] constexpr std::string_view
        selected_summary_path() const noexcept
        {
            return witness != nullptr ? witness->summary_path()
                                      : std::string_view{
                                            "task-syscall-frame-witness.summary"};
        }
    };

    static_assert(semantic::WitnessCarrier<TaskSyscallFrameWitness>);
    static_assert(semantic::HandoffTarget<TaskSyscallFrameWitnessHandoffTarget>);

    [[nodiscard]] constexpr TaskSyscallFrameView task_syscall_frame_view_from_trace_event(
        const TaskSyscallFrameTraceEvent& event) noexcept
    {
        return TaskSyscallFrameView{
            .syscall = event.syscall,
            .arg0 = event.arg0,
            .arg1 = event.arg1,
            .arg2 = event.arg2,
            .arg3 = event.arg3,
        };
    }

    [[nodiscard]] constexpr TaskSyscallRequest
    task_syscall_request_from_trace_event(
        const TaskSyscallFrameTraceEvent& event) noexcept
    {
        return task_syscall_request_from_frame_view(
            task_syscall_frame_view_from_trace_event(event));
    }

    [[nodiscard]] constexpr TaskSyscallSemanticProjection
    task_syscall_semantic_projection(
        const TaskSyscallFrameTraceEvent& event) noexcept
    {
        return task_syscall_semantic_projection(
            task_syscall_request_from_trace_event(event));
    }

    [[nodiscard]] constexpr TaskSyscallFrameWitness
    task_syscall_frame_witness(
        const TaskSyscallFrameForensicSnapshot& snapshot) noexcept
    {
        TaskSyscallFrameWitness witness{};
        witness.has_trace = snapshot.has_terminal || snapshot.has_decode ||
                            snapshot.has_dispatch || snapshot.has_writeback ||
                            snapshot.has_last_failure;
        witness.has_terminal = snapshot.has_terminal;
        if (!snapshot.has_terminal) {
            return witness;
        }

        witness.sequence = snapshot.terminal.sequence;
        witness.ready = snapshot.has_decode && snapshot.has_dispatch &&
                        snapshot.has_writeback;
        witness.terminal_ok = snapshot.terminal.ok;
        witness.has_decode = snapshot.has_decode;
        witness.has_dispatch = snapshot.has_dispatch;
        witness.has_writeback = snapshot.has_writeback;
        witness.terminal_stage = snapshot.terminal.stage;
        witness.terminal_syscall = snapshot.terminal.syscall;
        witness.terminal_trap_service = snapshot.terminal.trap_service;
        witness.terminal_disposition = snapshot.terminal.disposition;
        witness.terminal_error = snapshot.terminal.error;
        witness.terminal_value = snapshot.terminal.value;
        witness.has_last_failure = snapshot.has_last_failure;

        if (snapshot.has_last_failure) {
            witness.last_failure_sequence = snapshot.last_failure.sequence;
            witness.last_failure_stage = snapshot.last_failure.stage;
            witness.last_failure_syscall = snapshot.last_failure.syscall;
            witness.last_failure_trap_service =
                snapshot.last_failure.trap_service;
            witness.last_failure_disposition =
                snapshot.last_failure.disposition;
            witness.last_failure_error = snapshot.last_failure.error;
            witness.last_failure_value = snapshot.last_failure.value;
        }

        return witness;
    }

    [[nodiscard]] constexpr bool task_syscall_frame_witness_ready(
        const TaskSyscallFrameWitness& witness) noexcept
    {
        return witness.ready;
    }

    [[nodiscard]] constexpr TaskSyscallFrameWitnessHandoffTarget
    task_syscall_frame_witness_handoff_target(
        const TaskSyscallFrameWitness& witness) noexcept
    {
        return TaskSyscallFrameWitnessHandoffTarget{
            .witness = &witness,
        };
    }

    template <std::size_t Capacity>
    class TaskSyscallFrameTraceBuffer {
    public:
        using value_type = TaskSyscallFrameTraceEvent;

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
    [[nodiscard]] TaskSyscallFrameForensicSnapshot
    task_syscall_frame_forensic_snapshot(
        const TaskSyscallFrameTraceBuffer<Capacity>& trace) noexcept
    {
        TaskSyscallFrameForensicSnapshot snapshot{};
        if (trace.size() == 0u) {
            return snapshot;
        }

        const auto* terminal = trace.at(trace.size() - 1u);
        if (terminal == nullptr) {
            return snapshot;
        }

        snapshot.has_terminal = true;
        snapshot.terminal = *terminal;

        for (std::size_t index = trace.size(); index > 0u; --index) {
            const auto* event = trace.at(index - 1u);
            if (event == nullptr) {
                continue;
            }

            if (!snapshot.has_last_failure && !event->ok) {
                snapshot.has_last_failure = true;
                snapshot.last_failure = *event;
            }

            switch (event->stage) {
            case TaskSyscallFrameStage::decode:
                if (!snapshot.has_decode) {
                    snapshot.has_decode = true;
                    snapshot.decode = *event;
                }
                return snapshot;
            case TaskSyscallFrameStage::dispatch:
                if (!snapshot.has_dispatch) {
                    snapshot.has_dispatch = true;
                    snapshot.dispatch = *event;
                }
                break;
            case TaskSyscallFrameStage::writeback:
                if (!snapshot.has_writeback) {
                    snapshot.has_writeback = true;
                    snapshot.writeback = *event;
                }
                break;
            }
        }

        return snapshot;
    }

    template <std::size_t Capacity>
    [[nodiscard]] TaskSyscallFrameWitness task_syscall_frame_witness(
        const TaskSyscallFrameTraceBuffer<Capacity>& trace) noexcept
    {
        return task_syscall_frame_witness(
            task_syscall_frame_forensic_snapshot(trace));
    }

    template <typename Table,
              typename Frame,
              typename TraceBuffer = TaskSyscallFrameTraceBuffer<1>>
    class TaskSyscallFrameBridge {
    public:
        using table_type = Table;
        using frame_type = Frame;
        using trace_type = TraceBuffer;
        using adapter_type = TaskSyscallFrameAdapter<Frame>;

        constexpr TaskSyscallFrameBridge() noexcept = default;

        TaskSyscallFrameBridge(Table& table,
                               adapter_type adapter,
                               TraceBuffer* trace = nullptr) noexcept
            : table_(&table), adapter_(adapter), trace_(trace)
        {
        }

        [[nodiscard]] bool valid() const noexcept
        {
            return table_ != nullptr &&
                   task_syscall_frame_adapter_ready(adapter_);
        }

        [[nodiscard]] Table& table() noexcept
        {
            return *table_;
        }

        [[nodiscard]] const Table& table() const noexcept
        {
            return *table_;
        }

        [[nodiscard]] const adapter_type& adapter() const noexcept
        {
            return adapter_;
        }

        void bind_table(Table& table) noexcept
        {
            table_ = &table;
        }

        void bind_adapter(adapter_type adapter) noexcept
        {
            adapter_ = adapter;
        }

        void bind_trace(TraceBuffer* trace) noexcept
        {
            trace_ = trace;
        }

        [[nodiscard]] TrapResult dispatch(Frame& frame) noexcept
        {
            TaskSyscallFrameView view{};
            if (!task_syscall_frame_adapter_ready(adapter_)) {
                const auto result = TrapResult{
                    .disposition = TrapDisposition::rejected,
                    .error = TrapError::unbound_adapter,
                    .value = 0,
                };
                trace_push(
                    TaskSyscallFrameStage::decode, view, result, false);
                return result;
            }

            if (!adapter_.capture(adapter_.ctx, frame, view)) {
                const auto result = TrapResult{
                    .disposition = TrapDisposition::rejected,
                    .error = TrapError::decode_failed,
                    .value = 0,
                };
                trace_push(
                    TaskSyscallFrameStage::decode, view, result, false);
                return result;
            }

            const auto decode_ok = TrapResult{
                .disposition = TrapDisposition::handled,
                .error = TrapError::none,
                .value = 0,
            };
            trace_push(TaskSyscallFrameStage::decode, view, decode_ok, true);

            if (table_ == nullptr) {
                const auto result = TrapResult{
                    .disposition = TrapDisposition::rejected,
                    .error = TrapError::unbound_bridge,
                    .value = 0,
                };
                trace_push(
                    TaskSyscallFrameStage::dispatch, view, result, false);
                return result;
            }

            const auto result =
                table_->dispatch(task_syscall_request_from_frame_view(view));
            trace_push(
                TaskSyscallFrameStage::dispatch, view, result, result.ok());

            if (!adapter_.apply_result(adapter_.ctx, frame, result)) {
                const auto writeback_failed = TrapResult{
                    .disposition = TrapDisposition::rejected,
                    .error = TrapError::writeback_failed,
                    .value = result.value,
                };
                trace_push(TaskSyscallFrameStage::writeback,
                           view,
                           writeback_failed,
                           false);
                return writeback_failed;
            }

            trace_push(
                TaskSyscallFrameStage::writeback, view, result, true);
            return result;
        }

    private:
        void trace_push(TaskSyscallFrameStage stage,
                        const TaskSyscallFrameView& view,
                        const TrapResult& result,
                        bool ok) noexcept
        {
            if (trace_ == nullptr) {
                return;
            }

            ++sequence_;
            const auto descriptor = task_syscall_catalog_entry(view.syscall);
            (void)trace_->push(typename TraceBuffer::value_type{
                .sequence = sequence_,
                .stage = stage,
                .syscall = view.syscall,
                .trap_service = descriptor.trap_service,
                .disposition = result.disposition,
                .error = result.error,
                .arg0 = view.arg0,
                .arg1 = view.arg1,
                .arg2 = view.arg2,
                .arg3 = view.arg3,
                .value = result.value,
                .ok = ok,
            });
        }

        Table* table_{nullptr};
        adapter_type adapter_{};
        TraceBuffer* trace_{nullptr};
        util::u64 sequence_{0};
    };

    template <typename Table, typename Frame>
    [[nodiscard]] auto make_task_syscall_frame_bridge(
        Table& table,
        TaskSyscallFrameAdapter<Frame> adapter) noexcept
        -> TaskSyscallFrameBridge<Table, Frame>
    {
        return TaskSyscallFrameBridge<Table, Frame>{table, adapter};
    }

    template <typename Table, typename Frame, typename TraceBuffer>
    [[nodiscard]] auto make_task_syscall_frame_bridge(
        Table& table,
        TaskSyscallFrameAdapter<Frame> adapter,
        TraceBuffer* trace) noexcept
        -> TaskSyscallFrameBridge<Table, Frame, TraceBuffer>
    {
        return TaskSyscallFrameBridge<Table, Frame, TraceBuffer>{
            table, adapter, trace};
    }

    template <typename Frame>
    struct TaskSyscallFramePort {
        void* self{nullptr};
        TrapResult (*dispatch_frame_fn)(void* self, Frame* frame) noexcept {
            nullptr
        };

        [[nodiscard]] bool valid() const noexcept
        {
            return self != nullptr && dispatch_frame_fn != nullptr;
        }

        [[nodiscard]] TrapResult dispatch_frame(Frame& frame) const noexcept
        {
            if (!valid()) {
                return TrapResult{
                    .disposition = TrapDisposition::rejected,
                    .error = TrapError::unbound_adapter,
                    .value = 0,
                };
            }

            return dispatch_frame_fn(self, &frame);
        }
    };

    template <typename Frame>
    struct TaskSyscallCallFrameAdapter {
        void* ctx{nullptr};
        bool (*make_frame)(void* ctx,
                           TaskSyscallRequest request,
                           Frame& out) noexcept {nullptr};
        bool (*result_ready)(void* ctx,
                             const Frame& frame,
                             const TrapResult& result) noexcept {nullptr};
    };

    template <typename Frame>
    [[nodiscard]] constexpr bool task_syscall_call_frame_adapter_ready(
        const TaskSyscallCallFrameAdapter<Frame>& adapter) noexcept
    {
        return adapter.make_frame != nullptr;
    }

    template <typename Frame>
    struct TaskSyscallTrapCallFrameAdapter {
        void* ctx{nullptr};
        TrapOrigin origin{TrapOrigin::kernel_thread};
        bool (*make_frame)(void* ctx,
                           TrapRequest request,
                           Frame& out) noexcept {nullptr};
        bool (*result_ready)(void* ctx,
                             const Frame& frame,
                             const TrapResult& result) noexcept {nullptr};
    };

    template <typename Frame>
    [[nodiscard]] constexpr bool task_syscall_trap_call_frame_adapter_ready(
        const TaskSyscallTrapCallFrameAdapter<Frame>& adapter) noexcept
    {
        return adapter.make_frame != nullptr;
    }

    template <typename Frame, typename Tick>
    class TaskSyscallFrameCaller {
    public:
        using frame_type = Frame;
        using tick_type = Tick;
        using port_type = TaskSyscallFramePort<Frame>;
        using adapter_type = TaskSyscallCallFrameAdapter<Frame>;

        constexpr TaskSyscallFrameCaller(port_type port = {},
                                         adapter_type adapter = {}) noexcept
            : port_(port), adapter_(adapter)
        {
        }

        [[nodiscard]] bool valid() const noexcept
        {
            return port_.valid() &&
                   task_syscall_call_frame_adapter_ready(adapter_);
        }

        [[nodiscard]] const port_type& port() const noexcept
        {
            return port_;
        }

        [[nodiscard]] const adapter_type& adapter() const noexcept
        {
            return adapter_;
        }

        void bind_port(port_type port) noexcept
        {
            port_ = port;
        }

        void bind_adapter(adapter_type adapter) noexcept
        {
            adapter_ = adapter;
        }

        [[nodiscard]] TrapResult dispatch(
            TaskSyscallRequest request) const noexcept
        {
            return dispatch_with(
                [this, request](Frame& frame) noexcept {
                    return adapter_.make_frame(adapter_.ctx, request, frame);
                });
        }

        [[nodiscard]] TrapResult dispatch(
            const TrapRequest& request) const noexcept
        {
            return dispatch(task_syscall_request_from_trap_request(request));
        }

        [[nodiscard]] TrapResult yield() const noexcept
        {
            return yield(TrapYieldCurrentView{});
        }

        [[nodiscard]] TrapResult yield(
            TrapYieldCurrentView yield_view) const noexcept
        {
            return dispatch(make_task_syscall_yield_request(yield_view));
        }

        template <typename DueTick>
        [[nodiscard]] TrapResult sleep_until(DueTick due) const noexcept
        {
            return sleep_until(TrapSleepUntilView<DueTick>{
                .due = due,
            });
        }

        template <typename DueTick>
        [[nodiscard]] TrapResult sleep_until(
            TrapSleepUntilView<DueTick> sleep) const noexcept
        {
            return dispatch(make_task_syscall_sleep_until_request(
                sleep));
        }

        [[nodiscard]] TrapResult debug_write(util::u64 value) const noexcept
        {
            return debug_write(TrapDebugWriteView{
                .value = value,
            });
        }

        [[nodiscard]] TrapResult debug_write(
            TrapDebugWriteView write) const noexcept
        {
            return dispatch(make_task_syscall_debug_write_request(
                write));
        }

        [[nodiscard]] TrapResult capability_call(util::u64 capability_id,
                                                 util::u64 operation,
                                                 util::u64 payload = 0) const
            noexcept
        {
            return capability_call(TrapCapabilityCallView{
                .capability_id = capability_id,
                .operation = operation,
                .payload = payload,
            });
        }

        [[nodiscard]] TrapResult capability_call(
            TrapCapabilityCallView capability) const noexcept
        {
            return dispatch(make_task_syscall_capability_call_request(
                capability));
        }

    private:
        template <typename Builder>
        [[nodiscard]] TrapResult dispatch_with(Builder&& builder) const noexcept
        {
            if (!valid()) {
                return TrapResult{
                    .disposition = TrapDisposition::rejected,
                    .error = TrapError::unbound_adapter,
                    .value = 0,
                };
            }

            Frame frame{};
            if (!builder(frame)) {
                return TrapResult{
                    .disposition = TrapDisposition::rejected,
                    .error = TrapError::decode_failed,
                    .value = 0,
                };
            }

            const auto result = port_.dispatch_frame(frame);
            if (adapter_.result_ready != nullptr &&
                result.error == TrapError::none &&
                !adapter_.result_ready(adapter_.ctx, frame, result)) {
                return TrapResult{
                    .disposition = TrapDisposition::rejected,
                    .error = TrapError::writeback_failed,
                    .value = result.value,
                };
            }

            return result;
        }

        port_type port_{};
        adapter_type adapter_{};
    };

    namespace detail {
        template <typename Bridge>
        [[nodiscard]] TrapResult task_syscall_frame_dispatch_adapter(
            void* self,
            typename Bridge::frame_type* frame) noexcept
        {
            return static_cast<Bridge*>(self)->dispatch(*frame);
        }

        template <typename Adapter>
        [[nodiscard]] bool task_syscall_frame_ingress_capture_adapter(
            void* self,
            const typename Adapter::frame_type& frame,
            TaskSyscallFrameView& out) noexcept
        {
            return static_cast<const Adapter*>(self)->capture(frame, out);
        }

        template <typename Adapter>
        [[nodiscard]] bool task_syscall_frame_ingress_apply_result_adapter(
            void* self,
            typename Adapter::frame_type& frame,
            const TrapResult& result) noexcept
        {
            return static_cast<const Adapter*>(self)->apply_result(frame,
                                                                   result);
        }

        template <typename Frame>
        [[nodiscard]] bool task_syscall_frame_trap_capture_adapter(
            void* self,
            const Frame& frame,
            TaskSyscallFrameView& out) noexcept
        {
            auto* trap_adapter =
                static_cast<RuntimeTrapFrameAdapter<Frame>*>(self);
            if (trap_adapter == nullptr ||
                !runtime_trap_frame_adapter_ready(*trap_adapter)) {
                return false;
            }

            TrapFrameView trap_frame{};
            if (!trap_adapter->capture(trap_adapter->ctx, frame, trap_frame)) {
                return false;
            }

            return task_syscall_frame_view_decode(trap_frame, out);
        }

        template <typename Frame>
        [[nodiscard]] bool task_syscall_frame_trap_apply_result_adapter(
            void* self,
            Frame& frame,
            const TrapResult& result) noexcept
        {
            auto* trap_adapter =
                static_cast<RuntimeTrapFrameAdapter<Frame>*>(self);
            if (trap_adapter == nullptr ||
                !runtime_trap_frame_adapter_ready(*trap_adapter)) {
                return false;
            }

            return trap_adapter->apply_result(trap_adapter->ctx, frame, result);
        }

        template <typename Frame>
        [[nodiscard]] bool task_syscall_trap_call_frame_make_adapter(
            void* self,
            TaskSyscallRequest request,
            Frame& out) noexcept
        {
            auto* adapter =
                static_cast<TaskSyscallTrapCallFrameAdapter<Frame>*>(self);
            if (adapter == nullptr ||
                !task_syscall_trap_call_frame_adapter_ready(*adapter)) {
                return false;
            }

            return adapter->make_frame(
                adapter->ctx,
                trap_request_from_task_syscall_request(
                    request, adapter->origin),
                out);
        }

        template <typename Frame>
        [[nodiscard]] bool task_syscall_trap_call_frame_result_ready_adapter(
            void* self,
            const Frame& frame,
            const TrapResult& result) noexcept
        {
            auto* adapter =
                static_cast<TaskSyscallTrapCallFrameAdapter<Frame>*>(self);
            if (adapter == nullptr || adapter->result_ready == nullptr) {
                return false;
            }

            return adapter->result_ready(adapter->ctx, frame, result);
        }
    }

    template <typename Bridge>
    [[nodiscard]] auto make_task_syscall_frame_port(Bridge& bridge) noexcept
        -> TaskSyscallFramePort<typename Bridge::frame_type>
    {
        return TaskSyscallFramePort<typename Bridge::frame_type>{
            .self = &bridge,
            .dispatch_frame_fn =
                &detail::task_syscall_frame_dispatch_adapter<Bridge>,
        };
    }

    template <typename Frame>
    [[nodiscard]] auto make_task_syscall_frame_ingress_adapter(
        RuntimeTrapFrameAdapter<Frame> trap_adapter) noexcept
        -> TaskSyscallFrameIngressAdapter<Frame>
    {
        return TaskSyscallFrameIngressAdapter<Frame>{trap_adapter};
    }

    template <typename Frame>
    [[nodiscard]] auto make_task_syscall_frame_adapter(
        RuntimeTrapFrameAdapter<Frame>& trap_adapter) noexcept
        -> TaskSyscallFrameAdapter<Frame>
    {
        return TaskSyscallFrameAdapter<Frame>{
            .ctx = &trap_adapter,
            .capture = &detail::task_syscall_frame_trap_capture_adapter<Frame>,
            .apply_result =
                &detail::task_syscall_frame_trap_apply_result_adapter<Frame>,
        };
    }

    template <typename Frame>
    [[nodiscard]] auto make_task_syscall_frame_adapter(
        TaskSyscallFrameIngressAdapter<Frame>& adapter) noexcept
        -> TaskSyscallFrameAdapter<Frame>
    {
        return TaskSyscallFrameAdapter<Frame>{
            .ctx = &adapter,
            .capture =
                &detail::task_syscall_frame_ingress_capture_adapter<
                    TaskSyscallFrameIngressAdapter<Frame>>,
            .apply_result =
                &detail::task_syscall_frame_ingress_apply_result_adapter<
                    TaskSyscallFrameIngressAdapter<Frame>>,
        };
    }

    template <typename Table, typename Frame>
    [[nodiscard]] auto make_task_syscall_frame_bridge(
        Table& table,
        RuntimeTrapFrameAdapter<Frame>& trap_adapter) noexcept
        -> TaskSyscallFrameBridge<Table, Frame>
    {
        return TaskSyscallFrameBridge<Table, Frame>{
            table, make_task_syscall_frame_adapter(trap_adapter)};
    }

    template <typename Table, typename Frame, typename TraceBuffer>
    [[nodiscard]] auto make_task_syscall_frame_bridge(
        Table& table,
        RuntimeTrapFrameAdapter<Frame>& trap_adapter,
        TraceBuffer* trace) noexcept
        -> TaskSyscallFrameBridge<Table, Frame, TraceBuffer>
    {
        return TaskSyscallFrameBridge<Table, Frame, TraceBuffer>{
            table, make_task_syscall_frame_adapter(trap_adapter), trace};
    }

    template <typename Table, typename Frame>
    [[nodiscard]] auto make_task_syscall_frame_bridge(
        Table& table,
        TaskSyscallFrameIngressAdapter<Frame>& adapter) noexcept
        -> TaskSyscallFrameBridge<Table, Frame>
    {
        return TaskSyscallFrameBridge<Table, Frame>{
            table, make_task_syscall_frame_adapter(adapter)};
    }

    template <typename Table, typename Frame, typename TraceBuffer>
    [[nodiscard]] auto make_task_syscall_frame_bridge(
        Table& table,
        TaskSyscallFrameIngressAdapter<Frame>& adapter,
        TraceBuffer* trace) noexcept
        -> TaskSyscallFrameBridge<Table, Frame, TraceBuffer>
    {
        return TaskSyscallFrameBridge<Table, Frame, TraceBuffer>{
            table, make_task_syscall_frame_adapter(adapter), trace};
    }

    template <typename Frame>
    [[nodiscard]] auto make_task_syscall_call_frame_adapter(
        TaskSyscallTrapCallFrameAdapter<Frame>& adapter) noexcept
        -> TaskSyscallCallFrameAdapter<Frame>
    {
        return TaskSyscallCallFrameAdapter<Frame>{
            .ctx = &adapter,
            .make_frame =
                &detail::task_syscall_trap_call_frame_make_adapter<Frame>,
            .result_ready = adapter.result_ready == nullptr
                ? nullptr
                : &detail::task_syscall_trap_call_frame_result_ready_adapter<
                      Frame>,
        };
    }

    template <typename Frame, typename Tick>
    [[nodiscard]] auto make_task_syscall_frame_caller(
        TaskSyscallFramePort<Frame> port,
        TaskSyscallCallFrameAdapter<Frame> adapter) noexcept
        -> TaskSyscallFrameCaller<Frame, Tick>
    {
        return TaskSyscallFrameCaller<Frame, Tick>{port, adapter};
    }

    template <typename Frame, typename Tick>
    [[nodiscard]] auto make_task_syscall_frame_caller(
        TaskSyscallFramePort<Frame> port,
        TaskSyscallTrapCallFrameAdapter<Frame>& adapter) noexcept
        -> TaskSyscallFrameCaller<Frame, Tick>
    {
        return TaskSyscallFrameCaller<Frame, Tick>{
            port, make_task_syscall_call_frame_adapter(adapter)};
    }
}
