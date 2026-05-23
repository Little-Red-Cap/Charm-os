module;

#include <array>
#include <cstddef>
#include <string_view>

export module kernel.task_message_session_acceptor;

export import kernel.task_message_session_dispatch;
import semantic.core;
import util.core;

export namespace kernel {
    inline constexpr util::u16 task_message_session_channel_unmapped_slot =
        static_cast<util::u16>(0xFFFFu);

    struct TaskMessageSessionChannel {
        util::u64 service_id{0};
        const char* service_name{"session-service"};
        util::u64 session_handle{0};
        util::u64 open_payload{0};
        util::u16 channel_slot{task_message_session_channel_unmapped_slot};
    };

    struct TaskMessageSessionChannelHandler {
        void* self{nullptr};
        TrapResult (*request_fn)(
            void* self,
            const TaskMessageSessionChannel& channel,
            TaskMessageSessionRequestDispatchView request) noexcept {nullptr};
        TrapResult (*close_fn)(
            void* self,
            const TaskMessageSessionChannel& channel,
            TaskMessageSessionCloseDispatchView close) noexcept {nullptr};

        [[nodiscard]] bool valid() const noexcept
        {
            return request_fn != nullptr && close_fn != nullptr;
        }

        [[nodiscard]] TrapResult request(
            const TaskMessageSessionChannel& channel,
            TaskMessageSessionRequestDispatchView request_view) const noexcept
        {
            if (!valid()) {
                return TrapResult{
                    .disposition = TrapDisposition::rejected,
                    .error = TrapError::unbound_adapter,
                    .value = 0,
                };
            }

            return request_fn(self, channel, request_view);
        }

        [[nodiscard]] TrapResult close(
            const TaskMessageSessionChannel& channel,
            TaskMessageSessionCloseDispatchView close_view) const noexcept
        {
            if (!valid()) {
                return TrapResult{
                    .disposition = TrapDisposition::rejected,
                    .error = TrapError::unbound_adapter,
                    .value = 0,
                };
            }

            return close_fn(self, channel, close_view);
        }
    };

    namespace detail {
        template <typename Target>
        [[nodiscard]] TrapResult task_message_session_channel_request_adapter(
            void* self,
            const TaskMessageSessionChannel& channel,
            TaskMessageSessionRequestDispatchView request) noexcept
        {
            return static_cast<Target*>(self)->request(channel, request);
        }

        template <typename Target>
        [[nodiscard]] TrapResult task_message_session_channel_close_adapter(
            void* self,
            const TaskMessageSessionChannel& channel,
            TaskMessageSessionCloseDispatchView close) noexcept
        {
            return static_cast<Target*>(self)->close(channel, close);
        }
    }

    template <typename Target>
    [[nodiscard]] auto make_task_message_session_channel_handler(
        Target& target) noexcept -> TaskMessageSessionChannelHandler
    {
        return TaskMessageSessionChannelHandler{
            .self = &target,
            .request_fn =
                &detail::task_message_session_channel_request_adapter<Target>,
            .close_fn =
                &detail::task_message_session_channel_close_adapter<Target>,
        };
    }

    struct TaskMessageSessionChannelAcceptor {
        void* self{nullptr};
        TrapResult (*accept_fn)(void* self,
                                const TaskMessageSessionChannel& channel,
                                TaskMessageSessionChannelHandler& out_handler)
            noexcept {nullptr};

        [[nodiscard]] bool valid() const noexcept
        {
            return accept_fn != nullptr;
        }

        [[nodiscard]] TrapResult accept(
            const TaskMessageSessionChannel& channel,
            TaskMessageSessionChannelHandler& out_handler) const noexcept
        {
            if (!valid()) {
                return TrapResult{
                    .disposition = TrapDisposition::rejected,
                    .error = TrapError::unbound_adapter,
                    .value = 0,
                };
            }

            return accept_fn(self, channel, out_handler);
        }
    };

