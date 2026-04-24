#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>

import kernel.capabilities;
import kernel.config;
import kernel.eda;
import kernel.evt;
import kernel.runtime_bridge;
import kernel.scheduler;
import kernel.task_message_service_pump;
import kernel.task_message_syscall_pump;
import kernel.task_state;
import kernel.task_syscall_table;
import kernel.thread;

namespace demo {
    struct Config : kernel::KernelConfig {
        static constexpr bool enable_timer = true;
        static constexpr std::size_t priority_levels = 2;
        static constexpr std::size_t evtq_capacity = 64;
        static constexpr std::size_t timer_capacity = 16;
        static constexpr bool enable_trace = true;
        static constexpr std::size_t trace_capacity = 32;
    };

    struct ManualTimeSource {
        using Tick = std::uint64_t;

        static Tick now() noexcept
        {
            return ticks_;
        }

        static void reset() noexcept
        {
            ticks_ = 0;
        }

        static void advance(Tick delta) noexcept
        {
            ticks_ += delta;
        }

    private:
        inline static Tick ticks_{0};
    };

    struct Caps {
        using TimeSource = ManualTimeSource;
        using IrqGuard = kernel::NoopIrqGuard;
        using Wakeup = kernel::NoopWakeup;
        using SwiTrigger = kernel::NoopSwiTrigger;
    };

    inline constexpr std::size_t kReplyCount{4u};
    inline constexpr std::size_t kCompletionCount{5u};
    inline constexpr std::size_t kDrainBudget{2u};
    inline constexpr std::uint32_t kServerBootstrapPayload{1u};
    inline constexpr std::uint32_t kClientBootstrapPayload{2u};
    inline constexpr std::uint32_t kIdlePayload{0xE1u};
    inline constexpr std::array<ManualTimeSource::Tick, 4> kWaitDuePlan{
        3u,
        7u,
        11u,
        15u,
    };
    inline constexpr std::array<ManualTimeSource::Tick, kReplyCount>
        kReplyDuePlan{
            14u,
            18u,
            22u,
            26u,
        };
    inline constexpr ManualTimeSource::Tick kMissingReplyDue{30u};
    inline constexpr std::array<std::uint64_t, kReplyCount> kExpectedTokens{
        0xC1u,
        0xC2u,
        0xC3u,
        0xC4u,
    };
    inline constexpr std::array<std::uint64_t, kReplyCount> kExpectedSequences{
        0x61u,
        0x62u,
        0x63u,
        0x64u,
    };
    inline constexpr std::uint64_t kMissingToken{0xCFu};
    inline constexpr std::uint64_t kMissingSequence{0x6Fu};
    inline constexpr std::array<std::uint64_t, kReplyCount> kExpectedValues{
        1u,
        21u,
        42u,
        0u,
    };
    inline constexpr std::array<kernel::TrapDisposition, kReplyCount>
        kExpectedDispositions{
            kernel::TrapDisposition::handled,
            kernel::TrapDisposition::handled,
            kernel::TrapDisposition::handled,
            kernel::TrapDisposition::unsupported,
        };
    inline constexpr std::array<kernel::TrapError, kReplyCount> kExpectedErrors{
        kernel::TrapError::none,
        kernel::TrapError::none,
        kernel::TrapError::none,
        kernel::TrapError::unsupported_service,
    };
    inline constexpr auto kInvalidSyscall =
        static_cast<kernel::TaskSyscallId>(99u);

    struct MessageSyscallFrame {
        std::uint16_t syscall_id{0};
        std::uint64_t arg0{0};
        std::uint64_t arg1{0};
        std::uint64_t arg2{0};
        std::uint64_t arg3{0};
        std::uint64_t return_value{0};
        kernel::TrapDisposition disposition{
            kernel::TrapDisposition::rejected};
        kernel::TrapError error{kernel::TrapError::none};
        bool writeback_seen{false};
    };

    struct SharedState {
        bool mailbox_valid{false};
        bool dispatcher_valid{false};
        bool frame_store_valid{false};
        bool frame_bridge_valid{false};
        bool message_frame_bridge_valid{false};
        bool service_loop_valid{false};
        bool service_drain_valid{false};
        bool service_pump_valid{false};
        bool syscall_client_valid{false};
        bool syscall_pump_valid{false};
        bool server_bootstrapped{false};
        bool client_bootstrapped{false};
        bool enqueue_ok{false};
        bool kick_ok{false};
        bool missing_issue_erased{false};
        bool unexpected_timeout{false};
        bool missing_timeout_seen{false};
        std::uint32_t idle_runs{0};
        std::uint32_t server_runs{0};
        std::uint32_t client_runs{0};
        std::uint32_t server_timeouts{0};
        std::uint32_t replies_received{0};
        std::uint32_t timeouts_received{0};
        std::uint32_t completions_received{0};
        std::size_t total_served{0};
        std::size_t wait_arm_index{0};
        std::size_t wait_arm_successes{0};
        std::array<bool, kCompletionCount> completion_timeout{};
        std::array<std::uint64_t, kCompletionCount> completion_tokens{};
        std::array<std::uint64_t, kCompletionCount> completion_sequences{};
        std::array<std::uint64_t, kCompletionCount> completion_values{};
        std::array<kernel::TrapDisposition, kCompletionCount>
            completion_dispositions{};
        std::array<kernel::TrapError, kCompletionCount> completion_errors{};
    };

