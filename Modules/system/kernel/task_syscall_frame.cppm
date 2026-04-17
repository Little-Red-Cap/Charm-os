module;

#include <array>
#include <cstddef>

export module kernel.task_syscall_frame;

export import kernel.task_syscall_table;
export import kernel.runtime_trap_ingress;
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