    namespace detail {
        template <typename Target>
        [[nodiscard]] TrapResult task_message_session_channel_accept_adapter(
            void* self,
            const TaskMessageSessionChannel& channel,
            TaskMessageSessionChannelHandler& out_handler) noexcept
        {
            return static_cast<Target*>(self)->accept(channel, out_handler);
        }
    }

    template <typename Target>
    [[nodiscard]] auto make_task_message_session_channel_acceptor(
        Target& target) noexcept -> TaskMessageSessionChannelAcceptor
    {
        return TaskMessageSessionChannelAcceptor{
            .self = &target,
            .accept_fn =
                &detail::task_message_session_channel_accept_adapter<Target>,
        };
    }

    struct TaskMessageSessionChannelSlot {
        bool active{false};
        TaskMessageSessionChannel channel{};
        TaskMessageSessionChannelHandler handler{};
    };

    struct TaskMessageSessionChannelLookup {
        const TaskMessageSessionChannelSlot* slot_entry{nullptr};
        util::u16 slot{task_message_session_channel_unmapped_slot};
        bool matched{false};
    };

    struct TaskMessageSessionServiceAcceptorResult {
        TaskMessageSessionActionKind action{
            TaskMessageSessionActionKind::none};
        util::u64 service_id{0};
        const char* service_name{"session-service"};
        util::u64 session_handle{0};
        util::u64 operation{0};
        util::u64 payload{0};
        util::u16 channel_slot{task_message_session_channel_unmapped_slot};
        bool acceptor_valid{false};
        bool channel_found{false};
        bool channel_bound{false};
        bool channel_closed{false};
        TrapResult trap{
            .disposition = TrapDisposition::rejected,
            .error = TrapError::unbound_adapter,
            .value = 0,
        };
    };

    struct TaskMessageSessionServiceAcceptorTraceEvent {
        util::u64 sequence{0};
        TaskMessageSessionActionKind action{
            TaskMessageSessionActionKind::none};
        util::u64 service_id{0};
        const char* service_name{"session-service"};
        util::u64 session_handle{0};
        util::u64 operation{0};
        util::u64 payload{0};
        util::u16 channel_slot{task_message_session_channel_unmapped_slot};
        bool acceptor_valid{false};
        bool channel_found{false};
        bool channel_bound{false};
        bool channel_closed{false};
        TrapDisposition disposition{TrapDisposition::rejected};
        TrapError error{TrapError::unbound_adapter};
        util::u64 value{0};
    };

    static_assert(
        semantic::reflected_member_names_match_when_enabled<
            TaskMessageSessionServiceAcceptorTraceEvent>(
            std::array<std::string_view, 15>{
                "sequence",
                "action",
                "service_id",
                "service_name",
                "session_handle",
                "operation",
                "payload",
                "channel_slot",
                "acceptor_valid",
                "channel_found",
                "channel_bound",
                "channel_closed",
                "disposition",
                "error",
                "value",
            }));

    struct TaskMessageSessionServiceAcceptorWitness {
        util::u64 sequence{0};
        bool ready{false};
        bool has_trace{false};
        TaskMessageSessionActionKind action{
            TaskMessageSessionActionKind::none};
        util::u64 service_id{0};
        util::u64 session_handle{0};
        util::u64 operation{0};
        util::u64 payload{0};
        util::u16 channel_slot{task_message_session_channel_unmapped_slot};
        bool acceptor_valid{false};
        bool channel_found{false};
        bool channel_bound{false};
        bool channel_closed{false};
        TrapDisposition disposition{TrapDisposition::rejected};
        TrapError error{TrapError::unbound_adapter};
        util::u64 value{0};

        [[nodiscard]] constexpr bool unbound_acceptor_branch_ok()
            const noexcept
        {
            return action == TaskMessageSessionActionKind::open &&
                   !acceptor_valid && !channel_bound &&
                   disposition == TrapDisposition::rejected &&
                   error == TrapError::unbound_adapter;
        }

