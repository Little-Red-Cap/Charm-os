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

    inline constexpr std::size_t kPublishedFrameCount{4u};
    inline constexpr std::size_t kMessageCount{5u};
    inline constexpr std::size_t kDrainBudget{2u};
    inline constexpr std::uint32_t kServerBootstrapPayload{1u};
    inline constexpr std::uint32_t kClientBootstrapPayload{2u};
    inline constexpr std::uint32_t kIdlePayload{0xC7u};
    inline constexpr std::array<ManualTimeSource::Tick, 4> kWaitDuePlan{
        3u,
        7u,
        11u,
        15u,
    };
    inline constexpr ManualTimeSource::Tick kInitialReplyDue{14u};
    inline constexpr ManualTimeSource::Tick kMissingReplyDue{18u};
    inline constexpr std::array<std::uint64_t, kPublishedFrameCount>
        kFrameTokens{
            0xA1u,
            0xA2u,
            0xA3u,
            0xA4u,
        };
    inline constexpr std::uint64_t kMissingToken{0xAFu};
    inline constexpr std::array<std::uint64_t, kMessageCount> kRequestTokens{
        kFrameTokens[0],
        kFrameTokens[1],
        kFrameTokens[2],
        kFrameTokens[3],
        kMissingToken,
    };
    inline constexpr std::array<std::uint64_t, kMessageCount> kRequestSequences{
        0x51u,
        0x52u,
        0x53u,
        0x54u,
        0x55u,
    };
    inline constexpr std::array<std::uint64_t, kPublishedFrameCount>
        kReplyValues{
            1u,
            21u,
            42u,
            0u,
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
        bool server_bootstrapped{false};
        bool client_bootstrapped{false};
        bool client_publish_all{false};
        bool client_send_all{false};
        bool client_reply_wait_armed{false};
        bool client_missing_wait_armed{false};
        bool missing_timeout_seen{false};
        bool idle_after_finish{false};
        bool hold_ready_seen{false};
        bool stale_ready_seen{false};
        std::uint32_t idle_runs{0};
        std::uint32_t server_runs{0};
        std::uint32_t client_runs{0};
        std::uint32_t server_timeouts{0};
        std::uint32_t replies_received{0};
        std::size_t total_served{0};
        std::size_t wait_arm_index{0};
        std::size_t wait_arm_successes{0};
        std::array<std::uint64_t, kPublishedFrameCount> reply_sequences{};
        std::array<std::uint64_t, kPublishedFrameCount> reply_values{};
        std::array<std::uint64_t, kPublishedFrameCount> taken_tokens{};
        std::array<bool, kPublishedFrameCount> taken_ok{};
        std::array<MessageSyscallFrame, kPublishedFrameCount> completed_frames{};
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
                     kernel::ThreadControl& control,
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

    inline constexpr auto kIdleId = Registry::id_of<IdleTask>();
    inline constexpr auto kServerId = Registry::id_of<ServerTask>();
    inline constexpr auto kClientId = Registry::id_of<ClientTask>();

    inline ServicePump* server_service_pump{nullptr};
    inline TaskMessages* client_messages{nullptr};
    inline FrameStore* frame_store{nullptr};

    [[nodiscard]] constexpr bool same_text(const char* actual,
                                           std::string_view expected) noexcept
    {
        return actual != nullptr && std::string_view{actual} == expected;
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

    [[nodiscard]] constexpr MessageSyscallFrame make_yield_frame() noexcept
    {
        return MessageSyscallFrame{
            .syscall_id = static_cast<std::uint16_t>(kernel::TaskSyscallId::yield),
        };
    }

    [[nodiscard]] constexpr MessageSyscallFrame make_sleep_frame() noexcept
    {
        return MessageSyscallFrame{
            .syscall_id =
                static_cast<std::uint16_t>(kernel::TaskSyscallId::sleep_until),
            .arg0 = 21u,
        };
    }

    [[nodiscard]] constexpr MessageSyscallFrame make_capability_frame() noexcept
    {
        return MessageSyscallFrame{
            .syscall_id =
                static_cast<std::uint16_t>(kernel::TaskSyscallId::capability_call),
            .arg0 = 7u,
            .arg1 = 2u,
            .arg2 = 33u,
        };
    }

    [[nodiscard]] constexpr MessageSyscallFrame make_invalid_frame() noexcept
    {
        return MessageSyscallFrame{
            .syscall_id = kInvalidSyscallId,
            .arg0 = 0xAAu,
            .arg1 = 0xBBu,
        };
    }

    [[nodiscard]] constexpr std::size_t completed_index_from_sequence(
        std::uint64_t sequence) noexcept
    {
        for (std::size_t index = 0; index < kPublishedFrameCount; ++index) {
            if (kRequestSequences[index] == sequence) {
                return index;
            }
        }

        return kPublishedFrameCount;
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
        if (context.shared->replies_received == kPublishedFrameCount &&
            context.shared->missing_timeout_seen &&
            context.shared->server_timeouts >= 2u) {
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
            if (!result.progressed && !result.wait_armed) {
                context.shared->stale_ready_seen = true;
            }
            std::printf(
                "[server] queue-empty served=%zu due=%llu armed=%d progressed=%d\n",
                result.drain.served,
                static_cast<unsigned long long>(due),
                result.wait_armed ? 1 : 0,
                result.progressed ? 1 : 0);
            return;
        case kernel::TaskMessageServicePumpReason::budget_reached:
            context.shared->hold_ready_seen = result.hold_ready;
            std::printf("[server] hold served=%zu due=%llu hold=%d\n",
                        result.drain.served,
                        static_cast<unsigned long long>(due),
                        result.hold_ready ? 1 : 0);
            return;
        case kernel::TaskMessageServicePumpReason::none:
            break;
        }
    }

    void client_step(ClientContext& context,
                     kernel::ThreadControl& control,
                     kernel::Event event)
    {
        if (event.id == kernel::EventId::init) {
            std::printf("[client] init\n");
            return;
        }

        if (context.shared == nullptr || client_messages == nullptr ||
            frame_store == nullptr || !client_messages->valid()) {
            return;
        }

        if (event.id == kernel::EventId::terminate) {
            control.finish();
            return;
        }

        if (event.id == kernel::EventId::user0 &&
            kernel::payload_u32(event) == kClientBootstrapPayload) {
            ++context.shared->client_runs;
            const bool publish_yield =
                frame_store->publish(kClientId, kFrameTokens[0], make_yield_frame());
            const bool publish_sleep =
                frame_store->publish(kClientId, kFrameTokens[1], make_sleep_frame());
            const bool publish_capability = frame_store->publish(
                kClientId, kFrameTokens[2], make_capability_frame());
            const bool publish_invalid = frame_store->publish(
                kClientId, kFrameTokens[3], make_invalid_frame());
            context.shared->client_publish_all =
                publish_yield && publish_sleep && publish_capability &&
                publish_invalid;

            bool sent_all = true;
            for (std::size_t index = 0; index < kMessageCount; ++index) {
                sent_all = client_messages->send(
                               kernel::task_message_syscall_frame_request_label,
                               kRequestTokens[index],
                               kRequestSequences[index]) &&
                           sent_all;
            }
            context.shared->client_send_all = sent_all;
            context.shared->client_reply_wait_armed =
                client_messages->wait_reply_until(kInitialReplyDue);
            std::printf(
                "[client] publish=%d send=%d wait=%d count=%zu\n",
                context.shared->client_publish_all ? 1 : 0,
                sent_all ? 1 : 0,
                context.shared->client_reply_wait_armed ? 1 : 0,
                kMessageCount);
            return;
        }

        if (client_messages->consume_reply_timeout(event)) {
            ++context.shared->client_runs;
            if (context.shared->replies_received == kPublishedFrameCount) {
                context.shared->missing_timeout_seen = true;
                context.shared->idle_after_finish = true;
                std::printf("[client] missing-timeout now=%llu\n",
                            static_cast<unsigned long long>(
                                ManualTimeSource::now()));
            } else {
                std::printf("[client] unexpected-timeout now=%llu\n",
                            static_cast<unsigned long long>(
                                ManualTimeSource::now()));
            }
            control.finish();
            return;
        }

        kernel::RuntimeMailboxReply reply{};
        if (!client_messages->receive_reply(reply)) {
            return;
        }

        ++context.shared->client_runs;
        const auto index = completed_index_from_sequence(reply.sequence);
        if (index < kPublishedFrameCount) {
            context.shared->reply_sequences[index] = reply.sequence;
            context.shared->reply_values[index] = reply.value;
            context.shared->taken_tokens[index] = kFrameTokens[index];
            context.shared->taken_ok[index] = frame_store->take(
                kClientId,
                kFrameTokens[index],
                context.shared->completed_frames[index]);
        }
        ++context.shared->replies_received;

        std::printf(
            "[client] reply seq=%llu value=%llu count=%u token=%llu take=%d\n",
            static_cast<unsigned long long>(reply.sequence),
            static_cast<unsigned long long>(reply.value),
            context.shared->replies_received,
            index < kPublishedFrameCount
                ? static_cast<unsigned long long>(kFrameTokens[index])
                : 0ull,
            (index < kPublishedFrameCount && context.shared->taken_ok[index]) ? 1
                                                                              : 0);

        if (context.shared->replies_received == kPublishedFrameCount &&
            !context.shared->client_missing_wait_armed) {
            context.shared->client_missing_wait_armed =
                client_messages->wait_reply_until(kMissingReplyDue);
            std::printf("[client] wait-missing=%d due=%llu\n",
                        context.shared->client_missing_wait_armed ? 1 : 0,
                        static_cast<unsigned long long>(kMissingReplyDue));
        }
    }

    struct RuntimeTraceStatus {
        bool server_bootstrap{false};
        bool client_bootstrap{false};
        bool tick{false};
        bool idle{false};

        [[nodiscard]] bool ok() const noexcept
        {
            return server_bootstrap && client_bootstrap && tick && idle;
        }
    };

    struct PumpTraceStatus {
        bool bootstrap{false};
        bool timeout1{false};
        std::size_t hold_count{0};
        bool queue_rearm{false};
        bool stale_fail{false};
        bool timeout2{false};

        [[nodiscard]] bool ok() const noexcept
        {
            return bootstrap && timeout1 && hold_count == 2u && queue_rearm &&
                   stale_fail && timeout2;
        }
    };

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

    struct FrameTraceStatus {
        bool yield_ok{false};
        bool sleep_ok{false};
        bool capability_ok{false};
        bool invalid_ok{false};

        [[nodiscard]] bool ok() const noexcept
        {
            return yield_ok && sleep_ok && capability_ok && invalid_ok;
        }
    };

    struct SyscallDispatchTraceStatus {
        bool yield_ok{false};
        bool sleep_ok{false};
        bool capability_ok{false};

        [[nodiscard]] bool ok() const noexcept
        {
            return yield_ok && sleep_ok && capability_ok;
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

    [[nodiscard]] RuntimeTraceStatus inspect_runtime_trace(
        RuntimeTrace& trace) noexcept
    {
        RuntimeTraceStatus status{};

        for (std::size_t index = 0; index < trace.size(); ++index) {
            const auto* record = trace.at(index);
            if (record == nullptr) {
                continue;
            }

            std::printf(
                "[runtime-trace] t=%llu kind=%s task=%s%llu event=%u value=%llu ok=%d\n",
                static_cast<unsigned long long>(record->stamp),
                kernel::runtime_trace_kind_name(record->kind),
                record->task_valid ? "" : "-",
                static_cast<unsigned long long>(record->task.value),
                static_cast<unsigned int>(record->event_id),
                static_cast<unsigned long long>(record->value),
                record->ok ? 1 : 0);

            switch (record->kind) {
            case kernel::RuntimeTraceKind::worker_bootstrap:
                status.server_bootstrap =
                    status.server_bootstrap ||
                    (record->task_valid && record->task == kServerId &&
                     record->event_id == kernel::EventId::user0 &&
                     record->value == kServerBootstrapPayload && record->ok);
                status.client_bootstrap =
                    status.client_bootstrap ||
                    (record->task_valid && record->task == kClientId &&
                     record->event_id == kernel::EventId::user0 &&
                     record->value == kClientBootstrapPayload && record->ok);
                break;
            case kernel::RuntimeTraceKind::tick:
                status.tick =
                    status.tick ||
                    (!record->task_valid &&
                     record->event_id == kernel::EventId::tick &&
                     record->value >= 1u && record->ok);
                break;
            case kernel::RuntimeTraceKind::idle_bootstrap:
                status.idle =
                    status.idle ||
                    (record->task_valid && record->task == kIdleId &&
                     record->event_id == kernel::EventId::user0 &&
                     record->value == kIdlePayload && record->ok);
                break;
            case kernel::RuntimeTraceKind::isr_defer:
            case kernel::RuntimeTraceKind::yield:
            case kernel::RuntimeTraceKind::sleep:
                break;
            }
        }

        return status;
    }

    [[nodiscard]] PumpTraceStatus inspect_pump_trace(PumpTrace& trace) noexcept
    {
        PumpTraceStatus status{};

        for (std::size_t index = 0; index < trace.size(); ++index) {
            const auto* record = trace.at(index);
            if (record == nullptr) {
                continue;
            }

            std::printf(
                "[pump-trace] seq=%llu kind=%s reason=%s event=%u value=%llu due=%llu budget=%zu served=%zu ok=%d\n",
                static_cast<unsigned long long>(record->sequence),
                kernel::task_message_service_pump_trace_kind_name(record->kind),
                kernel::task_message_service_pump_reason_name(record->reason),
                static_cast<unsigned int>(record->event_id),
                static_cast<unsigned long long>(record->value),
                static_cast<unsigned long long>(record->due),
                record->budget,
                record->served,
                record->ok ? 1 : 0);

            status.bootstrap =
                status.bootstrap ||
                (record->sequence == 1u &&
                 record->kind ==
                     kernel::TaskMessageServicePumpTraceKind::bootstrap &&
                 record->reason ==
                     kernel::TaskMessageServicePumpReason::bootstrap &&
                 record->event_id == kernel::EventId::user0 &&
                 record->value == kServerBootstrapPayload &&
                 record->due == kWaitDuePlan[0] &&
                 record->budget == kDrainBudget && record->served == 0u &&
                 record->ok);
            status.timeout1 =
                status.timeout1 ||
                (record->sequence == 2u &&
                 record->kind == kernel::TaskMessageServicePumpTraceKind::rearm &&
                 record->reason ==
                     kernel::TaskMessageServicePumpReason::timeout &&
                 record->event_id == kernel::EventId::sync &&
                 record->value == kernel::runtime_mailbox_receive_timeout_code &&
                 record->due == kWaitDuePlan[1] &&
                 record->budget == kDrainBudget && record->served == 0u &&
                 record->ok);
            if (record->kind == kernel::TaskMessageServicePumpTraceKind::hold &&
                record->reason ==
                    kernel::TaskMessageServicePumpReason::budget_reached &&
                record->event_id == kernel::EventId::message &&
                record->value == kernel::runtime_mailbox_receive_ready_code &&
                record->due == kWaitDuePlan[2] &&
                record->budget == kDrainBudget &&
                record->served == kDrainBudget && record->ok) {
                ++status.hold_count;
            }
            status.queue_rearm =
                status.queue_rearm ||
                (record->kind == kernel::TaskMessageServicePumpTraceKind::rearm &&
                 record->reason ==
                     kernel::TaskMessageServicePumpReason::queue_empty &&
                 record->event_id == kernel::EventId::message &&
                 record->value == kernel::runtime_mailbox_receive_ready_code &&
                 record->due == kWaitDuePlan[2] &&
                 record->budget == kDrainBudget && record->served == 1u &&
                 record->ok);
            status.stale_fail =
                status.stale_fail ||
                (record->kind == kernel::TaskMessageServicePumpTraceKind::rearm &&
                 record->reason ==
                     kernel::TaskMessageServicePumpReason::queue_empty &&
                 record->event_id == kernel::EventId::message &&
                 record->value == kernel::runtime_mailbox_receive_ready_code &&
                 record->due == kWaitDuePlan[3] &&
                 record->budget == kDrainBudget && record->served == 0u &&
                 !record->ok);
            status.timeout2 =
                status.timeout2 ||
                (record->kind == kernel::TaskMessageServicePumpTraceKind::rearm &&
                 record->reason ==
                     kernel::TaskMessageServicePumpReason::timeout &&
                 record->event_id == kernel::EventId::sync &&
                 record->value == kernel::runtime_mailbox_receive_timeout_code &&
                 record->due == kWaitDuePlan[3] &&
                 record->budget == kDrainBudget && record->served == 0u &&
                 record->ok);
        }

        return status;
    }

    [[nodiscard]] MessageDispatchTraceStatus inspect_message_dispatch_trace(
        MessageDispatchTrace& trace) noexcept
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
                record->label_name,
                static_cast<unsigned long long>(record->value),
                static_cast<unsigned long long>(record->request_sequence),
                record->accepted ? 1 : 0,
                record->matched ? 1 : 0,
                record->handler_valid ? 1 : 0,
                record->handled ? 1 : 0,
                record->replied ? 1 : 0,
                static_cast<unsigned long long>(record->reply_value));

            switch (record->sequence) {
            case 1u:
                status.yield_ok =
                    record->from == kClientId &&
                    record->label ==
                        kernel::task_message_syscall_frame_request_label &&
                    same_text(
                        record->label_name,
                        kernel::task_message_syscall_frame_request_label_name()) &&
                    record->value == kRequestTokens[0] &&
                    record->request_sequence == kRequestSequences[0] &&
                    record->accepted && record->matched &&
                    record->handler_valid && record->handled &&
                    record->replied && record->reply_value == kReplyValues[0];
                break;
            case 2u:
                status.sleep_ok =
                    record->value == kRequestTokens[1] &&
                    record->request_sequence == kRequestSequences[1] &&
                    record->accepted && record->matched &&
                    record->handler_valid && record->handled &&
                    record->replied && record->reply_value == kReplyValues[1];
                break;
            case 3u:
                status.capability_ok =
                    record->value == kRequestTokens[2] &&
                    record->request_sequence == kRequestSequences[2] &&
                    record->accepted && record->matched &&
                    record->handler_valid && record->handled &&
                    record->replied && record->reply_value == kReplyValues[2];
                break;
            case 4u:
                status.invalid_ok =
                    record->value == kRequestTokens[3] &&
                    record->request_sequence == kRequestSequences[3] &&
                    record->accepted && record->matched &&
                    record->handler_valid && record->handled &&
                    record->replied && record->reply_value == kReplyValues[3];
                break;
            case 5u:
                status.missing_ok =
                    record->value == kRequestTokens[4] &&
                    record->request_sequence == kRequestSequences[4] &&
                    record->accepted && record->matched &&
                    record->handler_valid && !record->handled &&
                    !record->replied && record->reply_value == 0u;
                break;
            default:
                break;
            }
        }

        return status;
    }

    [[nodiscard]] MessageFrameTraceStatus inspect_message_frame_trace(
        MessageFrameTrace& trace) noexcept
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

            switch (record->sequence) {
            case 1u:
                status.yield_ok =
                    record->from == kClientId &&
                    record->label ==
                        kernel::task_message_syscall_frame_request_label &&
                    record->token == kRequestTokens[0] &&
                    record->request_sequence == kRequestSequences[0] &&
                    record->slot_found && record->port_valid &&
                    record->result_ready &&
                    record->disposition == kernel::TrapDisposition::handled &&
                    record->error == kernel::TrapError::none &&
                    record->reply_value == kReplyValues[0] && record->handled;
                break;
            case 2u:
                status.sleep_ok =
                    record->token == kRequestTokens[1] &&
                    record->request_sequence == kRequestSequences[1] &&
                    record->slot_found && record->port_valid &&
                    record->result_ready &&
                    record->disposition == kernel::TrapDisposition::handled &&
                    record->error == kernel::TrapError::none &&
                    record->reply_value == kReplyValues[1] && record->handled;
                break;
            case 3u:
                status.capability_ok =
                    record->token == kRequestTokens[2] &&
                    record->request_sequence == kRequestSequences[2] &&
                    record->slot_found && record->port_valid &&
                    record->result_ready &&
                    record->disposition == kernel::TrapDisposition::handled &&
                    record->error == kernel::TrapError::none &&
                    record->reply_value == kReplyValues[2] && record->handled;
                break;
            case 4u:
                status.invalid_ok =
                    record->token == kRequestTokens[3] &&
                    record->request_sequence == kRequestSequences[3] &&
                    record->slot_found && record->port_valid &&
                    record->result_ready &&
                    record->disposition ==
                        kernel::TrapDisposition::unsupported &&
                    record->error == kernel::TrapError::unsupported_service &&
                    record->reply_value == kReplyValues[3] && record->handled;
                break;
            case 5u:
                status.missing_ok =
                    record->token == kRequestTokens[4] &&
                    record->request_sequence == kRequestSequences[4] &&
                    !record->slot_found && record->port_valid &&
                    !record->result_ready &&
                    record->disposition == kernel::TrapDisposition::rejected &&
                    record->error == kernel::TrapError::invalid_argument &&
                    record->reply_value == 0u && !record->handled;
                break;
            default:
                break;
            }
        }

        return status;
    }

    [[nodiscard]] FrameTraceStatus inspect_frame_trace(FrameTrace& trace) noexcept
    {
        FrameTraceStatus status{};

        for (std::size_t index = 0; index < trace.size(); ++index) {
            const auto* record = trace.at(index);
            if (record == nullptr) {
                continue;
            }

            std::printf(
                "[syscall-frame-trace] seq=%llu stage=%s syscall=%s disposition=%s error=%s arg0=%llu arg1=%llu arg2=%llu value=%llu ok=%d\n",
                static_cast<unsigned long long>(record->sequence),
                kernel::task_syscall_frame_stage_name(record->stage),
                kernel::task_syscall_name(record->syscall),
                kernel::trap_disposition_name(record->disposition),
                kernel::trap_error_name(record->error),
                static_cast<unsigned long long>(record->arg0),
                static_cast<unsigned long long>(record->arg1),
                static_cast<unsigned long long>(record->arg2),
                static_cast<unsigned long long>(record->value),
                record->ok ? 1 : 0);

            switch (record->sequence) {
            case 1u:
                status.yield_ok =
                    record->stage == kernel::TaskSyscallFrameStage::decode &&
                    record->syscall == kernel::TaskSyscallId::yield &&
                    record->ok;
                break;
            case 5u:
                status.sleep_ok =
                    record->stage == kernel::TaskSyscallFrameStage::dispatch &&
                    record->syscall == kernel::TaskSyscallId::sleep_until &&
                    record->arg0 == 21u &&
                    record->disposition == kernel::TrapDisposition::handled &&
                    record->error == kernel::TrapError::none &&
                    record->value == 21u && record->ok;
                break;
            case 8u:
                status.capability_ok =
                    record->stage == kernel::TaskSyscallFrameStage::dispatch &&
                    record->syscall ==
                        kernel::TaskSyscallId::capability_call &&
                    record->arg0 == 7u && record->arg1 == 2u &&
                    record->arg2 == 33u &&
                    record->disposition == kernel::TrapDisposition::handled &&
                    record->error == kernel::TrapError::none &&
                    record->value == 42u && record->ok;
                break;
            case 12u:
                status.invalid_ok =
                    record->stage ==
                        kernel::TaskSyscallFrameStage::writeback &&
                    record->syscall == kInvalidSyscall &&
                    record->arg0 == 0xAAu && record->arg1 == 0xBBu &&
                    record->disposition ==
                        kernel::TrapDisposition::unsupported &&
                    record->error == kernel::TrapError::unsupported_service &&
                    record->value == 0u && record->ok;
                break;
            default:
                break;
            }
        }

        return status;
    }

    [[nodiscard]] SyscallDispatchTraceStatus inspect_syscall_dispatch_trace(
        SyscallDispatchTrace& trace) noexcept
    {
        SyscallDispatchTraceStatus status{};

        for (std::size_t index = 0; index < trace.size(); ++index) {
            const auto* record = trace.at(index);
            if (record == nullptr) {
                continue;
            }

            std::printf(
                "[syscall-dispatch-trace] seq=%llu syscall=%s disposition=%s error=%s arg0=%llu arg1=%llu arg2=%llu value=%llu\n",
                static_cast<unsigned long long>(record->sequence),
                kernel::task_syscall_name(record->syscall),
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
                    record->disposition == kernel::TrapDisposition::handled &&
                    record->error == kernel::TrapError::none &&
                    record->value == 1u;
                break;
            case 2u:
                status.sleep_ok =
                    record->syscall == kernel::TaskSyscallId::sleep_until &&
                    record->arg0 == 21u &&
                    record->disposition == kernel::TrapDisposition::handled &&
                    record->error == kernel::TrapError::none &&
                    record->value == 21u;
                break;
            case 3u:
                status.capability_ok =
                    record->syscall ==
                        kernel::TaskSyscallId::capability_call &&
                    record->arg0 == 7u && record->arg1 == 2u &&
                    record->arg2 == 33u &&
                    record->disposition == kernel::TrapDisposition::handled &&
                    record->error == kernel::TrapError::none &&
                    record->value == 42u;
                break;
            default:
                break;
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
                    record->slot == 0u && record->matched &&
                    record->handler_valid &&
                    record->disposition == kernel::TrapDisposition::handled &&
                    record->error == kernel::TrapError::none &&
                    record->value == 1u;
                break;
            case 2u:
                status.sleep_ok =
                    record->syscall == kernel::TaskSyscallId::sleep_until &&
                    record->slot == 1u && record->matched &&
                    record->handler_valid && record->arg0 == 21u &&
                    record->disposition == kernel::TrapDisposition::handled &&
                    record->error == kernel::TrapError::none &&
                    record->value == 21u;
                break;
            case 3u:
                status.capability_ok =
                    record->syscall ==
                        kernel::TaskSyscallId::capability_call &&
                    record->slot == 2u && record->matched &&
                    record->handler_valid && record->arg0 == 7u &&
                    record->arg1 == 2u && record->arg2 == 33u &&
                    record->disposition == kernel::TrapDisposition::handled &&
                    record->error == kernel::TrapError::none &&
                    record->value == 42u;
                break;
            case 4u:
                status.invalid_ok =
                    record->syscall == kInvalidSyscall &&
                    record->slot == kernel::task_syscall_table_unmapped_slot &&
                    !record->matched && !record->handler_valid &&
                    record->disposition ==
                        kernel::TrapDisposition::unsupported &&
                    record->error == kernel::TrapError::unsupported_service &&
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

    demo::server_service_pump = &service_pump;
    demo::client_messages = &client_task_messages;
    demo::frame_store = &frame_store;

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
    shared.server_bootstrapped =
        runtime.bootstrap_worker(
            demo::kServerId,
            demo::make_server_bootstrap_event());

    std::size_t loops = 0;
    while ((!shared.missing_timeout_seen || shared.server_timeouts < 2u ||
            shared.replies_received < demo::kPublishedFrameCount) &&
           loops < 64u) {
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

    const auto runtime_status = demo::inspect_runtime_trace(runtime_trace);
    const auto pump_status = demo::inspect_pump_trace(pump_trace);
    const auto message_dispatch_status =
        demo::inspect_message_dispatch_trace(message_dispatch_trace);
    const auto message_frame_status =
        demo::inspect_message_frame_trace(message_frame_trace);
    const auto frame_status = demo::inspect_frame_trace(frame_trace);
    const auto syscall_dispatch_status =
        demo::inspect_syscall_dispatch_trace(syscall_dispatch_trace);
    const auto syscall_table_status =
        demo::inspect_syscall_table_trace(syscall_table_trace);

    const bool traces_ok = runtime_status.ok() && pump_status.ok() &&
                           message_dispatch_status.ok() &&
                           message_frame_status.ok() && frame_status.ok() &&
                           syscall_dispatch_status.ok() &&
                           syscall_table_status.ok();
    const bool replies_ok =
        shared.replies_received == demo::kPublishedFrameCount &&
        shared.reply_sequences ==
            std::array<std::uint64_t, demo::kPublishedFrameCount>{
                demo::kRequestSequences[0],
                demo::kRequestSequences[1],
                demo::kRequestSequences[2],
                demo::kRequestSequences[3],
            } &&
        shared.reply_values == demo::kReplyValues;
    const bool taken_ok =
        shared.taken_ok ==
            std::array<bool, demo::kPublishedFrameCount>{
                true,
                true,
                true,
                true,
            } &&
        shared.taken_tokens == demo::kFrameTokens;
    const bool frame_values_ok =
        shared.completed_frames[0].writeback_seen &&
        shared.completed_frames[0].syscall_id ==
            static_cast<std::uint16_t>(kernel::TaskSyscallId::yield) &&
        shared.completed_frames[0].return_value == 1u &&
        shared.completed_frames[0].disposition ==
            kernel::TrapDisposition::handled &&
        shared.completed_frames[0].error == kernel::TrapError::none &&
        shared.completed_frames[1].writeback_seen &&
        shared.completed_frames[1].syscall_id ==
            static_cast<std::uint16_t>(kernel::TaskSyscallId::sleep_until) &&
        shared.completed_frames[1].arg0 == 21u &&
        shared.completed_frames[1].return_value == 21u &&
        shared.completed_frames[2].writeback_seen &&
        shared.completed_frames[2].syscall_id ==
            static_cast<std::uint16_t>(kernel::TaskSyscallId::capability_call) &&
        shared.completed_frames[2].arg0 == 7u &&
        shared.completed_frames[2].arg1 == 2u &&
        shared.completed_frames[2].arg2 == 33u &&
        shared.completed_frames[2].return_value == 42u &&
        shared.completed_frames[3].writeback_seen &&
        shared.completed_frames[3].syscall_id == demo::kInvalidSyscallId &&
        shared.completed_frames[3].arg0 == 0xAAu &&
        shared.completed_frames[3].arg1 == 0xBBu &&
        shared.completed_frames[3].return_value == 0u &&
        shared.completed_frames[3].disposition ==
            kernel::TrapDisposition::unsupported &&
        shared.completed_frames[3].error ==
            kernel::TrapError::unsupported_service;
    const bool surface_ok =
        dispatch_state.yield_calls == 1u &&
        dispatch_state.sleep_calls == 1u &&
        dispatch_state.capability_calls == 1u &&
        dispatch_state.last_due == 21u &&
        dispatch_state.last_capability_id == 7u &&
        dispatch_state.last_capability_operation == 2u &&
        dispatch_state.last_capability_payload == 33u &&
        frame_adapter_state.capture_calls == demo::kPublishedFrameCount &&
        frame_adapter_state.writeback_calls == demo::kPublishedFrameCount &&
        result_ready_state.ready_calls == demo::kPublishedFrameCount &&
        result_ready_state.last_value == 0u &&
        result_ready_state.last_disposition ==
            kernel::TrapDisposition::unsupported &&
        result_ready_state.last_error ==
            kernel::TrapError::unsupported_service &&
        result_ready_state.last_writeback_seen;
    const bool ok = shared.mailbox_valid && shared.dispatcher_valid &&
                    shared.frame_store_valid && shared.frame_bridge_valid &&
                    shared.message_frame_bridge_valid &&
                    shared.service_loop_valid && shared.service_drain_valid &&
                    shared.service_pump_valid && shared.server_bootstrapped &&
                    shared.client_bootstrapped && shared.client_publish_all &&
                    shared.client_send_all && shared.client_reply_wait_armed &&
                    shared.client_missing_wait_armed &&
                    shared.missing_timeout_seen && shared.hold_ready_seen &&
                    shared.stale_ready_seen && shared.total_served == 5u &&
                    shared.server_timeouts >= 2u &&
                    shared.wait_arm_successes >= 4u &&
                    shared.wait_arm_index >= 4u &&
                    shared.idle_after_finish && shared.idle_runs >= 1u &&
                    shared.server_runs >= 7u && shared.client_runs == 6u &&
                    replies_ok && taken_ok && frame_values_ok && surface_ok &&
                    traces_ok && message_dispatch_trace.size() == 5u &&
                    message_frame_trace.size() == 5u &&
                    frame_trace.size() == 12u &&
                    syscall_dispatch_trace.size() == 3u &&
                    syscall_table_trace.size() == 4u &&
                    pump_trace.size() >= 7u && frame_store.pending() == 0u &&
                    mailbox.pending_requests() == 0u &&
                    mailbox.pending_replies() == 0u &&
                    mailbox.reply_waiters() == 0u &&
                    mailbox.receive_waiting() && loops < 64u;

    std::printf(
        "[runtime-task-message-syscall-frame-demo] ok=%d valid=%d frame_store=%d frame_bridge=%d message_frame=%d loop=%d drain=%d pump=%d server_boot=%d client_boot=%d publish=%d send=%d replies=%u missing_timeout=%d served=%zu timeouts=%u arms=%zu idle=%d pending_frames=%zu pending_req=%zu pending_reply=%zu reply_waiters=%zu waiting=%d loops=%llu\n",
        ok ? 1 : 0,
        shared.mailbox_valid ? 1 : 0,
        shared.frame_store_valid ? 1 : 0,
        shared.frame_bridge_valid ? 1 : 0,
        shared.message_frame_bridge_valid ? 1 : 0,
        shared.service_loop_valid ? 1 : 0,
        shared.service_drain_valid ? 1 : 0,
        shared.service_pump_valid ? 1 : 0,
        shared.server_bootstrapped ? 1 : 0,
        shared.client_bootstrapped ? 1 : 0,
        shared.client_publish_all ? 1 : 0,
        shared.client_send_all ? 1 : 0,
        shared.replies_received,
        shared.missing_timeout_seen ? 1 : 0,
        shared.total_served,
        shared.server_timeouts,
        shared.wait_arm_successes,
        shared.idle_after_finish ? 1 : 0,
        frame_store.pending(),
        mailbox.pending_requests(),
        mailbox.pending_replies(),
        mailbox.reply_waiters(),
        mailbox.receive_waiting() ? 1 : 0,
        static_cast<unsigned long long>(loops));
    std::printf(
        "[runtime-task-message-syscall-frame-values] seq0=%llu seq1=%llu seq2=%llu seq3=%llu reply0=%llu reply1=%llu reply2=%llu reply3=%llu cap_id=%llu cap_op=%llu cap_payload=%llu invalid_disp=%s invalid_err=%s stale=%d hold=%d\n",
        static_cast<unsigned long long>(shared.reply_sequences[0]),
        static_cast<unsigned long long>(shared.reply_sequences[1]),
        static_cast<unsigned long long>(shared.reply_sequences[2]),
        static_cast<unsigned long long>(shared.reply_sequences[3]),
        static_cast<unsigned long long>(shared.reply_values[0]),
        static_cast<unsigned long long>(shared.reply_values[1]),
        static_cast<unsigned long long>(shared.reply_values[2]),
        static_cast<unsigned long long>(shared.reply_values[3]),
        static_cast<unsigned long long>(shared.completed_frames[2].arg0),
        static_cast<unsigned long long>(shared.completed_frames[2].arg1),
        static_cast<unsigned long long>(shared.completed_frames[2].arg2),
        kernel::trap_disposition_name(shared.completed_frames[3].disposition),
        kernel::trap_error_name(shared.completed_frames[3].error),
        shared.stale_ready_seen ? 1 : 0,
        shared.hold_ready_seen ? 1 : 0);
    std::printf(
        "[runtime-task-message-syscall-frame-trace] ok=%d runtime=%d pump=%d message_dispatch=%d message_frame=%d syscall_frame=%d syscall_dispatch=%d syscall_table=%d hold_count=%zu\n",
        traces_ok ? 1 : 0,
        runtime_status.ok() ? 1 : 0,
        pump_status.ok() ? 1 : 0,
        message_dispatch_status.ok() ? 1 : 0,
        message_frame_status.ok() ? 1 : 0,
        frame_status.ok() ? 1 : 0,
        syscall_dispatch_status.ok() ? 1 : 0,
        syscall_table_status.ok() ? 1 : 0,
        pump_status.hold_count);
    return ok ? 0 : 1;
}
