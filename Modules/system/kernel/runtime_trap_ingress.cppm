module;

#include <array>
#include <cstddef>

export module kernel.runtime_trap_ingress;

export import kernel.runtime_trap;
import kernel.eda;
import util.core;

export namespace kernel {
    enum class TrapIngressStage : util::u8 {
        decode = 0,
        dispatch,
        writeback,
    };

    [[nodiscard]] constexpr const char* trap_ingress_stage_name(
        TrapIngressStage stage) noexcept
    {
        switch (stage) {
        case TrapIngressStage::decode:
            return "decode";
        case TrapIngressStage::dispatch:
            return "dispatch";
        case TrapIngressStage::writeback:
            return "writeback";
        }
        return "unknown";
    }

    template <typename Tick>
    struct RuntimeTrapIngressTraceEvent {
        Tick stamp{};
        TrapIngressStage stage{TrapIngressStage::decode};
        TrapService service{TrapService::invalid};
        TrapOrigin origin{TrapOrigin::kernel_thread};
        TaskId task{};
        bool task_valid{false};
        TrapDisposition disposition{TrapDisposition::rejected};
        TrapError error{TrapError::none};
        util::u64 arg0{0};
        util::u64 arg1{0};
        util::u64 arg2{0};
        util::u64 arg3{0};
        util::u64 value{0};
        bool ok{false};
    };

    template <typename Tick>
    [[nodiscard]] constexpr TrapFrameView trap_frame_view_from_ingress_trace_event(
        const RuntimeTrapIngressTraceEvent<Tick>& event) noexcept
    {
        return TrapFrameView{
            .service_id = static_cast<util::u16>(event.service),
            .arg0 = event.arg0,
            .arg1 = event.arg1,
            .arg2 = event.arg2,
            .arg3 = event.arg3,
            .origin = event.origin,
            .task = event.task,
            .task_valid = event.task_valid,
        };
    }

    template <typename Tick>
    [[nodiscard]] constexpr TrapRequest trap_request_from_ingress_trace_event(
        const RuntimeTrapIngressTraceEvent<Tick>& event) noexcept
    {
        return trap_request_from_frame(
            trap_frame_view_from_ingress_trace_event(event));
    }

    template <typename Tick>
    [[nodiscard]] constexpr TrapYieldCurrentView trap_yield_current_view(
        const RuntimeTrapIngressTraceEvent<Tick>& event) noexcept
    {
        return trap_yield_current_view(
            trap_request_from_ingress_trace_event(event));
    }

    template <typename Tick>
    [[nodiscard]] constexpr TrapSleepUntilView<Tick> trap_sleep_until_view(
        const RuntimeTrapIngressTraceEvent<Tick>& event) noexcept
    {
        return trap_sleep_until_view<Tick>(
            trap_request_from_ingress_trace_event(event));
    }

    template <typename Tick>
    [[nodiscard]] constexpr TrapDebugWriteView trap_debug_write_view(
        const RuntimeTrapIngressTraceEvent<Tick>& event) noexcept
    {
        return trap_debug_write_view(
            trap_request_from_ingress_trace_event(event));
    }

    template <typename Tick>
    [[nodiscard]] constexpr TrapCapabilityCallView trap_capability_call_view(
        const RuntimeTrapIngressTraceEvent<Tick>& event) noexcept
    {
        return trap_capability_call_view(
            trap_request_from_ingress_trace_event(event));
    }

    template <typename Tick>
    [[nodiscard]] constexpr TrapSemanticProjection trap_semantic_projection(
        const RuntimeTrapIngressTraceEvent<Tick>& event) noexcept
    {
        return trap_semantic_projection(
            trap_request_from_ingress_trace_event(event));
    }

    template <typename Tick, std::size_t Capacity>
    class RuntimeTrapIngressTraceBuffer {
    public:
        using tick_type = Tick;
        using value_type = RuntimeTrapIngressTraceEvent<Tick>;

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

    template <typename Frame>
    struct RuntimeTrapFrameAdapter {
        void* ctx{nullptr};
        bool (*capture)(void* ctx,
                        const Frame& frame,
                        TrapFrameView& out) noexcept {nullptr};
        bool (*apply_result)(void* ctx,
                             Frame& frame,
                             const TrapResult& result) noexcept {nullptr};
    };

    template <typename Frame>
    [[nodiscard]] constexpr bool runtime_trap_frame_adapter_ready(
        const RuntimeTrapFrameAdapter<Frame>& adapter) noexcept
    {
        return adapter.capture != nullptr && adapter.apply_result != nullptr;
    }