        [[nodiscard]] constexpr bool open_bound_branch_ok() const noexcept
        {
            return action == TaskMessageSessionActionKind::open &&
                   acceptor_valid && channel_bound &&
                   channel_slot != task_message_session_channel_unmapped_slot &&
                   disposition == TrapDisposition::handled &&
                   error == TrapError::none;
        }

        [[nodiscard]] constexpr bool open_full_branch_ok() const noexcept
        {
            return action == TaskMessageSessionActionKind::open &&
                   acceptor_valid && !channel_bound &&
                   channel_slot == task_message_session_channel_unmapped_slot &&
                   disposition == TrapDisposition::rejected &&
                   error == TrapError::invalid_argument;
        }

        [[nodiscard]] constexpr bool open_unbound_handler_branch_ok()
            const noexcept
        {
            return action == TaskMessageSessionActionKind::open &&
                   acceptor_valid && !channel_bound &&
                   channel_slot != task_message_session_channel_unmapped_slot &&
                   disposition == TrapDisposition::rejected &&
                   error == TrapError::unbound_adapter;
        }

        [[nodiscard]] constexpr bool channel_missing_branch_ok() const noexcept
        {
            return (action == TaskMessageSessionActionKind::request ||
                    action == TaskMessageSessionActionKind::close) &&
                   !channel_found &&
                   channel_slot == task_message_session_channel_unmapped_slot &&
                   disposition == TrapDisposition::rejected &&
                   error == TrapError::invalid_argument;
        }

        [[nodiscard]] constexpr bool request_branch_ok() const noexcept
        {
            return action == TaskMessageSessionActionKind::request &&
                   channel_found &&
                   channel_slot != task_message_session_channel_unmapped_slot &&
                   !channel_bound && !channel_closed;
        }

        [[nodiscard]] constexpr bool close_branch_ok() const noexcept
        {
            if (action != TaskMessageSessionActionKind::close ||
                !channel_found ||
                channel_slot == task_message_session_channel_unmapped_slot ||
                channel_bound) {
                return false;
            }

            const bool close_ok =
                disposition == TrapDisposition::handled &&
                error == TrapError::none;
            return channel_closed == close_ok;
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

        [[nodiscard]] constexpr semantic::Verdict verdict() const noexcept
        {
            if (!ready) {
                return semantic::Verdict::collapsed;
            }

            if (unbound_acceptor_branch_ok() ||
                open_bound_branch_ok() || open_full_branch_ok() ||
                open_unbound_handler_branch_ok() ||
                channel_missing_branch_ok() || request_branch_ok() ||
                close_branch_ok()) {
                return semantic::Verdict::standing;
            }

            return semantic::Verdict::drifted;
        }

        [[nodiscard]] constexpr semantic::FailureDomain
        failure_domain() const noexcept
        {
            if (!ready) {
                return semantic::FailureDomain::input;
            }

            if (verdict() == semantic::Verdict::standing) {
                return semantic::FailureDomain::none;
            }

            if (!acceptor_valid || error == TrapError::unbound_adapter) {
                return semantic::FailureDomain::handoff;
            }

            if (channel_missing_branch_ok() ||
                channel_slot == task_message_session_channel_unmapped_slot) {
                return semantic::FailureDomain::route;
            }

            return semantic::FailureDomain::selection;
        }

        [[nodiscard]] constexpr std::string_view summary_path() const noexcept
        {
            return "task-message-session-acceptor-witness.summary";
        }
    };

    struct TaskMessageSessionServiceAcceptorWitnessHandoffTarget {
        const TaskMessageSessionServiceAcceptorWitness* witness{nullptr};

        [[nodiscard]] constexpr std::string_view entry_name() const noexcept
        {
            return "task-message-session-acceptor-witness";
        }

        [[nodiscard]] constexpr std::string_view
        selected_summary_path() const noexcept
        {
            return witness != nullptr ? witness->summary_path()
                                      : std::string_view{
                                            "task-message-session-acceptor-witness.summary"};
        }
    };

