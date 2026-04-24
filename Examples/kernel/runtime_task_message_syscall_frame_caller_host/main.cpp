#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string_view>
#include <utility>

import kernel.capabilities;
import kernel.config;
import kernel.eda;
import kernel.evt;
import kernel.runtime_bridge;
import kernel.scheduler;
import kernel.task_message_service_pump;
import kernel.task_message_syscall_frame;
import kernel.task_message_syscall_frame_caller;
import kernel.task_state;
import kernel.task_syscall_table;
import kernel.thread;

namespace demo {
    using namespace std::literals;

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

    inline constexpr std::size_t kCallCount{4u};
    inline constexpr std::size_t kMessageCount{5u};
    inline constexpr std::size_t kDrainBudget{2u};
    inline constexpr std::uint32_t kServerBootstrapPayload{1u};
    inline constexpr std::uint32_t kClientBootstrapPayload{2u};
    inline constexpr std::uint32_t kIdlePayload{0xD1u};
    inline constexpr std::array<ManualTimeSource::Tick, 4> kWaitDuePlan{
        3u,
        7u,
        11u,
        15u,
    };
    inline constexpr std::array<ManualTimeSource::Tick, kCallCount>
        kReplyDuePlan{
            14u,
            18u,
            22u,
            26u,
        };
    inline constexpr ManualTimeSource::Tick kMissingReplyDue{30u};
    inline constexpr std::uint64_t kMissingToken{0xCFu};
    inline constexpr std::uint64_t kMissingSequence{0x6Fu};
    inline constexpr std::array<std::uint64_t, kCallCount> kExpectedValues{
        1u,
        21u,
        42u,
        0u,
    };
    inline constexpr std::array<kernel::TrapDisposition, kCallCount>
        kExpectedDispositions{
            kernel::TrapDisposition::handled,
            kernel::TrapDisposition::handled,
            kernel::TrapDisposition::handled,
            kernel::TrapDisposition::unsupported,
        };
    inline constexpr std::array<kernel::TrapError, kCallCount> kExpectedErrors{
        kernel::TrapError::none,
        kernel::TrapError::none,
        kernel::TrapError::none,
        kernel::TrapError::unsupported_service,
    };
    inline constexpr auto kInvalidSyscallId = static_cast<std::uint16_t>(99u);
    inline constexpr auto kInvalidSyscall =
        static_cast<kernel::TaskSyscallId>(kInvalidSyscallId);

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
        bool caller_valid{false};
        bool server_bootstrapped{false};
        bool client_bootstrapped{false};
        bool missing_request_sent{false};
        bool missing_timeout_seen{false};
        bool unexpected_client_timeout{false};
        bool idle_after_finish{false};
        std::uint32_t idle_runs{0};
        std::uint32_t server_runs{0};
        std::uint32_t client_runs{0};
        std::uint32_t server_timeouts{0};
        std::uint32_t replies_received{0};
        std::uint32_t calls_issued{0};
        std::size_t total_served{0};
        std::size_t wait_arm_index{0};
        std::size_t wait_arm_successes{0};
        std::array<bool, kCallCount> issue_ok{};
        std::array<bool, kCallCount> result_ok{};
        std::array<std::uint64_t, kCallCount> issued_tokens{};
        std::array<std::uint64_t, kCallCount> issued_sequences{};
        std::array<std::uint64_t, kCallCount> reply_sequences{};
        std::array<kernel::TrapResult, kCallCount> results{};
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
    using RuntimeTrace =
        kernel::RuntimeTraceBuffer<ManualTimeSource::Tick, 64>;
    using Mailbox = kernel::RuntimeMailbox<RunningScheduler, 8, 8, 4>;
    using TaskMessages = kernel::TaskMessageApi<Mailbox>;
    using FrameStore =
        kernel::TaskMessageSyscallFrameStore<MessageSyscallFrame, 8>;
    using FrameTrace = kernel::TaskSyscallFrameTraceBuffer<16>;
    using FramePort = kernel::TaskSyscallFramePort<MessageSyscallFrame>;
    using MessageFrameTrace =
        kernel::TaskMessageSyscallFrameBridgeTraceBuffer<8>;
    using MessageDispatchTrace = kernel::TaskMessageDispatchTraceBuffer<8>;
    using MessageTable = kernel::TaskMessageTable<1>;
    using MessageDispatcher = kernel::TaskMessageDispatcher<
        TaskMessages,
        MessageTable,
        MessageDispatchTrace>;
    using ServiceLoop = kernel::TaskMessageServiceLoop<MessageDispatcher>;
    using ServiceDrain = kernel::TaskMessageServiceDrain<ServiceLoop>;
    using PumpTrace = kernel::TaskMessageServicePumpTraceBuffer<16>;
    using ServicePump =
        kernel::TaskMessageServicePump<ServiceDrain, PumpTrace>;
    using SyscallDispatchTrace = kernel::TaskSyscallDispatchTraceBuffer<8>;
    using SyscallDispatcher = kernel::TaskSyscallDispatcher<
        FakeDispatchSurface,
        SyscallDispatchTrace>;
    using SyscallTableTrace = kernel::TaskSyscallTableTraceBuffer<8>;
    using SyscallTable = kernel::TaskSyscallTable<3, SyscallTableTrace>;
    using FrameBridge = kernel::TaskSyscallFrameBridge<
        SyscallTable,
        MessageSyscallFrame,
        FrameTrace>;
    using MessageFrameBridge = kernel::TaskMessageSyscallFrameBridge<
        FrameStore,
        FramePort,
        kernel::TaskMessageSyscallFrameResultAdapter<MessageSyscallFrame>,
        MessageFrameTrace>;
    using CallerAdapter =
        kernel::TaskMessageSyscallFrameCallAdapter<MessageSyscallFrame>;
    using ClientCaller =
        kernel::TaskMessageSyscallFrameCaller<TaskMessages,
                                              FrameStore,
                                              CallerAdapter>;