    struct FakeDispatchSurfaceState {
        bool bound{true};
        std::uint32_t yield_calls{0};
        std::uint32_t sleep_calls{0};
        std::uint32_t capability_calls{0};
        std::uint64_t last_due{0};
        std::uint64_t last_capability_id{0};
        std::uint64_t last_capability_operation{0};
        std::uint64_t last_capability_payload{0};
    };

    struct FakeDispatchSurface {
        using tick_type = std::uint64_t;

        FakeDispatchSurfaceState* state{nullptr};

        [[nodiscard]] bool valid() const noexcept
        {
            return state != nullptr && state->bound;
        }

        [[nodiscard]] kernel::TrapResult yield_current(
            kernel::TrapYieldCurrentView) const noexcept
        {
            if (!valid()) {
                return unbound_result();
            }

            ++state->yield_calls;
            return handled_result(1u);
        }

        [[nodiscard]] kernel::TrapResult sleep_current_until(
            kernel::TrapSleepUntilView<tick_type> sleep) const noexcept
        {
            if (!valid()) {
                return unbound_result();
            }

            ++state->sleep_calls;
            state->last_due = sleep.due;
            return handled_result(sleep.due);
        }

        [[nodiscard]] kernel::TrapResult debug_write(
            kernel::TrapDebugWriteView) const noexcept
        {
            return unbound_result();
        }

        [[nodiscard]] kernel::TrapResult capability_call(
            kernel::TrapCapabilityCallView capability) const noexcept
        {
            if (!valid()) {
                return unbound_result();
            }

            ++state->capability_calls;
            state->last_capability_id = capability.capability_id;
            state->last_capability_operation = capability.operation;
            state->last_capability_payload = capability.payload;
            return handled_result(capability.capability_id +
                                  capability.operation +
                                  capability.payload);
        }

    private:
        [[nodiscard]] static constexpr kernel::TrapResult handled_result(
            std::uint64_t value) noexcept
        {
            return kernel::TrapResult{
                .disposition = kernel::TrapDisposition::handled,
                .error = kernel::TrapError::none,
                .value = value,
            };
        }

        [[nodiscard]] static constexpr kernel::TrapResult unbound_result()
            noexcept
        {
            return kernel::TrapResult{
                .disposition = kernel::TrapDisposition::rejected,
                .error = kernel::TrapError::unbound_bridge,
                .value = 0,
            };
        }
    };

    struct FrameAdapterState {
        std::uint32_t capture_calls{0};
        std::uint32_t writeback_calls{0};
    };

    struct FrameResultReadyState {
        std::uint32_t ready_calls{0};
        std::uint64_t last_value{0};
        kernel::TrapDisposition last_disposition{
            kernel::TrapDisposition::rejected};
        kernel::TrapError last_error{kernel::TrapError::none};
        bool last_writeback_seen{false};
    };

    struct CallerAdapterState {
        std::uint32_t make_calls{0};
        std::uint32_t capture_result_calls{0};
        kernel::TaskSyscallId last_syscall{kernel::TaskSyscallId::invalid};
        std::uint64_t last_arg0{0};
        std::uint64_t last_arg1{0};
        std::uint64_t last_arg2{0};
        std::uint64_t last_reply_value{0};
        kernel::TrapDisposition last_disposition{
            kernel::TrapDisposition::rejected};
        kernel::TrapError last_error{kernel::TrapError::none};
        bool last_writeback_seen{false};
    };

    bool capture_message_syscall_frame(
        void* ctx,
        const MessageSyscallFrame& frame,
        kernel::TaskSyscallFrameView& out) noexcept
    {
        auto* state = static_cast<FrameAdapterState*>(ctx);
        if (state != nullptr) {
            ++state->capture_calls;
        }

        out = kernel::TaskSyscallFrameView{
            .syscall = static_cast<kernel::TaskSyscallId>(frame.syscall_id),
            .arg0 = frame.arg0,
            .arg1 = frame.arg1,
            .arg2 = frame.arg2,
            .arg3 = frame.arg3,
        };
        return true;
    }

    bool apply_message_syscall_result(void* ctx,
                                      MessageSyscallFrame& frame,
                                      const kernel::TrapResult& result) noexcept
    {
        auto* state = static_cast<FrameAdapterState*>(ctx);
        if (state != nullptr) {
            ++state->writeback_calls;
        }

        frame.return_value = result.value;
        frame.disposition = result.disposition;
        frame.error = result.error;
        frame.writeback_seen = true;
        return true;
    }

    bool message_syscall_frame_result_ready(
        void* ctx,
        const MessageSyscallFrame& frame,
        const kernel::TrapResult& result) noexcept
    {
        auto* state = static_cast<FrameResultReadyState*>(ctx);
        if (state != nullptr) {
            ++state->ready_calls;
            state->last_value = result.value;
            state->last_disposition = result.disposition;
            state->last_error = result.error;
            state->last_writeback_seen = frame.writeback_seen;
        }

        return frame.writeback_seen && frame.return_value == result.value &&
               frame.disposition == result.disposition &&
               frame.error == result.error;
    }

    bool make_message_syscall_call_frame(
        void* ctx,
        kernel::TaskSyscallRequest request,
        MessageSyscallFrame& out) noexcept
    {
        auto* state = static_cast<CallerAdapterState*>(ctx);
        if (state != nullptr) {
            ++state->make_calls;
            state->last_syscall = request.syscall;
            state->last_arg0 = request.arg0;
            state->last_arg1 = request.arg1;
            state->last_arg2 = request.arg2;
        }

        out = MessageSyscallFrame{
            .syscall_id = static_cast<std::uint16_t>(request.syscall),
            .arg0 = request.arg0,
            .arg1 = request.arg1,
            .arg2 = request.arg2,
            .arg3 = request.arg3,
        };
        return true;
    }