    static_assert(
        semantic::reflected_member_names_match_when_enabled<
            TaskMessageSessionServiceAcceptorWitness>(
            std::array<std::string_view, 16>{
                "sequence",
                "ready",
                "has_trace",
                "action",
                "service_id",
                "session_handle",
                "operation",
                "payload",
                "channel_slot",
                "acceptor_valid",
                "channel_found",
                "channel_bound",
                "channel_closed",
                "disposition",
                "error",
                "value",
            }));

    static_assert(
        semantic::WitnessCarrier<TaskMessageSessionServiceAcceptorWitness>);
    static_assert(
        semantic::HandoffTarget<
            TaskMessageSessionServiceAcceptorWitnessHandoffTarget>);

    [[nodiscard]] constexpr TaskMessageSessionServiceAcceptorWitness
    task_message_session_service_acceptor_witness(
        const TaskMessageSessionServiceAcceptorResult& result) noexcept
    {
        return TaskMessageSessionServiceAcceptorWitness{
            .ready = true,
            .action = result.action,
            .service_id = result.service_id,
            .session_handle = result.session_handle,
            .operation = result.operation,
            .payload = result.payload,
            .channel_slot = result.channel_slot,
            .acceptor_valid = result.acceptor_valid,
            .channel_found = result.channel_found,
            .channel_bound = result.channel_bound,
            .channel_closed = result.channel_closed,
            .disposition = result.trap.disposition,
            .error = result.trap.error,
            .value = result.trap.value,
        };
    }

    [[nodiscard]] constexpr TaskMessageSessionServiceAcceptorWitness
    task_message_session_service_acceptor_witness(
        const TaskMessageSessionServiceAcceptorTraceEvent& event) noexcept
    {
        return TaskMessageSessionServiceAcceptorWitness{
            .sequence = event.sequence,
            .ready = event.sequence != 0u,
            .has_trace = true,
            .action = event.action,
            .service_id = event.service_id,
            .session_handle = event.session_handle,
            .operation = event.operation,
            .payload = event.payload,
            .channel_slot = event.channel_slot,
            .acceptor_valid = event.acceptor_valid,
            .channel_found = event.channel_found,
            .channel_bound = event.channel_bound,
            .channel_closed = event.channel_closed,
            .disposition = event.disposition,
            .error = event.error,
            .value = event.value,
        };
    }

    [[nodiscard]] constexpr bool
    task_message_session_service_acceptor_witness_ready(
        const TaskMessageSessionServiceAcceptorWitness& witness) noexcept
    {
        return witness.ready;
    }

    [[nodiscard]] constexpr
        TaskMessageSessionServiceAcceptorWitnessHandoffTarget
        task_message_session_service_acceptor_witness_handoff_target(
            const TaskMessageSessionServiceAcceptorWitness& witness) noexcept
    {
        return TaskMessageSessionServiceAcceptorWitnessHandoffTarget{
            .witness = &witness,
        };
    }

    template <std::size_t Capacity>
    class TaskMessageSessionServiceAcceptorTraceBuffer {
    public:
        using value_type = TaskMessageSessionServiceAcceptorTraceEvent;

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
    [[nodiscard]] constexpr TaskMessageSessionServiceAcceptorWitness
    task_message_session_service_acceptor_witness(
        const TaskMessageSessionServiceAcceptorTraceBuffer<Capacity>& trace)
        noexcept
    {
        const auto* terminal =
            trace.size() == 0u ? nullptr : trace.at(trace.size() - 1u);
        if (terminal == nullptr) {
            return TaskMessageSessionServiceAcceptorWitness{};
        }

        return task_message_session_service_acceptor_witness(*terminal);
    }

    template <std::size_t ChannelCapacity,
              typename TraceBuffer =
                  TaskMessageSessionServiceAcceptorTraceBuffer<1>>
    class TaskMessageSessionServiceAcceptor {
    public:
        using slot_type = TaskMessageSessionChannelSlot;
        using result_type = TaskMessageSessionServiceAcceptorResult;
        using trace_type = TraceBuffer;