    inline constexpr auto kIdleId = Registry::id_of<IdleTask>();
    inline constexpr auto kServerId = Registry::id_of<ServerTask>();
    inline constexpr auto kClientId = Registry::id_of<ClientTask>();

    inline ServicePump* server_service_pump{nullptr};
    inline TaskMessages* client_messages{nullptr};
    inline ClientCaller* client_caller{nullptr};

    [[nodiscard]] constexpr bool same_text(const char* actual,
                                           std::string_view expected) noexcept
    {
        return actual != nullptr && std::string_view{actual} == expected;
    }

    [[nodiscard]] constexpr bool trap_result_matches(
        const kernel::TrapResult& result,
        kernel::TrapDisposition disposition,
        kernel::TrapError error,
        std::uint64_t value) noexcept
    {
        return result.disposition == disposition && result.error == error &&
               result.value == value;
    }

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

    [[nodiscard]] kernel::TaskSyscallRequest call_request(
        std::size_t index) noexcept
    {
        switch (index) {
        case 0u:
            return kernel::make_task_syscall_yield_request();
        case 1u:
            return kernel::make_task_syscall_sleep_until_request(
                kernel::TrapSleepUntilView<ManualTimeSource::Tick>{
                    .due = 21u,
                });
        case 2u:
            return kernel::make_task_syscall_capability_call_request(
                kernel::TrapCapabilityCallView{
                    .capability_id = 7u,
                    .operation = 2u,
                    .payload = 33u,
                });
        case 3u:
            return kernel::TaskSyscallRequest{
                .syscall = kInvalidSyscall,
                .arg0 = 0xAAu,
                .arg1 = 0xBBu,
            };
        default:
            return {};
        }
    }