    bool capture_message_syscall_call_result(
        void* ctx,
        const MessageSyscallFrame& frame,
        std::uint64_t reply_value,
        kernel::TrapResult& out) noexcept
    {
        auto* state = static_cast<CallerAdapterState*>(ctx);
        if (state != nullptr) {
            ++state->capture_result_calls;
            state->last_reply_value = reply_value;
            state->last_disposition = frame.disposition;
            state->last_error = frame.error;
            state->last_writeback_seen = frame.writeback_seen;
        }

        out = kernel::TrapResult{
            .disposition = frame.disposition,
            .error = frame.error,
            .value = reply_value,
        };
        return frame.writeback_seen && frame.return_value == reply_value;
    }

    struct IdleContext {
        SharedState* shared{nullptr};
    };

    struct ServerContext {
        SharedState* shared{nullptr};
    };

    struct ClientContext {
        SharedState* shared{nullptr};
    };

    void idle_step(IdleContext& context,
                   kernel::ThreadControl&,
                   kernel::Event event);
    void server_step(ServerContext& context,
                     kernel::ThreadControl&,
                     kernel::Event event);
    void client_step(ClientContext& context,
                     kernel::ThreadControl&,
                     kernel::Event event);

    inline constexpr kernel::Priority kIdlePriority{0};
    inline constexpr kernel::Priority kWorkerPriority{1};

    using IdleTask = kernel::ThreadTask<IdleContext, &idle_step, kIdlePriority>;
    using ServerTask =
        kernel::ThreadTask<ServerContext, &server_step, kWorkerPriority>;
    using ClientTask =
        kernel::ThreadTask<ClientContext, &client_step, kWorkerPriority>;

    using Registry = kernel::TaskRegistry<IdleTask, ServerTask, ClientTask>;
    using RunningScheduler =
        kernel::Scheduler<Config, Registry, Caps, kernel::state::Running>;
    using Mailbox = kernel::RuntimeMailbox<RunningScheduler, 8, 8, 4>;
    using TaskMessages = kernel::TaskMessageApi<Mailbox>;
    using FrameStore =
        kernel::TaskMessageSyscallFrameStore<MessageSyscallFrame, 8>;
    using MessageTable = kernel::TaskMessageTable<1>;
    using MessageDispatcher = kernel::TaskMessageDispatcher<TaskMessages, MessageTable>;
    using ServiceLoop = kernel::TaskMessageServiceLoop<MessageDispatcher>;
    using ServiceDrain = kernel::TaskMessageServiceDrain<ServiceLoop>;
    using ServicePump = kernel::TaskMessageServicePump<ServiceDrain>;
    using SyscallTable = kernel::TaskSyscallTable<3>;
    using FrameBridge =
        kernel::TaskSyscallFrameBridge<SyscallTable, MessageSyscallFrame>;
    using MessageFrameBridge = kernel::TaskMessageSyscallFrameBridge<
        FrameStore,
        kernel::TaskSyscallFramePort<MessageSyscallFrame>,
        kernel::TaskMessageSyscallFrameResultAdapter<MessageSyscallFrame>>;
    using CallerAdapter =
        kernel::TaskMessageSyscallFrameCallAdapter<MessageSyscallFrame>;
    using FrameCaller = kernel::TaskMessageSyscallFrameCaller<
        TaskMessages,
        FrameStore,
        CallerAdapter>;
    using SyscallClient = kernel::TaskMessageSyscallClient<FrameCaller>;
    using PumpTrace = kernel::TaskMessageSyscallPumpTraceBuffer<16>;
    using SyscallPump =
        kernel::TaskMessageSyscallPump<SyscallClient, 8, 8, PumpTrace>;

    inline constexpr auto kIdleId = Registry::id_of<IdleTask>();
    inline constexpr auto kServerId = Registry::id_of<ServerTask>();
    inline constexpr auto kClientId = Registry::id_of<ClientTask>();

    inline ServicePump* server_service_pump{nullptr};
    inline SyscallPump* client_syscall_pump{nullptr};

    [[nodiscard]] kernel::Event make_idle_event() noexcept
    {
        return kernel::make_event(kernel::EventId::user0, kIdlePayload);
    }

    [[nodiscard]] kernel::Event make_server_bootstrap_event() noexcept
    {
        return kernel::make_event(
            kernel::EventId::user0,
            static_cast<std::uint32_t>(kServerBootstrapPayload));
    }

    [[nodiscard]] kernel::Event make_client_bootstrap_event() noexcept
    {
        return kernel::make_event(
            kernel::EventId::user0,
            static_cast<std::uint32_t>(kClientBootstrapPayload));
    }

    [[nodiscard]] ManualTimeSource::Tick next_server_due(
        const SharedState& shared) noexcept
    {
        const auto index =
            shared.wait_arm_index < kWaitDuePlan.size() ? shared.wait_arm_index
                                                        : kWaitDuePlan.size() - 1u;
        return kWaitDuePlan[index];
    }

    void mark_wait_arm(SharedState& shared, bool armed) noexcept
    {
        if (!armed) {
            return;
        }

        ++shared.wait_arm_successes;
        ++shared.wait_arm_index;
    }