        static_assert(ChannelCapacity > 0);

        constexpr TaskMessageSessionServiceAcceptor() noexcept = default;

        constexpr explicit TaskMessageSessionServiceAcceptor(
            TaskMessageSessionChannelAcceptor acceptor,
            const char* service_name = "session-service",
            TraceBuffer* trace = nullptr) noexcept
            : acceptor_(acceptor), service_name_(service_name), trace_(trace)
        {
        }

        [[nodiscard]] bool valid() const noexcept
        {
            return acceptor_.valid();
        }

        [[nodiscard]] const char* service_name() const noexcept
        {
            return service_name_;
        }

        void bind_acceptor(TaskMessageSessionChannelAcceptor acceptor) noexcept
        {
            acceptor_ = acceptor;
        }

        void bind_service_name(const char* service_name) noexcept
        {
            service_name_ = service_name != nullptr ? service_name
                                                    : "session-service";
        }

        void bind_trace(TraceBuffer* trace) noexcept
        {
            trace_ = trace;
        }

        void reset_channels() noexcept
        {
            slots_ = {};
        }

        [[nodiscard]] const slot_type* channel(std::size_t index) const noexcept
        {
            if (index >= ChannelCapacity) {
                return nullptr;
            }

            return &slots_[index];
        }

        [[nodiscard]] std::size_t active_channels() const noexcept
        {
            std::size_t active = 0;
            for (const auto& slot : slots_) {
                if (slot.active) {
                    ++active;
                }
            }
            return active;
        }

        [[nodiscard]] TaskMessageSessionChannelLookup lookup_channel(
            util::u64 session_handle) const noexcept
        {
            for (std::size_t index = 0; index < ChannelCapacity; ++index) {
                if (!slots_[index].active ||
                    slots_[index].channel.session_handle != session_handle) {
                    continue;
                }

                return TaskMessageSessionChannelLookup{
                    .slot_entry = &slots_[index],
                    .slot = static_cast<util::u16>(index),
                    .matched = true,
                };
            }

            return TaskMessageSessionChannelLookup{};
        }

        [[nodiscard]] result_type open_session(util::u64 service_id,
                                               util::u64 session_handle,
                                               util::u64 payload = 0) noexcept
        {
            return dispatch_open(TaskMessageSessionOpenDispatchView{
                .service_id = service_id,
                .session_handle = session_handle,
                .payload = payload,
            });
        }

        [[nodiscard]] result_type request_session(util::u64 session_handle,
                                                  util::u64 operation,
                                                  util::u64 payload = 0) noexcept
        {
            const auto found = lookup_channel(session_handle);
            return dispatch_request(TaskMessageSessionRequestDispatchView{
                .service_id = found.slot_entry != nullptr
                                  ? found.slot_entry->channel.service_id
                                  : 0,
                .session_handle = session_handle,
                .operation = operation,
                .payload = payload,
            });
        }

        [[nodiscard]] result_type close_session(util::u64 session_handle,
                                                util::u64 reason = 0) noexcept
        {
            const auto found = lookup_channel(session_handle);
            return dispatch_close(TaskMessageSessionCloseDispatchView{
                .service_id = found.slot_entry != nullptr
                                  ? found.slot_entry->channel.service_id
                                  : 0,
                .session_handle = session_handle,
                .reason = reason,
            });
        }