    template <typename TrapBridge,
              typename Frame,
              typename TraceBuffer =
                  RuntimeTrapIngressTraceBuffer<typename TrapBridge::tick_type, 1>>
    class RuntimeTrapIngress {
    public:
        using trap_type = TrapBridge;
        using frame_type = Frame;
        using tick_type = typename TrapBridge::tick_type;
        using trace_type = TraceBuffer;
        using adapter_type = RuntimeTrapFrameAdapter<Frame>;

        RuntimeTrapIngress(TrapBridge& trap,
                           adapter_type adapter,
                           TraceBuffer* trace = nullptr) noexcept
            : trap_(&trap), adapter_(adapter), trace_(trace)
        {
        }

        [[nodiscard]] TrapBridge& trap() noexcept
        {
            return *trap_;
        }

        [[nodiscard]] const TrapBridge& trap() const noexcept
        {
            return *trap_;
        }

        [[nodiscard]] const adapter_type& adapter() const noexcept
        {
            return adapter_;
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
            using time_source =
                typename TrapBridge::runtime_type::scheduler_type::TimeSource;
            const auto now = time_source::now();

            TrapFrameView view{};
            if (!runtime_trap_frame_adapter_ready(adapter_)) {
                const auto result = TrapResult{
                    .disposition = TrapDisposition::rejected,
                    .error = TrapError::unbound_adapter,
                    .value = 0,
                };
                trace_push(now, TrapIngressStage::decode, view, result, false);
                return result;
            }

            if (!adapter_.capture(adapter_.ctx, frame, view)) {
                const auto result = TrapResult{
                    .disposition = TrapDisposition::rejected,
                    .error = TrapError::decode_failed,
                    .value = 0,
                };
                trace_push(now, TrapIngressStage::decode, view, result, false);
                return result;
            }

            const auto decode_ok = TrapResult{
                .disposition = TrapDisposition::handled,
                .error = TrapError::none,
                .value = 0,
            };
            trace_push(
                now, TrapIngressStage::decode, view, decode_ok, true);

            const auto result = trap_->dispatch_frame(view);
            trace_push(now,
                       TrapIngressStage::dispatch,
                       view,
                       result,
                       result.ok());

            if (!adapter_.apply_result(adapter_.ctx, frame, result)) {
                const auto writeback_failed = TrapResult{
                    .disposition = TrapDisposition::rejected,
                    .error = TrapError::writeback_failed,
                    .value = result.value,
                };
                trace_push(now,
                           TrapIngressStage::writeback,
                           view,
                           writeback_failed,
                           false);
                return writeback_failed;
            }

            trace_push(now, TrapIngressStage::writeback, view, result, true);
            return result;
        }