    void issue_next_call(SharedState& shared) noexcept
    {
        if (client_caller == nullptr || shared.calls_issued >= kCallCount) {
            return;
        }

        const auto index = shared.calls_issued;
        const auto issued =
            client_caller->begin(call_request(index), kReplyDuePlan[index]);
        shared.issue_ok[index] = issued;
        if (issued) {
            shared.issued_tokens[index] = client_caller->state().token;
            shared.issued_sequences[index] = client_caller->state().sequence;
            ++shared.calls_issued;
        }

        std::printf(
            "[client] issue idx=%zu ok=%d token=%llu seq=%llu due=%llu\n",
            static_cast<std::size_t>(index),
            issued ? 1 : 0,
            issued ? static_cast<unsigned long long>(
                         shared.issued_tokens[index])
                   : 0ull,
            issued ? static_cast<unsigned long long>(
                         shared.issued_sequences[index])
                   : 0ull,
            static_cast<unsigned long long>(kReplyDuePlan[index]));
    }

    bool arm_missing_request(SharedState& shared) noexcept
    {
        if (client_messages == nullptr) {
            return false;
        }

        auto& mailbox = client_messages->mailbox();
        const auto armed = mailbox.wait_reply_until(kClientId, kMissingReplyDue);
        if (!armed) {
            return false;
        }

        const auto sent = mailbox.send(kClientId,
                                       kernel::task_message_syscall_frame_request_label,
                                       kMissingToken,
                                       kMissingSequence);
        if (!sent) {
            (void)mailbox.consume_reply_timeout(
                kClientId, mailbox.reply_timeout_event());
            return false;
        }

        shared.missing_request_sent = true;
        std::printf("[client] missing-send ok=1 token=%llu seq=%llu due=%llu\n",
                    static_cast<unsigned long long>(kMissingToken),
                    static_cast<unsigned long long>(kMissingSequence),
                    static_cast<unsigned long long>(kMissingReplyDue));
        return true;
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
        if (context.shared->replies_received == kCallCount &&
            context.shared->missing_timeout_seen &&
            context.shared->server_timeouts >= 1u) {
            context.shared->idle_after_finish = true;
        }

        std::printf("[idle] run=%u replies=%u timeout=%d now=%llu\n",
                    context.shared->idle_runs,
                    context.shared->replies_received,
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
            std::printf("[server] serve reason=%s served=%zu due=%llu armed=%d\n",
                        kernel::task_message_service_pump_reason_name(
                            result.reason),
                        result.drain.served,
                        static_cast<unsigned long long>(due),
                        result.wait_armed ? 1 : 0);
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

        if (context.shared == nullptr || client_messages == nullptr ||
            client_caller == nullptr) {
            return;
        }

        if (event.id == kernel::EventId::user0 &&
            kernel::payload_u32(event) == kClientBootstrapPayload) {
            ++context.shared->client_runs;
            issue_next_call(*context.shared);
            return;
        }

        if (client_caller->consume_reply_timeout(event)) {
            ++context.shared->client_runs;
            context.shared->unexpected_client_timeout = true;
            std::printf("[client] unexpected-timeout\n");
            return;
        }

        if (context.shared->missing_request_sent &&
            client_messages->mailbox().consume_reply_timeout(kClientId, event)) {
            ++context.shared->client_runs;
            context.shared->missing_timeout_seen = true;
            context.shared->missing_request_sent = false;
            std::printf("[client] missing-timeout now=%llu\n",
                        static_cast<unsigned long long>(ManualTimeSource::now()));
            return;
        }

        kernel::RuntimeMailboxReply reply{};
        kernel::TrapResult result{};
        if (!client_caller->receive_reply(reply, result)) {
            return;
        }

        ++context.shared->client_runs;
        const auto index = context.shared->replies_received;
        if (index >= kCallCount) {
            return;
        }

        context.shared->reply_sequences[index] = reply.sequence;
        context.shared->results[index] = result;
        context.shared->result_ok[index] = trap_result_matches(
            result,
            kExpectedDispositions[index],
            kExpectedErrors[index],
            kExpectedValues[index]);
        ++context.shared->replies_received;

        std::printf(
            "[client] reply idx=%zu seq=%llu value=%llu disp=%s err=%s ok=%d\n",
            static_cast<std::size_t>(index),
            static_cast<unsigned long long>(reply.sequence),
            static_cast<unsigned long long>(result.value),
            kernel::trap_disposition_name(result.disposition),
            kernel::trap_error_name(result.error),
            context.shared->result_ok[index] ? 1 : 0);

        if (context.shared->calls_issued < kCallCount) {
            issue_next_call(*context.shared);
            return;
        }

        if (context.shared->replies_received == kCallCount &&
            !context.shared->missing_request_sent &&
            !context.shared->missing_timeout_seen) {
            (void)arm_missing_request(*context.shared);
        }
    }

    struct MessageDispatchTraceStatus {
        bool yield_ok{false};
        bool sleep_ok{false};
        bool capability_ok{false};
        bool invalid_ok{false};
        bool missing_ok{false};

        [[nodiscard]] bool ok() const noexcept
        {
            return yield_ok && sleep_ok && capability_ok && invalid_ok &&
                   missing_ok;
        }
    };

    struct MessageFrameTraceStatus {
        bool yield_ok{false};
        bool sleep_ok{false};
        bool capability_ok{false};
        bool invalid_ok{false};
        bool missing_ok{false};

        [[nodiscard]] bool ok() const noexcept
        {
            return yield_ok && sleep_ok && capability_ok && invalid_ok &&
                   missing_ok;
        }
    };

    struct SyscallTableTraceStatus {
        bool yield_ok{false};
        bool sleep_ok{false};
        bool capability_ok{false};
        bool invalid_ok{false};

        [[nodiscard]] bool ok() const noexcept
        {
            return yield_ok && sleep_ok && capability_ok && invalid_ok;
        }
    };

    [[nodiscard]] MessageDispatchTraceStatus inspect_message_dispatch_trace(
        MessageDispatchTrace& trace,
        const std::array<std::uint64_t, kCallCount>& sequences) noexcept
    {
        MessageDispatchTraceStatus status{};

        for (std::size_t index = 0; index < trace.size(); ++index) {
            const auto* record = trace.at(index);
            if (record == nullptr) {
                continue;
            }

            std::printf(
                "[message-dispatch-trace] seq=%llu from=%llu label=%llu name=%s value=%llu req_seq=%llu accepted=%d matched=%d handler=%d handled=%d replied=%d reply=%llu\n",
                static_cast<unsigned long long>(record->sequence),
                static_cast<unsigned long long>(record->from.value),
                static_cast<unsigned long long>(record->label),
                record->label_name != nullptr ? record->label_name : "",
                static_cast<unsigned long long>(record->value),
                static_cast<unsigned long long>(record->request_sequence),
                record->accepted ? 1 : 0,
                record->matched ? 1 : 0,
                record->handler_valid ? 1 : 0,
                record->handled ? 1 : 0,
                record->replied ? 1 : 0,
                static_cast<unsigned long long>(record->reply_value));

            if (record->request_sequence == sequences[0]) {
                status.yield_ok = record->handled && record->replied &&
                                  record->reply_value == kExpectedValues[0];
            } else if (record->request_sequence == sequences[1]) {
                status.sleep_ok = record->handled && record->replied &&
                                  record->reply_value == kExpectedValues[1];
            } else if (record->request_sequence == sequences[2]) {
                status.capability_ok =
                    record->handled && record->replied &&
                    record->reply_value == kExpectedValues[2];
            } else if (record->request_sequence == sequences[3]) {
                status.invalid_ok = record->handled && record->replied &&
                                    record->reply_value == kExpectedValues[3];
            } else if (record->request_sequence == kMissingSequence) {
                status.missing_ok =
                    record->handler_valid && !record->handled &&
                    !record->replied && record->reply_value == 0u;
            }
        }

        return status;
    }

    [[nodiscard]] MessageFrameTraceStatus inspect_message_frame_trace(
        MessageFrameTrace& trace,
        const std::array<std::uint64_t, kCallCount>& tokens,
        const std::array<std::uint64_t, kCallCount>& sequences) noexcept
    {
        MessageFrameTraceStatus status{};

        for (std::size_t index = 0; index < trace.size(); ++index) {
            const auto* record = trace.at(index);
            if (record == nullptr) {
                continue;
            }

            std::printf(
                "[message-frame-trace] seq=%llu from=%llu label=%llu token=%llu req_seq=%llu slot=%d port=%d ready=%d disposition=%s error=%s reply=%llu handled=%d\n",
                static_cast<unsigned long long>(record->sequence),
                static_cast<unsigned long long>(record->from.value),
                static_cast<unsigned long long>(record->label),
                static_cast<unsigned long long>(record->token),
                static_cast<unsigned long long>(record->request_sequence),
                record->slot_found ? 1 : 0,
                record->port_valid ? 1 : 0,
                record->result_ready ? 1 : 0,
                kernel::trap_disposition_name(record->disposition),
                kernel::trap_error_name(record->error),
                static_cast<unsigned long long>(record->reply_value),
                record->handled ? 1 : 0);

            if (record->token == tokens[0]) {
                status.yield_ok =
                    record->request_sequence == sequences[0] &&
                    record->slot_found && record->result_ready &&
                    record->disposition == kExpectedDispositions[0] &&
                    record->error == kExpectedErrors[0] &&
                    record->reply_value == kExpectedValues[0] &&
                    record->handled;
            } else if (record->token == tokens[1]) {
                status.sleep_ok =
                    record->request_sequence == sequences[1] &&
                    record->slot_found && record->result_ready &&
                    record->disposition == kExpectedDispositions[1] &&
                    record->error == kExpectedErrors[1] &&
                    record->reply_value == kExpectedValues[1] &&
                    record->handled;
            } else if (record->token == tokens[2]) {
                status.capability_ok =
                    record->request_sequence == sequences[2] &&
                    record->slot_found && record->result_ready &&
                    record->disposition == kExpectedDispositions[2] &&
                    record->error == kExpectedErrors[2] &&
                    record->reply_value == kExpectedValues[2] &&
                    record->handled;
            } else if (record->token == tokens[3]) {
                status.invalid_ok =
                    record->request_sequence == sequences[3] &&
                    record->slot_found && record->result_ready &&
                    record->disposition == kExpectedDispositions[3] &&
                    record->error == kExpectedErrors[3] &&
                    record->reply_value == kExpectedValues[3] &&
                    record->handled;
            } else if (record->token == kMissingToken) {
                status.missing_ok = !record->slot_found &&
                                    record->port_valid &&
                                    !record->result_ready &&
                                    record->error ==
                                        kernel::TrapError::invalid_argument &&
                                    !record->handled;
            }
        }

        return status;
    }

    [[nodiscard]] SyscallTableTraceStatus inspect_syscall_table_trace(
        SyscallTableTrace& trace) noexcept
    {
        SyscallTableTraceStatus status{};

        for (std::size_t index = 0; index < trace.size(); ++index) {
            const auto* record = trace.at(index);
            if (record == nullptr) {
                continue;
            }

            std::printf(
                "[syscall-table-trace] seq=%llu syscall=%s slot=%u matched=%d handler=%d disposition=%s error=%s arg0=%llu arg1=%llu arg2=%llu value=%llu\n",
                static_cast<unsigned long long>(record->sequence),
                kernel::task_syscall_name(record->syscall),
                static_cast<unsigned int>(record->slot),
                record->matched ? 1 : 0,
                record->handler_valid ? 1 : 0,
                kernel::trap_disposition_name(record->disposition),
                kernel::trap_error_name(record->error),
                static_cast<unsigned long long>(record->arg0),
                static_cast<unsigned long long>(record->arg1),
                static_cast<unsigned long long>(record->arg2),
                static_cast<unsigned long long>(record->value));

            switch (record->sequence) {
            case 1u:
                status.yield_ok =
                    record->syscall == kernel::TaskSyscallId::yield &&
                    record->matched && record->handler_valid &&
                    record->value == 1u;
                break;
            case 2u:
                status.sleep_ok =
                    record->syscall == kernel::TaskSyscallId::sleep_until &&
                    record->matched && record->handler_valid &&
                    record->arg0 == 21u && record->value == 21u;
                break;
            case 3u:
                status.capability_ok =
                    record->syscall == kernel::TaskSyscallId::capability_call &&
                    record->matched && record->handler_valid &&
                    record->arg0 == 7u && record->arg1 == 2u &&
                    record->arg2 == 33u && record->value == 42u;
                break;
            case 4u:
                status.invalid_ok =
                    record->syscall == kInvalidSyscall &&
                    !record->matched && !record->handler_valid &&
                    record->disposition ==
                        kernel::TrapDisposition::unsupported &&
                    record->error ==
                        kernel::TrapError::unsupported_service &&
                    record->arg0 == 0xAAu && record->arg1 == 0xBBu &&
                    record->value == 0u;
                break;
            default:
                break;
            }
        }

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
    demo::RuntimeTrace runtime_trace{};
    demo::PumpTrace pump_trace{};
    demo::MessageDispatchTrace message_dispatch_trace{};
    demo::MessageFrameTrace message_frame_trace{};
    demo::SyscallDispatchTrace syscall_dispatch_trace{};
    demo::SyscallTableTrace syscall_table_trace{};
    demo::FrameTrace frame_trace{};
    demo::FrameStore frame_store{};
    demo::FakeDispatchSurfaceState dispatch_state{};
    demo::FrameAdapterState frame_adapter_state{};
    demo::FrameResultReadyState result_ready_state{};
    demo::CallerAdapterState caller_adapter_state{};

    kernel::RuntimeBridge runtime{
        running,
        demo::kIdleId,
        demo::make_idle_event(),
        &runtime_trace,
    };
    demo::Mailbox mailbox{running, demo::kServerId};
    demo::TaskMessages server_task_messages =
        kernel::make_task_message_api(mailbox);
    demo::TaskMessages client_task_messages =
        kernel::make_task_message_api(mailbox);
    auto syscall_dispatcher = kernel::make_task_syscall_dispatcher(
        demo::FakeDispatchSurface{
            .state = &dispatch_state,
        },
        &syscall_dispatch_trace);
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
        },
        &syscall_table_trace);
    demo::FrameBridge frame_bridge = kernel::make_task_syscall_frame_bridge(
        syscall_table,
        kernel::TaskSyscallFrameAdapter<demo::MessageSyscallFrame>{
            .ctx = &frame_adapter_state,
            .capture = &demo::capture_message_syscall_frame,
            .apply_result = &demo::apply_message_syscall_result,
        },
        &frame_trace);
    demo::MessageFrameBridge message_frame_bridge =
        kernel::make_task_message_syscall_frame_bridge(
            frame_store,
            kernel::make_task_syscall_frame_port(frame_bridge),
            kernel::TaskMessageSyscallFrameResultAdapter<
                demo::MessageSyscallFrame>{
                .ctx = &result_ready_state,
                .result_ready = &demo::message_syscall_frame_result_ready,
            },
            &message_frame_trace);
    auto message_table = kernel::make_task_message_table(
        std::array<kernel::TaskMessageHandlerEntry, 1>{
            kernel::task_message_handler_entry(
                kernel::task_message_syscall_frame_request_label,
                kernel::task_message_syscall_frame_request_label_name(),
                kernel::make_task_message_handler(message_frame_bridge)),
        });
    demo::MessageDispatcher message_dispatcher =
        kernel::make_task_message_dispatcher(
            server_task_messages, message_table, &message_dispatch_trace);
    demo::ServiceLoop service_loop =
        kernel::make_task_message_service_loop(message_dispatcher);
    demo::ServiceDrain service_drain =
        kernel::make_task_message_service_drain(service_loop);
    demo::ServicePump service_pump =
        kernel::make_task_message_service_pump(
            service_drain,
            demo::make_server_bootstrap_event(),
            &pump_trace);
    demo::ClientCaller client_caller =
        kernel::make_task_message_syscall_frame_caller(
            client_task_messages,
            frame_store,
            demo::CallerAdapter{
                .ctx = &caller_adapter_state,
                .make_frame = &demo::make_message_syscall_call_frame,
                .capture_result = &demo::capture_message_syscall_call_result,
            });
    client_caller.bind_cursors(0xC1u, 0x61u);