        [[nodiscard]] result_type dispatch_open(
            TaskMessageSessionOpenDispatchView open) noexcept
        {
            auto result = result_type{
                .action = TaskMessageSessionActionKind::open,
                .service_id = open.service_id,
                .service_name = service_name_,
                .session_handle = open.session_handle,
                .payload = open.payload,
                .acceptor_valid = acceptor_.valid(),
            };

            if (!acceptor_.valid()) {
                result.trap = unbound_adapter_result();
                trace_push(result);
                return result;
            }

            const auto free_slot = first_free_channel_slot();
            result.channel_slot = free_slot.slot;
            if (!free_slot.found) {
                result.trap = invalid_argument_result();
                trace_push(result);
                return result;
            }

            const auto channel = TaskMessageSessionChannel{
                .service_id = open.service_id,
                .service_name = service_name_,
                .session_handle = open.session_handle,
                .open_payload = open.payload,
                .channel_slot = free_slot.slot,
            };
            auto bound_handler = TaskMessageSessionChannelHandler{};
            result.trap = acceptor_.accept(channel, bound_handler);
            if (result.trap.ok() && !bound_handler.valid()) {
                result.trap = unbound_adapter_result();
            }

            if (result.trap.ok()) {
                auto& slot = slots_[free_slot.slot];
                slot.active = true;
                slot.channel = channel;
                slot.handler = bound_handler;
                result.channel_bound = true;
            }

            trace_push(result);
            return result;
        }

        [[nodiscard]] result_type dispatch_request(
            TaskMessageSessionRequestDispatchView request) noexcept
        {
            auto result = result_type{
                .action = TaskMessageSessionActionKind::request,
                .service_id = request.service_id,
                .service_name = service_name_,
                .session_handle = request.session_handle,
                .operation = request.operation,
                .payload = request.payload,
                .acceptor_valid = acceptor_.valid(),
            };

            const auto found = lookup_channel(request.session_handle);
            result.channel_slot = found.slot;
            result.channel_found = found.matched;
            if (!found.matched || found.slot_entry == nullptr) {
                result.trap = invalid_argument_result();
                trace_push(result);
                return result;
            }

            if (!found.slot_entry->handler.valid()) {
                result.trap = unbound_adapter_result();
                trace_push(result);
                return result;
            }

            result.trap =
                found.slot_entry->handler.request(found.slot_entry->channel,
                                                 request);
            trace_push(result);
            return result;
        }

        [[nodiscard]] result_type dispatch_close(
            TaskMessageSessionCloseDispatchView close) noexcept
        {
            auto result = result_type{
                .action = TaskMessageSessionActionKind::close,
                .service_id = close.service_id,
                .service_name = service_name_,
                .session_handle = close.session_handle,
                .payload = close.reason,
                .acceptor_valid = acceptor_.valid(),
            };

            const auto found = lookup_channel(close.session_handle);
            result.channel_slot = found.slot;
            result.channel_found = found.matched;
            if (!found.matched || found.slot_entry == nullptr) {
                result.trap = invalid_argument_result();
                trace_push(result);
                return result;
            }

            if (!found.slot_entry->handler.valid()) {
                result.trap = unbound_adapter_result();
                trace_push(result);
                return result;
            }

            result.trap =
                found.slot_entry->handler.close(found.slot_entry->channel, close);
            if (result.trap.ok()) {
                slots_[found.slot] = {};
                result.channel_closed = true;
            }

            trace_push(result);
            return result;
        }

        [[nodiscard]] TrapResult open(
            TaskMessageSessionOpenDispatchView open) noexcept
        {
            return dispatch_open(open).trap;
        }

        [[nodiscard]] TrapResult request(
            TaskMessageSessionRequestDispatchView request) noexcept
        {
            return dispatch_request(request).trap;
        }

        [[nodiscard]] TrapResult close(
            TaskMessageSessionCloseDispatchView close) noexcept
        {
            return dispatch_close(close).trap;
        }

    private:
        struct FreeSlotLookup {
            util::u16 slot{task_message_session_channel_unmapped_slot};
            bool found{false};
        };

        [[nodiscard]] static constexpr TrapResult invalid_argument_result()
            noexcept
        {
            return TrapResult{
                .disposition = TrapDisposition::rejected,
                .error = TrapError::invalid_argument,
                .value = 0,
            };
        }

        [[nodiscard]] static constexpr TrapResult unbound_adapter_result()
            noexcept
        {
            return TrapResult{
                .disposition = TrapDisposition::rejected,
                .error = TrapError::unbound_adapter,
                .value = 0,
            };
        }