    private:
        void trace_push(tick_type stamp,
                        TrapIngressStage stage,
                        const TrapFrameView& view,
                        const TrapResult& result,
                        bool ok) noexcept
        {
            if (trace_ == nullptr) {
                return;
            }

            (void)trace_->push(typename TraceBuffer::value_type{
                .stamp = stamp,
                .stage = stage,
                .service = static_cast<TrapService>(view.service_id),
                .origin = view.origin,
                .task = view.task,
                .task_valid = view.task_valid,
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

        TrapBridge* trap_{nullptr};
        adapter_type adapter_{};
        TraceBuffer* trace_{nullptr};
    };

    template <typename Frame>
    struct RuntimeTrapIngressPort {
        void* self{nullptr};
        TrapResult (*dispatch_frame_fn)(void* self, Frame* frame) noexcept {nullptr};

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

    template <typename Frame, typename Tick>
    struct RuntimeTrapCallFrameAdapter {
        void* ctx{nullptr};
        bool (*make_yield_frame)(void* ctx,
                                 TrapYieldCurrentView yield,
                                 Frame& out) noexcept {nullptr};
        bool (*make_sleep_frame)(void* ctx,
                                 TrapSleepUntilView<Tick> sleep,
                                 Frame& out) noexcept {nullptr};
        bool (*make_debug_write_frame)(void* ctx,
                                       TrapDebugWriteView write,
                                       Frame& out) noexcept {nullptr};
        bool (*make_capability_call_frame)(void* ctx,
                                           TrapCapabilityCallView capability,
                                           Frame& out) noexcept {nullptr};
        bool (*result_ready)(void* ctx,
                             const Frame& frame,
                             const TrapResult& result) noexcept {nullptr};
    };

    template <typename Frame, typename Tick>
    [[nodiscard]] constexpr bool runtime_trap_call_frame_adapter_ready(
        const RuntimeTrapCallFrameAdapter<Frame, Tick>& adapter) noexcept
    {
        return adapter.make_yield_frame != nullptr &&
               adapter.make_sleep_frame != nullptr;
    }

    template <typename Frame, typename Tick>
    class RuntimeTrapIngressCaller {
    public:
        using frame_type = Frame;
        using tick_type = Tick;
        using ingress_type = RuntimeTrapIngressPort<Frame>;
        using adapter_type = RuntimeTrapCallFrameAdapter<Frame, Tick>;

        RuntimeTrapIngressCaller(ingress_type ingress = {},
                                 adapter_type adapter = {}) noexcept
            : ingress_(ingress), adapter_(adapter)
        {
        }

        [[nodiscard]] bool valid() const noexcept
        {
            return ingress_.valid() &&
                   runtime_trap_call_frame_adapter_ready(adapter_);
        }

        [[nodiscard]] const ingress_type& ingress() const noexcept
        {
            return ingress_;
        }

        [[nodiscard]] const adapter_type& adapter() const noexcept
        {
            return adapter_;
        }

        void bind_ingress(ingress_type ingress) noexcept
        {
            ingress_ = ingress;
        }

        void bind_adapter(adapter_type adapter) noexcept
        {
            adapter_ = adapter;
        }

        [[nodiscard]] TrapResult yield_current() const noexcept
        {
            return yield_current(TrapYieldCurrentView{});
        }

        [[nodiscard]] TrapResult yield_current(
            TrapYieldCurrentView yield) const noexcept
        {
            return dispatch_with([this, yield](Frame& frame) noexcept {
                return adapter_.make_yield_frame(adapter_.ctx, yield, frame);
            });
        }

        [[nodiscard]] TrapResult sleep_current_until(Tick due) const noexcept
        {
            return sleep_current_until(TrapSleepUntilView<Tick>{
                .due = due,
            });
        }

        [[nodiscard]] TrapResult sleep_current_until(
            TrapSleepUntilView<Tick> sleep) const noexcept
        {
            return dispatch_with([this, sleep](Frame& frame) noexcept {
                return adapter_.make_sleep_frame(adapter_.ctx, sleep, frame);
            });
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
            if (adapter_.make_debug_write_frame == nullptr) {
                return TrapResult{
                    .disposition = TrapDisposition::rejected,
                    .error = TrapError::unbound_adapter,
                    .value = 0,
                };
            }

            return dispatch_with([this, write](Frame& frame) noexcept {
                return adapter_.make_debug_write_frame(adapter_.ctx,
                                                       write,
                                                       frame);
            });
        }

        [[nodiscard]] TrapResult capability_call(
            util::u64 capability_id,
            util::u64 operation,
            util::u64 payload = 0) const noexcept
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
            if (adapter_.make_capability_call_frame == nullptr) {
                return TrapResult{
                    .disposition = TrapDisposition::rejected,
                    .error = TrapError::unbound_adapter,
                    .value = 0,
                };
            }

            return dispatch_with(
                [this, capability](Frame& frame) noexcept {
                    return adapter_.make_capability_call_frame(adapter_.ctx,
                                                               capability,
                                                               frame);
                });
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

            const auto result = ingress_.dispatch_frame(frame);
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

        ingress_type ingress_{};
        adapter_type adapter_{};
    };

    namespace detail {
        template <typename Ingress>
        [[nodiscard]] TrapResult runtime_trap_ingress_dispatch_adapter(
            void* self,
            typename Ingress::frame_type* frame) noexcept
        {
            return static_cast<Ingress*>(self)->dispatch(*frame);
        }
    }

    template <typename Ingress>
    [[nodiscard]] auto make_runtime_trap_ingress_port(Ingress& ingress) noexcept
        -> RuntimeTrapIngressPort<typename Ingress::frame_type>
    {
        return RuntimeTrapIngressPort<typename Ingress::frame_type>{
            .self = &ingress,
            .dispatch_frame_fn =
                &detail::runtime_trap_ingress_dispatch_adapter<Ingress>,
        };
    }

    template <typename Frame, typename Tick>
    [[nodiscard]] auto make_runtime_trap_ingress_caller(
        RuntimeTrapIngressPort<Frame> ingress,
        RuntimeTrapCallFrameAdapter<Frame, Tick> adapter) noexcept
        -> RuntimeTrapIngressCaller<Frame, Tick>
    {
        return RuntimeTrapIngressCaller<Frame, Tick>{ingress, adapter};
    }
}