    [[nodiscard]] bool enqueue_requests(SharedState& shared) noexcept
    {
        if (client_syscall_pump == nullptr) {
            return false;
        }

        const bool ok =
            client_syscall_pump->enqueue_yield(kReplyDuePlan[0]) &&
            client_syscall_pump->enqueue_sleep_until(
                kernel::TrapSleepUntilView<ManualTimeSource::Tick>{
                    .due = 21u,
                },
                kReplyDuePlan[1]) &&
            client_syscall_pump->enqueue_capability_call(
                kernel::TrapCapabilityCallView{
                    .capability_id = 7u,
                    .operation = 2u,
                    .payload = 33u,
                },
                kReplyDuePlan[2]) &&
            client_syscall_pump->enqueue(
                kernel::TaskSyscallRequest{
                    .syscall = kInvalidSyscall,
                    .arg0 = 0xAAu,
                    .arg1 = 0xBBu,
                },
                kReplyDuePlan[3]) &&
            client_syscall_pump->enqueue(
                kClientId,
                kMissingToken,
                kMissingSequence,
                kernel::make_task_syscall_yield_request(),
                kMissingReplyDue);
        shared.enqueue_ok = ok;
        std::printf("[client] enqueue ok=%d pending=%zu\n",
                    ok ? 1 : 0,
                    client_syscall_pump->pending_requests());
        return ok;
    }

    void idle_step(IdleContext& context,
                   kernel::ThreadControl&,
                   kernel::Event event)
    {
        if (event.id == kernel::EventId::init) {
            std::printf("[idle] init\n");
            return;
        }

        if (context.shared == nullptr) {
            return;
        }

        ++context.shared->idle_runs;
        std::printf("[idle] run=%u completions=%u timeout=%d now=%llu\n",
                    context.shared->idle_runs,
                    context.shared->completions_received,
                    context.shared->missing_timeout_seen ? 1 : 0,
                    static_cast<unsigned long long>(ManualTimeSource::now()));
    }

    void server_step(ServerContext& context,
                     kernel::ThreadControl&,
                     kernel::Event event)
    {
        if (event.id == kernel::EventId::init) {
            std::printf("[server] init\n");
            return;
        }

        if (context.shared == nullptr || server_service_pump == nullptr ||
            !server_service_pump->valid()) {
            return;
        }

        const auto due = next_server_due(*context.shared);
        const auto result = server_service_pump->step(event, kDrainBudget, due);

        if (result.reason == kernel::TaskMessageServicePumpReason::none &&
            !result.progressed && !result.bootstrap_consumed &&
            !result.hold_ready) {
            return;
        }

        ++context.shared->server_runs;
        context.shared->total_served += result.drain.served;
        mark_wait_arm(*context.shared, result.wait_armed);

        switch (result.reason) {
        case kernel::TaskMessageServicePumpReason::bootstrap:
            std::printf("[server] bootstrap due=%llu armed=%d\n",
                        static_cast<unsigned long long>(due),
                        result.wait_armed ? 1 : 0);
            return;
        case kernel::TaskMessageServicePumpReason::timeout:
            ++context.shared->server_timeouts;
            std::printf("[server] timeout count=%u due=%llu armed=%d\n",
                        context.shared->server_timeouts,
                        static_cast<unsigned long long>(due),
                        result.wait_armed ? 1 : 0);
            return;
        case kernel::TaskMessageServicePumpReason::queue_empty:
        case kernel::TaskMessageServicePumpReason::budget_reached:
        case kernel::TaskMessageServicePumpReason::none:
            std::printf("[server] step reason=%s served=%zu due=%llu\n",
                        kernel::task_message_service_pump_reason_name(
                            result.reason),
                        result.drain.served,
                        static_cast<unsigned long long>(due));
            return;
        }
    }

    void client_step(ClientContext& context,
                     kernel::ThreadControl&,
                     kernel::Event event)
    {
        if (event.id == kernel::EventId::init) {
            std::printf("[client] init\n");
            return;
        }

        if (context.shared == nullptr || client_syscall_pump == nullptr ||
            !client_syscall_pump->valid()) {
            return;
        }

        if (event.id == kernel::EventId::user0 &&
            kernel::payload_u32(event) == kClientBootstrapPayload) {
            ++context.shared->client_runs;
            context.shared->enqueue_ok = enqueue_requests(*context.shared);
            context.shared->kick_ok = client_syscall_pump->kick();
            std::printf("[client] kick=%d busy=%d pending=%zu\n",
                        context.shared->kick_ok ? 1 : 0,
                        client_syscall_pump->busy() ? 1 : 0,
                        client_syscall_pump->pending_requests());
            return;
        }

        const auto result = client_syscall_pump->step(event);
        if (result.issued &&
            result.issued_request.token == kMissingToken &&
            !context.shared->missing_issue_erased) {
            context.shared->missing_issue_erased =
                client_syscall_pump->client().caller().frames().erase(
                    kClientId, kMissingToken);
            std::printf("[client] erase-missing token=%llu seq=%llu ok=%d\n",
                        static_cast<unsigned long long>(kMissingToken),
                        static_cast<unsigned long long>(kMissingSequence),
                        context.shared->missing_issue_erased ? 1 : 0);
        }

        kernel::TaskMessageSyscallPumpCompletion<kernel::RuntimeMailboxReply>
            completion{};
        while (client_syscall_pump->receive_completion(completion)) {
            ++context.shared->client_runs;
            const auto index =
                static_cast<std::size_t>(context.shared->completions_received);
            if (index < kCompletionCount) {
                context.shared->completion_timeout[index] = completion.timeout;
                context.shared->completion_tokens[index] = completion.token;
                context.shared->completion_sequences[index] =
                    completion.request_sequence;
                context.shared->completion_values[index] = completion.trap.value;
                context.shared->completion_dispositions[index] =
                    completion.trap.disposition;
                context.shared->completion_errors[index] = completion.trap.error;
            }

            ++context.shared->completions_received;
            if (completion.timeout) {
                ++context.shared->timeouts_received;
                context.shared->missing_timeout_seen =
                    completion.token == kMissingToken &&
                    completion.request_sequence == kMissingSequence;
                if (!context.shared->missing_timeout_seen) {
                    context.shared->unexpected_timeout = true;
                }

                std::printf(
                    "[client] timeout token=%llu seq=%llu missing=%d\n",
                    static_cast<unsigned long long>(completion.token),
                    static_cast<unsigned long long>(
                        completion.request_sequence),
                    context.shared->missing_timeout_seen ? 1 : 0);
            } else {
                ++context.shared->replies_received;
                std::printf(
                    "[client] reply idx=%zu token=%llu seq=%llu value=%llu disp=%s err=%s\n",
                    index,
                    static_cast<unsigned long long>(completion.token),
                    static_cast<unsigned long long>(
                        completion.request_sequence),
                    static_cast<unsigned long long>(completion.trap.value),
                    kernel::trap_disposition_name(
                        completion.trap.disposition),
                    kernel::trap_error_name(completion.trap.error));
            }
        }
    }