    demo::server_service_pump = &service_pump;
    demo::client_messages = &client_task_messages;
    demo::client_caller = &client_caller;

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
    shared.caller_valid = client_caller.valid();
    shared.server_bootstrapped =
        runtime.bootstrap_worker(
            demo::kServerId,
            demo::make_server_bootstrap_event());

    std::size_t loops = 0;
    while ((!shared.missing_timeout_seen ||
            shared.replies_received < demo::kCallCount) &&
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

    const auto dispatch_status = demo::inspect_message_dispatch_trace(
        message_dispatch_trace, shared.issued_sequences);
    const auto frame_status = demo::inspect_message_frame_trace(
        message_frame_trace, shared.issued_tokens, shared.issued_sequences);
    const auto table_status =
        demo::inspect_syscall_table_trace(syscall_table_trace);

    const bool sequences_ok =
        shared.reply_sequences == shared.issued_sequences;
    const bool results_ok =
        shared.result_ok ==
        std::array<bool, demo::kCallCount>{
            true,
            true,
            true,
            true,
        };
    const bool caller_state_ok =
        caller_adapter_state.make_calls == demo::kCallCount &&
        caller_adapter_state.capture_result_calls == demo::kCallCount &&
        caller_adapter_state.last_syscall == demo::kInvalidSyscall &&
        caller_adapter_state.last_arg0 == 0xAAu &&
        caller_adapter_state.last_arg1 == 0xBBu &&
        caller_adapter_state.last_arg2 == 0u &&
        caller_adapter_state.last_reply_value == 0u &&
        caller_adapter_state.last_disposition ==
            kernel::TrapDisposition::unsupported &&
        caller_adapter_state.last_error ==
            kernel::TrapError::unsupported_service &&
        caller_adapter_state.last_writeback_seen;
    const bool surface_ok =
        dispatch_state.yield_calls == 1u &&
        dispatch_state.sleep_calls == 1u &&
        dispatch_state.capability_calls == 1u &&
        dispatch_state.last_due == 21u &&
        dispatch_state.last_capability_id == 7u &&
        dispatch_state.last_capability_operation == 2u &&
        dispatch_state.last_capability_payload == 33u &&
        frame_adapter_state.capture_calls == demo::kCallCount &&
        frame_adapter_state.writeback_calls == demo::kCallCount &&
        result_ready_state.ready_calls == demo::kCallCount &&
        result_ready_state.last_value == 0u &&
        result_ready_state.last_disposition ==
            kernel::TrapDisposition::unsupported &&
        result_ready_state.last_error ==
            kernel::TrapError::unsupported_service &&
        result_ready_state.last_writeback_seen;
    const bool ok =
        shared.mailbox_valid && shared.dispatcher_valid &&
        shared.frame_store_valid && shared.frame_bridge_valid &&
        shared.message_frame_bridge_valid && shared.service_loop_valid &&
        shared.service_drain_valid && shared.service_pump_valid &&
        shared.caller_valid && shared.server_bootstrapped &&
        shared.client_bootstrapped &&
        shared.issue_ok ==
            std::array<bool, demo::kCallCount>{
                true,
                true,
                true,
                true,
            } &&
        shared.calls_issued == demo::kCallCount &&
        shared.replies_received == demo::kCallCount &&
        shared.missing_request_sent == false &&
        shared.missing_timeout_seen &&
        !shared.unexpected_client_timeout &&
        shared.total_served == demo::kMessageCount &&
        shared.server_timeouts >= 1u &&
        shared.wait_arm_successes >= 1u &&
        shared.idle_runs >= 1u &&
        shared.server_runs >= 5u &&
        shared.client_runs >= 6u &&
        sequences_ok && results_ok && caller_state_ok && surface_ok &&
        dispatch_status.ok() && frame_status.ok() && table_status.ok() &&
        message_dispatch_trace.size() == demo::kMessageCount &&
        message_frame_trace.size() == demo::kMessageCount &&
        frame_trace.size() == 12u &&
        syscall_dispatch_trace.size() == 3u &&
        syscall_table_trace.size() == 4u &&
        frame_store.pending() == 0u &&
        mailbox.pending_requests() == 0u &&
        mailbox.pending_replies() == 0u &&
        mailbox.reply_waiters() == 0u &&
        mailbox.receive_waiting() &&
        !client_caller.busy() &&
        loops < 96u;

    std::printf(
        "[runtime-task-message-syscall-frame-caller-demo] ok=%d valid=%d caller=%d server_boot=%d client_boot=%d issued=%u replies=%u missing_timeout=%d served=%zu timeouts=%u idle=%d pending_frames=%zu pending_req=%zu pending_reply=%zu reply_waiters=%zu waiting=%d loops=%llu\n",
        ok ? 1 : 0,
        shared.mailbox_valid ? 1 : 0,
        shared.caller_valid ? 1 : 0,
        shared.server_bootstrapped ? 1 : 0,
        shared.client_bootstrapped ? 1 : 0,
        shared.calls_issued,
        shared.replies_received,
        shared.missing_timeout_seen ? 1 : 0,
        shared.total_served,
        shared.server_timeouts,
        shared.idle_after_finish ? 1 : 0,
        frame_store.pending(),
        mailbox.pending_requests(),
        mailbox.pending_replies(),
        mailbox.reply_waiters(),
        mailbox.receive_waiting() ? 1 : 0,
        static_cast<unsigned long long>(loops));
    std::printf(
        "[runtime-task-message-syscall-frame-caller-values] token0=%llu token1=%llu token2=%llu token3=%llu seq0=%llu seq1=%llu seq2=%llu seq3=%llu value0=%llu value1=%llu value2=%llu value3=%llu cap_id=%llu cap_op=%llu cap_payload=%llu invalid_err=%s\n",
        static_cast<unsigned long long>(shared.issued_tokens[0]),
        static_cast<unsigned long long>(shared.issued_tokens[1]),
        static_cast<unsigned long long>(shared.issued_tokens[2]),
        static_cast<unsigned long long>(shared.issued_tokens[3]),
        static_cast<unsigned long long>(shared.reply_sequences[0]),
        static_cast<unsigned long long>(shared.reply_sequences[1]),
        static_cast<unsigned long long>(shared.reply_sequences[2]),
        static_cast<unsigned long long>(shared.reply_sequences[3]),
        static_cast<unsigned long long>(shared.results[0].value),
        static_cast<unsigned long long>(shared.results[1].value),
        static_cast<unsigned long long>(shared.results[2].value),
        static_cast<unsigned long long>(shared.results[3].value),
        static_cast<unsigned long long>(dispatch_state.last_capability_id),
        static_cast<unsigned long long>(dispatch_state.last_capability_operation),
        static_cast<unsigned long long>(dispatch_state.last_capability_payload),
        kernel::trap_error_name(shared.results[3].error));
    return ok ? 0 : 1;
}
