module;

#include <array>
#include <cstddef>
#include <cstdio>
#include <string_view>

export module kernel.runtime_trap_ingress;

export import kernel.runtime_trap;
import kernel.eda;
import semantic.core;
import util.core;

namespace kernel::detail {
    template <typename... Args>
    [[nodiscard]] inline std::size_t append_fmt(
        char* out,
        std::size_t max,
        std::size_t offset,
        const char* fmt,
        Args... args) noexcept
    {
        if (out == nullptr || max == 0u || offset >= max) {
            return offset;
        }

        const int written =
            std::snprintf(out + offset, max - offset, fmt, args...);
        if (written <= 0) {
            out[offset] = '\0';
            return offset;
        }

        const auto size = static_cast<std::size_t>(written);
        if (size >= (max - offset)) {
            return max - 1u;
        }

        return offset + size;
    }

    [[nodiscard]] constexpr const char* json_bool(bool value) noexcept
    {
        return value ? "true" : "false";
    }
}

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
        util::u64 sequence{0};
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
    struct RuntimeTrapIngressForensicSnapshot {
        using value_type = RuntimeTrapIngressTraceEvent<Tick>;

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

        [[nodiscard]] constexpr TrapIngressStage terminal_stage() const noexcept
        {
            return has_terminal ? terminal.stage : TrapIngressStage::decode;
        }

        [[nodiscard]] constexpr bool ok() const noexcept
        {
            return has_terminal && terminal.ok;
        }

        [[nodiscard]] constexpr bool decode_failed() const noexcept
        {
            return has_terminal &&
                   terminal.stage == TrapIngressStage::decode &&
                   terminal.error == TrapError::decode_failed;
        }

        [[nodiscard]] constexpr bool writeback_failed() const noexcept
        {
            return has_terminal &&
                   terminal.stage == TrapIngressStage::writeback &&
                   terminal.error == TrapError::writeback_failed;
        }
    };

    template <typename Tick>
    struct RuntimeTrapIngressWitness {
        Tick terminal_stamp{};
        util::u64 sequence{0};
        bool ready{false};
        bool terminal_ok{false};
        bool has_decode{false};
        bool has_dispatch{false};
        bool has_writeback{false};
        TrapIngressStage terminal_stage{TrapIngressStage::decode};
        TrapService terminal_service{TrapService::invalid};
        TrapOrigin terminal_origin{TrapOrigin::kernel_thread};
        TrapDisposition terminal_disposition{TrapDisposition::rejected};
        TrapError terminal_error{TrapError::none};
        bool has_last_failure{false};
        util::u64 last_failure_sequence{0};
        TrapIngressStage last_failure_stage{TrapIngressStage::decode};
        TrapService last_failure_service{TrapService::invalid};
        TrapOrigin last_failure_origin{TrapOrigin::kernel_thread};
        TrapDisposition last_failure_disposition{TrapDisposition::rejected};
        TrapError last_failure_error{TrapError::none};
        bool has_trace{false};
        bool has_terminal{false};

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
                (terminal_stage == TrapIngressStage::decode ||
                 terminal_stage == TrapIngressStage::writeback)) {
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
                const auto stage =
                    terminal_failed ? terminal_stage : last_failure_stage;
                const auto error =
                    terminal_failed ? terminal_error : last_failure_error;

                if (stage == TrapIngressStage::decode &&
                    error != TrapError::none) {
                    return semantic::FailureDomain::selection;
                }

                if (stage == TrapIngressStage::dispatch &&
                    error != TrapError::none) {
                    return semantic::FailureDomain::route;
                }

                if (stage == TrapIngressStage::writeback &&
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
            return "runtime-trap-ingress-witness.summary";
        }
    };

    template <typename Tick>
    using RuntimeTrapIngressForensicWitness = RuntimeTrapIngressWitness<Tick>;

    template <typename Tick>
    struct RuntimeTrapIngressWitnessHandoffTarget {
        const RuntimeTrapIngressWitness<Tick>* witness{nullptr};

        [[nodiscard]] constexpr std::string_view entry_name() const noexcept
        {
            return "runtime-trap-ingress-witness";
        }

        [[nodiscard]] constexpr std::string_view
        selected_summary_path() const noexcept
        {
            return witness != nullptr ? witness->summary_path()
                                      : std::string_view{
                                            "runtime-trap-ingress-witness.summary"};
        }
    };

    static_assert(semantic::WitnessCarrier<RuntimeTrapIngressWitness<util::u64>>);
    static_assert(
        semantic::HandoffTarget<RuntimeTrapIngressWitnessHandoffTarget<util::u64>>);

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

    template <typename Tick>
    [[nodiscard]] constexpr bool same_trap_ingress_attempt(
        const RuntimeTrapIngressTraceEvent<Tick>& lhs,
        const RuntimeTrapIngressTraceEvent<Tick>& rhs) noexcept
    {
        return lhs.sequence != 0u && lhs.sequence == rhs.sequence;
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

    template <typename Tick, std::size_t Capacity>
    [[nodiscard]] RuntimeTrapIngressForensicSnapshot<Tick>
    trap_ingress_forensic_snapshot(
        const RuntimeTrapIngressTraceBuffer<Tick, Capacity>& trace) noexcept
    {
        RuntimeTrapIngressForensicSnapshot<Tick> snapshot{};
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

            if (!same_trap_ingress_attempt(*event, snapshot.terminal)) {
                continue;
            }

            switch (event->stage) {
            case TrapIngressStage::decode:
                if (!snapshot.has_decode) {
                    snapshot.has_decode = true;
                    snapshot.decode = *event;
                }
                break;
            case TrapIngressStage::dispatch:
                if (!snapshot.has_dispatch) {
                    snapshot.has_dispatch = true;
                    snapshot.dispatch = *event;
                }
                break;
            case TrapIngressStage::writeback:
                if (!snapshot.has_writeback) {
                    snapshot.has_writeback = true;
                    snapshot.writeback = *event;
                }
                break;
            }
        }

        return snapshot;
    }

    template <typename Tick>
    [[nodiscard]] constexpr RuntimeTrapIngressWitness<Tick> trap_ingress_witness(
        const RuntimeTrapIngressForensicSnapshot<Tick>& snapshot) noexcept
    {
        RuntimeTrapIngressWitness<Tick> witness{};
        witness.has_trace = snapshot.has_terminal || snapshot.has_decode ||
                            snapshot.has_dispatch || snapshot.has_writeback ||
                            snapshot.has_last_failure;
        witness.has_terminal = snapshot.has_terminal;
        if (!snapshot.has_terminal) {
            return witness;
        }

        witness.terminal_stamp = snapshot.terminal.stamp;
        witness.sequence = snapshot.terminal.sequence;
        witness.ready = snapshot.has_decode && snapshot.has_dispatch &&
                        snapshot.has_writeback;
        witness.terminal_ok = snapshot.terminal.ok;
        witness.has_decode = snapshot.has_decode;
        witness.has_dispatch = snapshot.has_dispatch;
        witness.has_writeback = snapshot.has_writeback;
        witness.terminal_stage = snapshot.terminal.stage;
        witness.terminal_service = snapshot.terminal.service;
        witness.terminal_origin = snapshot.terminal.origin;
        witness.terminal_disposition = snapshot.terminal.disposition;
        witness.terminal_error = snapshot.terminal.error;
        witness.has_last_failure = snapshot.has_last_failure;

        if (snapshot.has_last_failure) {
            witness.last_failure_sequence = snapshot.last_failure.sequence;
            witness.last_failure_stage = snapshot.last_failure.stage;
            witness.last_failure_service = snapshot.last_failure.service;
            witness.last_failure_origin = snapshot.last_failure.origin;
            witness.last_failure_disposition =
                snapshot.last_failure.disposition;
            witness.last_failure_error = snapshot.last_failure.error;
        }

        return witness;
    }

    template <typename Tick, std::size_t Capacity>
    [[nodiscard]] RuntimeTrapIngressWitness<Tick> trap_ingress_witness(
        const RuntimeTrapIngressTraceBuffer<Tick, Capacity>& trace) noexcept
    {
        return trap_ingress_witness(trap_ingress_forensic_snapshot(trace));
    }

    template <typename Tick>
    [[nodiscard]] constexpr bool trap_ingress_witness_ready(
        const RuntimeTrapIngressWitness<Tick>& witness) noexcept
    {
        return witness.ready;
    }

    template <typename Tick>
    [[nodiscard]] constexpr RuntimeTrapIngressWitnessHandoffTarget<Tick>
    trap_ingress_witness_handoff_target(
        const RuntimeTrapIngressWitness<Tick>& witness) noexcept
    {
        return RuntimeTrapIngressWitnessHandoffTarget<Tick>{
            .witness = &witness,
        };
    }

    template <typename Tick>
    [[nodiscard]] std::size_t format_trap_ingress_witness_json(
        char* out,
        std::size_t max,
        const RuntimeTrapIngressWitness<Tick>& witness) noexcept
    {
        if (out == nullptr || max == 0u) {
            return 0u;
        }

        out[0] = '\0';
        std::size_t offset = 0;
        offset = detail::append_fmt(
            out,
            max,
            offset,
            "{\"ready\":%s,\"ok\":%s,\"sequence\":%llu,\"stamp\":%llu,",
            detail::json_bool(witness.ready),
            detail::json_bool(witness.ok()),
            static_cast<unsigned long long>(witness.sequence),
            static_cast<unsigned long long>(witness.terminal_stamp));
        offset = detail::append_fmt(
            out,
            max,
            offset,
            "\"stages\":{\"decode\":%s,\"dispatch\":%s,\"writeback\":%s},",
            detail::json_bool(witness.has_decode),
            detail::json_bool(witness.has_dispatch),
            detail::json_bool(witness.has_writeback));
        offset = detail::append_fmt(
            out,
            max,
            offset,
            "\"terminal\":{\"stage\":\"%s\",\"service\":\"%s\",\"origin\":\"%s\","
            "\"disposition\":\"%s\",\"error\":\"%s\"},",
            trap_ingress_stage_name(witness.terminal_stage),
            trap_service_name(witness.terminal_service),
            trap_origin_name(witness.terminal_origin),
            trap_disposition_name(witness.terminal_disposition),
            trap_error_name(witness.terminal_error));
        offset = detail::append_fmt(
            out,
            max,
            offset,
            "\"last_failure\":{\"present\":%s,\"prior_attempt\":%s,"
            "\"sequence\":%llu,\"stage\":\"%s\",\"service\":\"%s\","
            "\"origin\":\"%s\",\"disposition\":\"%s\",\"error\":\"%s\"}}",
            detail::json_bool(witness.has_last_failure),
            detail::json_bool(witness.last_failure_is_prior_attempt()),
            static_cast<unsigned long long>(witness.last_failure_sequence),
            trap_ingress_stage_name(witness.last_failure_stage),
            trap_service_name(witness.last_failure_service),
            trap_origin_name(witness.last_failure_origin),
            trap_disposition_name(witness.last_failure_disposition),
            trap_error_name(witness.last_failure_error));
        return offset;
    }

    template <typename Tick>
    [[nodiscard]] constexpr RuntimeTrapIngressForensicWitness<Tick>
    trap_ingress_forensic_witness(
        const RuntimeTrapIngressForensicSnapshot<Tick>& snapshot) noexcept
    {
        return trap_ingress_witness(snapshot);
    }

    template <typename Tick, std::size_t Capacity>
    [[nodiscard]] RuntimeTrapIngressForensicWitness<Tick>
    trap_ingress_forensic_witness(
        const RuntimeTrapIngressTraceBuffer<Tick, Capacity>& trace) noexcept
    {
        return trap_ingress_witness(trace);
    }

    template <typename Tick>
    [[nodiscard]] constexpr bool trap_ingress_forensic_witness_ready(
        const RuntimeTrapIngressForensicWitness<Tick>& witness) noexcept
    {
        return trap_ingress_witness_ready(witness);
    }

    template <typename Tick>
    [[nodiscard]] std::size_t format_trap_ingress_forensic_witness_json(
        char* out,
        std::size_t max,
        const RuntimeTrapIngressForensicWitness<Tick>& witness) noexcept
    {
        return format_trap_ingress_witness_json(out, max, witness);
    }

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
            const auto sequence = next_sequence_++;

            TrapFrameView view{};
            if (!runtime_trap_frame_adapter_ready(adapter_)) {
                const auto result = TrapResult{
                    .disposition = TrapDisposition::rejected,
                    .error = TrapError::unbound_adapter,
                    .value = 0,
                };
                trace_push(
                    now, sequence, TrapIngressStage::decode, view, result, false);
                return result;
            }

            if (!adapter_.capture(adapter_.ctx, frame, view)) {
                const auto result = TrapResult{
                    .disposition = TrapDisposition::rejected,
                    .error = TrapError::decode_failed,
                    .value = 0,
                };
                trace_push(
                    now, sequence, TrapIngressStage::decode, view, result, false);
                return result;
            }

            const auto decode_ok = TrapResult{
                .disposition = TrapDisposition::handled,
                .error = TrapError::none,
                .value = 0,
            };
            trace_push(
                now, sequence, TrapIngressStage::decode, view, decode_ok, true);

            const auto result = trap_->dispatch_frame(view);
            trace_push(now,
                       sequence,
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
                           sequence,
                           TrapIngressStage::writeback,
                           view,
                           writeback_failed,
                           false);
                return writeback_failed;
            }

            trace_push(
                now, sequence, TrapIngressStage::writeback, view, result, true);
            return result;
        }

    private:
        void trace_push(tick_type stamp,
                        util::u64 sequence,
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
                .sequence = sequence,
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
        util::u64 next_sequence_{1u};
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