    struct PumpTraceStatus {
        bool ok{true};
        std::size_t issue_count{0};
        std::size_t reply_count{0};
        std::size_t timeout_count{0};
        bool drop_seen{false};
    };

    [[nodiscard]] PumpTraceStatus inspect_pump_trace(PumpTrace& trace) noexcept
    {
        PumpTraceStatus status{};

        for (std::size_t index = 0; index < trace.size(); ++index) {
            const auto* record = trace.at(index);
            if (record == nullptr) {
                continue;
            }

            std::printf(
                "[pump-trace] seq=%llu kind=%s owner=%llu token=%llu req_seq=%llu due=%llu pending_req=%zu pending_completion=%zu ok=%d timeout=%d value=%llu disp=%s err=%s\n",
                static_cast<unsigned long long>(record->sequence),
                kernel::task_message_syscall_pump_trace_kind_name(
                    record->kind),
                static_cast<unsigned long long>(record->owner.value),
                static_cast<unsigned long long>(record->token),
                static_cast<unsigned long long>(record->request_sequence),
                static_cast<unsigned long long>(record->wait_due),
                record->pending_requests,
                record->pending_completions,
                record->ok ? 1 : 0,
                record->timeout ? 1 : 0,
                static_cast<unsigned long long>(record->reply_value),
                kernel::trap_disposition_name(record->disposition),
                kernel::trap_error_name(record->error));

            switch (record->sequence) {
            case 1u:
                status.ok = status.ok &&
                            record->kind ==
                                kernel::TaskMessageSyscallPumpTraceKind::issue &&
                            record->owner == kClientId &&
                            record->token == kExpectedTokens[0] &&
                            record->request_sequence == kExpectedSequences[0] &&
                            record->wait_due == kReplyDuePlan[0] && record->ok;
                break;
            case 2u:
                status.ok = status.ok &&
                            record->kind ==
                                kernel::TaskMessageSyscallPumpTraceKind::reply &&
                            record->token == kExpectedTokens[0] &&
                            record->request_sequence == kExpectedSequences[0] &&
                            record->reply_value == kExpectedValues[0] &&
                            record->disposition == kExpectedDispositions[0] &&
                            record->error == kExpectedErrors[0] && record->ok;
                break;
            case 3u:
                status.ok = status.ok &&
                            record->kind ==
                                kernel::TaskMessageSyscallPumpTraceKind::issue &&
                            record->token == kExpectedTokens[1] &&
                            record->request_sequence == kExpectedSequences[1] &&
                            record->wait_due == kReplyDuePlan[1] && record->ok;
                break;
            case 4u:
                status.ok = status.ok &&
                            record->kind ==
                                kernel::TaskMessageSyscallPumpTraceKind::reply &&
                            record->token == kExpectedTokens[1] &&
                            record->request_sequence == kExpectedSequences[1] &&
                            record->reply_value == kExpectedValues[1] &&
                            record->disposition == kExpectedDispositions[1] &&
                            record->error == kExpectedErrors[1] && record->ok;
                break;
            case 5u:
                status.ok = status.ok &&
                            record->kind ==
                                kernel::TaskMessageSyscallPumpTraceKind::issue &&
                            record->token == kExpectedTokens[2] &&
                            record->request_sequence == kExpectedSequences[2] &&
                            record->wait_due == kReplyDuePlan[2] && record->ok;
                break;
            case 6u:
                status.ok = status.ok &&
                            record->kind ==
                                kernel::TaskMessageSyscallPumpTraceKind::reply &&
                            record->token == kExpectedTokens[2] &&
                            record->request_sequence == kExpectedSequences[2] &&
                            record->reply_value == kExpectedValues[2] &&
                            record->disposition == kExpectedDispositions[2] &&
                            record->error == kExpectedErrors[2] && record->ok;
                break;
            case 7u:
                status.ok = status.ok &&
                            record->kind ==
                                kernel::TaskMessageSyscallPumpTraceKind::issue &&
                            record->token == kExpectedTokens[3] &&
                            record->request_sequence == kExpectedSequences[3] &&
                            record->wait_due == kReplyDuePlan[3] && record->ok;
                break;
            case 8u:
                status.ok = status.ok &&
                            record->kind ==
                                kernel::TaskMessageSyscallPumpTraceKind::reply &&
                            record->token == kExpectedTokens[3] &&
                            record->request_sequence == kExpectedSequences[3] &&
                            record->reply_value == kExpectedValues[3] &&
                            record->disposition == kExpectedDispositions[3] &&
                            record->error == kExpectedErrors[3] && record->ok;
                break;
            case 9u:
                status.ok = status.ok &&
                            record->kind ==
                                kernel::TaskMessageSyscallPumpTraceKind::issue &&
                            record->token == kMissingToken &&
                            record->request_sequence == kMissingSequence &&
                            record->wait_due == kMissingReplyDue && record->ok;
                break;
            case 10u:
                status.ok = status.ok &&
                            record->kind ==
                                kernel::TaskMessageSyscallPumpTraceKind::timeout &&
                            record->token == kMissingToken &&
                            record->request_sequence == kMissingSequence &&
                            record->timeout &&
                            record->disposition ==
                                kernel::TrapDisposition::rejected &&
                            record->error == kernel::TrapError::none &&
                            record->reply_value == 0u && record->ok;
                break;
            default:
                status.ok = false;
                break;
            }

            switch (record->kind) {
            case kernel::TaskMessageSyscallPumpTraceKind::issue:
                ++status.issue_count;
                break;
            case kernel::TaskMessageSyscallPumpTraceKind::reply:
                ++status.reply_count;
                break;
            case kernel::TaskMessageSyscallPumpTraceKind::timeout:
                ++status.timeout_count;
                break;
            case kernel::TaskMessageSyscallPumpTraceKind::completion_drop:
                status.drop_seen = true;
                status.ok = false;
                break;
            }
        }

        status.ok = status.ok && status.issue_count == kCompletionCount &&
                    status.reply_count == kReplyCount &&
                    status.timeout_count == 1u && !status.drop_seen &&
                    trace.size() == 10u;
        return status;
    }
}