        [[nodiscard]] FreeSlotLookup first_free_channel_slot() const noexcept
        {
            for (std::size_t index = 0; index < ChannelCapacity; ++index) {
                if (slots_[index].active) {
                    continue;
                }

                return FreeSlotLookup{
                    .slot = static_cast<util::u16>(index),
                    .found = true,
                };
            }

            return FreeSlotLookup{};
        }

        void trace_push(const result_type& result) noexcept
        {
            if (trace_ == nullptr) {
                return;
            }

            (void)trace_->push(typename TraceBuffer::value_type{
                .sequence = ++sequence_,
                .action = result.action,
                .service_id = result.service_id,
                .service_name = result.service_name,
                .session_handle = result.session_handle,
                .operation = result.operation,
                .payload = result.payload,
                .channel_slot = result.channel_slot,
                .acceptor_valid = result.acceptor_valid,
                .channel_found = result.channel_found,
                .channel_bound = result.channel_bound,
                .channel_closed = result.channel_closed,
                .disposition = result.trap.disposition,
                .error = result.trap.error,
                .value = result.trap.value,
            });
        }

        TaskMessageSessionChannelAcceptor acceptor_{};
        const char* service_name_{"session-service"};
        std::array<slot_type, ChannelCapacity> slots_{};
        TraceBuffer* trace_{nullptr};
        util::u64 sequence_{0};
    };

    template <std::size_t ChannelCapacity>
    [[nodiscard]] auto make_task_message_session_service_acceptor(
        TaskMessageSessionChannelAcceptor acceptor,
        const char* service_name = "session-service") noexcept
        -> TaskMessageSessionServiceAcceptor<ChannelCapacity>
    {
        return TaskMessageSessionServiceAcceptor<ChannelCapacity>{
            acceptor,
            service_name,
        };
    }

    template <std::size_t ChannelCapacity,
              typename TraceBuffer>
    [[nodiscard]] auto make_task_message_session_service_acceptor(
        TaskMessageSessionChannelAcceptor acceptor,
        const char* service_name,
        TraceBuffer* trace) noexcept
        -> TaskMessageSessionServiceAcceptor<ChannelCapacity, TraceBuffer>
    {
        return TaskMessageSessionServiceAcceptor<ChannelCapacity, TraceBuffer>{
            acceptor,
            service_name,
            trace,
        };
    }

    template <std::size_t ChannelCapacity, typename Target>
    [[nodiscard]] auto make_task_message_session_service_acceptor(
        Target& target,
        const char* service_name = "session-service") noexcept
        -> TaskMessageSessionServiceAcceptor<ChannelCapacity>
    {
        return make_task_message_session_service_acceptor<ChannelCapacity>(
            make_task_message_session_channel_acceptor(target), service_name);
    }

    template <std::size_t ChannelCapacity,
              typename Target,
              typename TraceBuffer>
    [[nodiscard]] auto make_task_message_session_service_acceptor(
        Target& target,
        const char* service_name,
        TraceBuffer* trace) noexcept
        -> TaskMessageSessionServiceAcceptor<ChannelCapacity, TraceBuffer>
    {
        return make_task_message_session_service_acceptor<ChannelCapacity>(
            make_task_message_session_channel_acceptor(target),
            service_name,
            trace);
    }

    template <typename Acceptor>
    [[nodiscard]] auto task_message_session_service_acceptor_entry(
        util::u64 service_id,
        Acceptor& acceptor) noexcept -> TaskMessageSessionHandlerEntry
    {
        return task_message_session_handler_entry(
            service_id, make_task_message_session_handler(acceptor));
    }

    template <typename Acceptor>
    [[nodiscard]] auto task_message_session_service_acceptor_entry(
        util::u64 service_id,
        const char* service_name,
        Acceptor& acceptor) noexcept -> TaskMessageSessionHandlerEntry
    {
        return task_message_session_handler_entry(
            service_id,
            service_name,
            make_task_message_session_handler(acceptor));
    }
}