int main()
{
    demo::ManualTimeSource::reset();

    demo::Registry registry{};
    auto& idle = registry.get<demo::IdleTask>();
    auto& server = registry.get<demo::ServerTask>();
    auto& client = registry.get<demo::ClientTask>();

    demo::SharedState shared{};
    idle.context.shared = &shared;
    server.context.shared = &shared;
    client.context.shared = &shared;

    demo::Caps caps{};
    auto created = kernel::make_scheduler<demo::Config>(registry, caps);
    auto running = kernel::start(std::move(created));
    demo::FrameStore frame_store{};
    demo::FakeDispatchSurfaceState dispatch_state{};
    demo::FrameAdapterState frame_adapter_state{};
    demo::FrameResultReadyState result_ready_state{};
    demo::CallerAdapterState caller_adapter_state{};
    demo::PumpTrace pump_trace{};

    kernel::RuntimeBridge runtime{
        running,
        demo::kIdleId,
        demo::make_idle_event(),
    };
    demo::Mailbox mailbox{running, demo::kServerId};
    demo::TaskMessages server_task_messages =
        kernel::make_task_message_api(mailbox);
    demo::TaskMessages client_task_messages =
        kernel::make_task_message_api(mailbox);
    auto syscall_dispatcher = kernel::make_task_syscall_dispatcher(
        demo::FakeDispatchSurface{
            .state = &dispatch_state,
        });
    auto syscall_table = kernel::make_task_syscall_table(
        std::array<kernel::TaskSyscallHandlerEntry, 3>{
            kernel::task_syscall_handler_entry(
                kernel::TaskSyscallId::yield,
                kernel::make_task_syscall_handler(syscall_dispatcher)),
            kernel::task_syscall_handler_entry(
                kernel::TaskSyscallId::sleep_until,
                kernel::make_task_syscall_handler(syscall_dispatcher)),
            kernel::task_syscall_handler_entry(
                kernel::TaskSyscallId::capability_call,
                kernel::make_task_syscall_handler(syscall_dispatcher)),
        });
    demo::FrameBridge frame_bridge = kernel::make_task_syscall_frame_bridge(
        syscall_table,
        kernel::TaskSyscallFrameAdapter<demo::MessageSyscallFrame>{
            .ctx = &frame_adapter_state,
            .capture = &demo::capture_message_syscall_frame,
            .apply_result = &demo::apply_message_syscall_result,
        });
    demo::MessageFrameBridge message_frame_bridge{
        frame_store,
        kernel::make_task_syscall_frame_port(frame_bridge),
        kernel::TaskMessageSyscallFrameResultAdapter<demo::MessageSyscallFrame>{
            .ctx = &result_ready_state,
            .result_ready = &demo::message_syscall_frame_result_ready,
        },
    };
    auto message_table = kernel::make_task_message_table(
        std::array<kernel::TaskMessageHandlerEntry, 1>{
            kernel::task_message_handler_entry(
                kernel::task_message_syscall_frame_request_label,
                kernel::task_message_syscall_frame_request_label_name(),
                kernel::make_task_message_handler(message_frame_bridge)),
        });
    demo::MessageDispatcher message_dispatcher =
        kernel::make_task_message_dispatcher(server_task_messages, message_table);
    demo::ServiceLoop service_loop =
        kernel::make_task_message_service_loop(message_dispatcher);
    demo::ServiceDrain service_drain =
        kernel::make_task_message_service_drain(service_loop);
    demo::ServicePump service_pump =
        kernel::make_task_message_service_pump(
            service_drain,
            demo::make_server_bootstrap_event());
    demo::FrameCaller frame_caller =
        kernel::make_task_message_syscall_frame_caller(
            client_task_messages,
            frame_store,
            demo::CallerAdapter{
                .ctx = &caller_adapter_state,
                .make_frame = &demo::make_message_syscall_call_frame,
                .capture_result = &demo::capture_message_syscall_call_result,
            });
    frame_caller.bind_cursors(0xC1u, 0x61u);
    demo::SyscallClient syscall_client =
        kernel::make_task_message_syscall_client(frame_caller);
    demo::SyscallPump syscall_pump =
        kernel::make_task_message_syscall_pump<
            demo::SyscallClient,
            8,
            8>(syscall_client, &pump_trace);

    demo::server_service_pump = &service_pump;
    demo::client_syscall_pump = &syscall_pump;

    while (running.run_once()) {
    }

    shared.mailbox_valid =
        mailbox.valid() && mailbox.server() == demo::kServerId &&
        server_task_messages.valid() && client_task_messages.valid();
    shared.dispatcher_valid = message_dispatcher.valid();
    shared.frame_store_valid = frame_store.valid();
    shared.frame_bridge_valid = frame_bridge.valid();
    shared.message_frame_bridge_valid = message_frame_bridge.valid();
    shared.service_loop_valid = service_loop.valid();
    shared.service_drain_valid = service_drain.valid();
    shared.service_pump_valid = service_pump.valid();
    shared.syscall_client_valid = syscall_client.valid();
    shared.syscall_pump_valid = syscall_pump.valid();
    shared.server_bootstrapped =
        runtime.bootstrap_worker(
            demo::kServerId,
            demo::make_server_bootstrap_event());

    std::size_t loops = 0;
    while ((shared.completions_received < demo::kCompletionCount ||
            !shared.missing_timeout_seen) &&
           loops < 96u) {
        if (shared.server_timeouts >= 1u && !shared.client_bootstrapped) {
            shared.client_bootstrapped =
                runtime.bootstrap_worker(
                    demo::kClientId,
                    demo::make_client_bootstrap_event());
        }

        (void)runtime.run_once_or_idle(demo::ManualTimeSource::now());
        demo::ManualTimeSource::advance(1u);
        ++loops;
    }

    const auto pump_status = demo::inspect_pump_trace(pump_trace);
    const bool reply_values_ok =
        shared.completion_timeout[0] == false &&
        shared.completion_timeout[1] == false &&
        shared.completion_timeout[2] == false &&
        shared.completion_timeout[3] == false &&
        shared.completion_timeout[4] == true &&
        shared.completion_tokens[0] == demo::kExpectedTokens[0] &&
        shared.completion_tokens[1] == demo::kExpectedTokens[1] &&
        shared.completion_tokens[2] == demo::kExpectedTokens[2] &&
        shared.completion_tokens[3] == demo::kExpectedTokens[3] &&
        shared.completion_tokens[4] == demo::kMissingToken &&
        shared.completion_sequences[0] == demo::kExpectedSequences[0] &&
        shared.completion_sequences[1] == demo::kExpectedSequences[1] &&
        shared.completion_sequences[2] == demo::kExpectedSequences[2] &&
        shared.completion_sequences[3] == demo::kExpectedSequences[3] &&
        shared.completion_sequences[4] == demo::kMissingSequence &&
        shared.completion_values[0] == demo::kExpectedValues[0] &&
        shared.completion_values[1] == demo::kExpectedValues[1] &&
        shared.completion_values[2] == demo::kExpectedValues[2] &&
        shared.completion_values[3] == demo::kExpectedValues[3] &&
        shared.completion_values[4] == 0u &&
        shared.completion_dispositions[0] ==
            demo::kExpectedDispositions[0] &&
        shared.completion_dispositions[1] ==
            demo::kExpectedDispositions[1] &&
        shared.completion_dispositions[2] ==
            demo::kExpectedDispositions[2] &&
        shared.completion_dispositions[3] ==
            demo::kExpectedDispositions[3] &&
        shared.completion_dispositions[4] ==
            kernel::TrapDisposition::rejected &&
        shared.completion_errors[0] == demo::kExpectedErrors[0] &&
        shared.completion_errors[1] == demo::kExpectedErrors[1] &&
        shared.completion_errors[2] == demo::kExpectedErrors[2] &&
        shared.completion_errors[3] == demo::kExpectedErrors[3] &&
        shared.completion_errors[4] == kernel::TrapError::none;
    const bool surface_ok =
        dispatch_state.yield_calls == 1u &&
        dispatch_state.sleep_calls == 1u &&
        dispatch_state.capability_calls == 1u &&
        dispatch_state.last_due == 21u &&
        dispatch_state.last_capability_id == 7u &&
        dispatch_state.last_capability_operation == 2u &&
        dispatch_state.last_capability_payload == 33u &&
        frame_adapter_state.capture_calls == demo::kReplyCount &&
        frame_adapter_state.writeback_calls == demo::kReplyCount &&
        result_ready_state.ready_calls == demo::kReplyCount &&
        result_ready_state.last_value == 0u &&
        result_ready_state.last_disposition ==
            kernel::TrapDisposition::unsupported &&
        result_ready_state.last_error ==
            kernel::TrapError::unsupported_service &&
        result_ready_state.last_writeback_seen &&
        caller_adapter_state.make_calls == demo::kCompletionCount &&
        caller_adapter_state.capture_result_calls == demo::kReplyCount &&
        caller_adapter_state.last_syscall == kernel::TaskSyscallId::yield &&
        caller_adapter_state.last_arg0 == 0u &&
        caller_adapter_state.last_arg1 == 0u &&
        caller_adapter_state.last_arg2 == 0u &&
        caller_adapter_state.last_reply_value == 0u &&
        caller_adapter_state.last_disposition ==
            kernel::TrapDisposition::unsupported &&
        caller_adapter_state.last_error ==
            kernel::TrapError::unsupported_service &&
        caller_adapter_state.last_writeback_seen;
    const bool ok =
        shared.mailbox_valid && shared.dispatcher_valid &&
        shared.frame_store_valid && shared.frame_bridge_valid &&
        shared.message_frame_bridge_valid && shared.service_loop_valid &&
        shared.service_drain_valid && shared.service_pump_valid &&
        shared.syscall_client_valid && shared.syscall_pump_valid &&
        shared.server_bootstrapped && shared.client_bootstrapped &&
        shared.enqueue_ok && shared.kick_ok && shared.missing_issue_erased &&
        !shared.unexpected_timeout && shared.missing_timeout_seen &&
        shared.replies_received == demo::kReplyCount &&
        shared.timeouts_received == 1u &&
        shared.completions_received == demo::kCompletionCount &&
        shared.total_served == demo::kCompletionCount &&
        shared.server_timeouts >= 1u && shared.wait_arm_successes >= 1u &&
        shared.idle_runs >= 1u && shared.server_runs >= 5u &&
        shared.client_runs == 6u && reply_values_ok && surface_ok &&
        pump_status.ok && frame_store.pending() == 0u &&
        mailbox.pending_requests() == 0u && mailbox.pending_replies() == 0u &&
        mailbox.reply_waiters() == 0u && mailbox.receive_waiting() &&
        syscall_pump.pending_requests() == 0u &&
        syscall_pump.pending_completions() == 0u && !syscall_pump.busy() &&
        loops < 96u;

    std::printf(
        "[runtime-task-message-syscall-pump-demo] ok=%d valid=%d client=%d pump=%d server_boot=%d client_boot=%d enqueue=%d kick=%d replies=%u timeouts=%u served=%zu server_timeouts=%u pending_req=%zu pending_completion=%zu pending_frames=%zu reply_waiters=%zu waiting=%d loops=%llu\n",
        ok ? 1 : 0,
        shared.mailbox_valid ? 1 : 0,
        shared.syscall_client_valid ? 1 : 0,
        shared.syscall_pump_valid ? 1 : 0,
        shared.server_bootstrapped ? 1 : 0,
        shared.client_bootstrapped ? 1 : 0,
        shared.enqueue_ok ? 1 : 0,
        shared.kick_ok ? 1 : 0,
        shared.replies_received,
        shared.timeouts_received,
        shared.total_served,
        shared.server_timeouts,
        syscall_pump.pending_requests(),
        syscall_pump.pending_completions(),
        frame_store.pending(),
        mailbox.reply_waiters(),
        mailbox.receive_waiting() ? 1 : 0,
        static_cast<unsigned long long>(loops));
    std::printf(
        "[runtime-task-message-syscall-pump-values] token0=%llu token1=%llu token2=%llu token3=%llu timeout_token=%llu seq0=%llu seq1=%llu seq2=%llu seq3=%llu timeout_seq=%llu value0=%llu value1=%llu value2=%llu value3=%llu invalid_err=%s timeout_err=%s\n",
        static_cast<unsigned long long>(shared.completion_tokens[0]),
        static_cast<unsigned long long>(shared.completion_tokens[1]),
        static_cast<unsigned long long>(shared.completion_tokens[2]),
        static_cast<unsigned long long>(shared.completion_tokens[3]),
        static_cast<unsigned long long>(shared.completion_tokens[4]),
        static_cast<unsigned long long>(shared.completion_sequences[0]),
        static_cast<unsigned long long>(shared.completion_sequences[1]),
        static_cast<unsigned long long>(shared.completion_sequences[2]),
        static_cast<unsigned long long>(shared.completion_sequences[3]),
        static_cast<unsigned long long>(shared.completion_sequences[4]),
        static_cast<unsigned long long>(shared.completion_values[0]),
        static_cast<unsigned long long>(shared.completion_values[1]),
        static_cast<unsigned long long>(shared.completion_values[2]),
        static_cast<unsigned long long>(shared.completion_values[3]),
        kernel::trap_error_name(shared.completion_errors[3]),
        kernel::trap_error_name(shared.completion_errors[4]));
    std::printf(
        "[runtime-task-message-syscall-pump-trace] ok=%d issue=%zu reply=%zu timeout=%zu drop=%d erased=%d\n",
        pump_status.ok ? 1 : 0,
        pump_status.issue_count,
        pump_status.reply_count,
        pump_status.timeout_count,
        pump_status.drop_seen ? 1 : 0,
        shared.missing_issue_erased ? 1 : 0);
    return ok ? 0 : 1;
}
