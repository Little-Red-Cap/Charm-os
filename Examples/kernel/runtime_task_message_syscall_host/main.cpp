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
import kernel.task_message_syscall_bridge;
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

    inline constexpr std::size_t kMessageCount{4u};
    inline constexpr std::size_t kReplyCount{3u};
    inline constexpr std::size_t kDrainBudget{2u};
    inline constexpr std::uint32_t kServerBootstrapPayload{1u};
    inline constexpr std::uint32_t kClientBootstrapPayload{2u};
    inline constexpr std::uint32_t kIdlePayload{0xB6u};
    inline constexpr std::array<ManualTimeSource::Tick, 4> kWaitDuePlan{
        3u,
        7u,
        11u,
        15u,
    };
    inline constexpr ManualTimeSource::Tick kInitialReplyDue{12u};
    inline constexpr ManualTimeSource::Tick kUnsupportedReplyDue{18u};
    inline constexpr std::array<kernel::TaskSyscallId, kMessageCount>
        kRequestSyscalls{
            kernel::TaskSyscallId::yield,
            kernel::TaskSyscallId::sleep_until,
            kernel::TaskSyscallId::debug_write,
            kernel::TaskSyscallId::capability_call,
        };
    inline constexpr std::array<std::uint64_t, kMessageCount> kRequestValues{
        0u,
        21u,
        0x44u,
        7u,
    };
    inline constexpr std::array<std::uint64_t, kMessageCount> kRequestSequences{
        0x41u,
        0x42u,
        0x43u,
        0x44u,
    };
    inline constexpr std::array<std::uint64_t, kReplyCount> kReplyValues{
        1u,
        21u,
        0x44u,
    };

    struct SharedState {
        bool mailbox_valid{false};
        bool dispatcher_valid{false};
        bool bridge_valid{false};
        bool service_loop_valid{false};
        bool service_drain_valid{false};
        bool service_pump_valid{false};
        bool server_bootstrapped{false};
        bool client_bootstrapped{false};
        bool client_send_all{false};
        bool client_reply_wait_armed{false};
        bool client_unsupported_wait_armed{false};
        bool unsupported_timeout_seen{false};
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
        std::array<std::uint64_t, kReplyCount> reply_sequences{};
        std::array<std::uint64_t, kReplyCount> reply_values{};
    };

    struct FakeDispatchSurfaceState {
        bool bound{true};
        std::uint32_t yield_calls{0};
        std::uint32_t sleep_calls{0};
        std::uint32_t debug_calls{0};
        std::uint32_t capability_calls{0};
        std::uint64_t last_due{0};
        std::uint64_t last_debug_value{0};
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
            kernel::TrapDebugWriteView write) const noexcept
        {
            if (!valid()) {
                return unbound_result();
            }

            ++state->debug_calls;
            state->last_debug_value = write.value;
            return handled_result(write.value);
        }

        [[nodiscard]] kernel::TrapResult capability_call(
            kernel::TrapCapabilityCallView) const noexcept
        {
            if (!valid()) {
                return unbound_result();
            }

            ++state->capability_calls;
            return handled_result(0u);
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
    using MessageDispatchTrace = kernel::TaskMessageDispatchTraceBuffer<8>;
    using MessageTable = kernel::TaskMessageTable<kMessageCount>;
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
    using SyscallTable = kernel::TaskSyscallTable<kReplyCount, SyscallTableTrace>;
    using BridgeTrace = kernel::TaskMessageSyscallBridgeTraceBuffer<8>;
    using SyscallBridge =
        kernel::TaskMessageSyscallBridge<SyscallTable, BridgeTrace>;

    inline constexpr auto kIdleId = Registry::id_of<IdleTask>();
    inline constexpr auto kServerId = Registry::id_of<ServerTask>();
    inline constexpr auto kClientId = Registry::id_of<ClientTask>();

    inline ServicePump* server_service_pump{nullptr};
    inline TaskMessages* client_messages{nullptr};

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
        if (context.shared->replies_received == kReplyCount &&
            context.shared->unsupported_timeout_seen &&
            context.shared->server_timeouts >= 2u) {
            context.shared->idle_after_finish = true;
        }

        std::printf("[idle] run=%u replies=%u timeout=%d now=%llu\n",
                    context.shared->idle_runs,
                    context.shared->replies_received,
                    context.shared->unsupported_timeout_seen ? 1 : 0,
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
            !client_messages->valid()) {
            return;
        }

        if (event.id == kernel::EventId::terminate) {
            control.finish();
            return;
        }

        if (event.id == kernel::EventId::user0 &&
            kernel::payload_u32(event) == kClientBootstrapPayload) {
            ++context.shared->client_runs;
            bool sent_all = true;
            for (std::size_t i = 0; i < kMessageCount; ++i) {
                sent_all = client_messages->send(
                               kernel::task_message_syscall_label(
                                   kRequestSyscalls[i]),
                               kRequestValues[i],
                               kRequestSequences[i]) &&
                           sent_all;
            }
            context.shared->client_send_all = sent_all;
            context.shared->client_reply_wait_armed =
                client_messages->wait_reply_until(kInitialReplyDue);
            std::printf("[client] send-all=%d wait=%d count=%zu\n",
                        sent_all ? 1 : 0,
                        context.shared->client_reply_wait_armed ? 1 : 0,
                        kMessageCount);
            return;
        }

        if (client_messages->consume_reply_timeout(event)) {
            ++context.shared->client_runs;
            if (context.shared->replies_received == kReplyCount) {
                context.shared->unsupported_timeout_seen = true;
                context.shared->idle_after_finish = true;
                std::printf("[client] reply-timeout unsupported now=%llu\n",
                            static_cast<unsigned long long>(
                                ManualTimeSource::now()));
            } else {
                std::printf("[client] unexpected-reply-timeout now=%llu\n",
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
        const auto index =
            static_cast<std::size_t>(context.shared->replies_received);
        if (index < kReplyCount) {
            context.shared->reply_sequences[index] = reply.sequence;
            context.shared->reply_values[index] = reply.value;
        }
        ++context.shared->replies_received;

        std::printf("[client] reply seq=%llu value=%llu count=%u\n",
                    static_cast<unsigned long long>(reply.sequence),
                    static_cast<unsigned long long>(reply.value),
                    context.shared->replies_received);

        if (context.shared->replies_received == kReplyCount &&
            !context.shared->client_unsupported_wait_armed) {
            context.shared->client_unsupported_wait_armed =
                client_messages->wait_reply_until(kUnsupportedReplyDue);
            std::printf("[client] wait-unsupported=%d due=%llu\n",
                        context.shared->client_unsupported_wait_armed ? 1 : 0,
                        static_cast<unsigned long long>(kUnsupportedReplyDue));
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

    struct DispatchTraceStatus {
        bool yield_ok{false};
        bool sleep_ok{false};
        bool debug_ok{false};
        bool unsupported_ok{false};

        [[nodiscard]] bool ok() const noexcept
        {
            return yield_ok && sleep_ok && debug_ok && unsupported_ok;
        }
    };

    struct BridgeTraceStatus {
        bool yield_ok{false};
        bool sleep_ok{false};
        bool debug_ok{false};
        bool unsupported_ok{false};

        [[nodiscard]] bool ok() const noexcept
        {
            return yield_ok && sleep_ok && debug_ok && unsupported_ok;
        }
    };

    struct SyscallTraceStatus {
        bool yield_ok{false};
        bool sleep_ok{false};
        bool debug_ok{false};

        [[nodiscard]] bool ok() const noexcept
        {
            return yield_ok && sleep_ok && debug_ok;
        }
    };

    [[nodiscard]] RuntimeTraceStatus inspect_runtime_trace(
        RuntimeTrace& trace) noexcept
    {
        RuntimeTraceStatus status{};

        for (std::size_t i = 0; i < trace.size(); ++i) {
            const auto* record = trace.at(i);
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

        for (std::size_t i = 0; i < trace.size(); ++i) {
            const auto* record = trace.at(i);
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
                 record->kind == kernel::TaskMessageServicePumpTraceKind::bootstrap &&
                 record->reason == kernel::TaskMessageServicePumpReason::bootstrap &&
                 record->event_id == kernel::EventId::user0 &&
                 record->value == kServerBootstrapPayload &&
                 record->due == kWaitDuePlan[0] &&
                 record->budget == kDrainBudget && record->served == 0u &&
                 record->ok);
            status.timeout1 =
                status.timeout1 ||
                (record->sequence == 2u &&
                 record->kind == kernel::TaskMessageServicePumpTraceKind::rearm &&
                 record->reason == kernel::TaskMessageServicePumpReason::timeout &&
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
                 record->budget == kDrainBudget && record->served == 0u &&
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
                 record->reason == kernel::TaskMessageServicePumpReason::timeout &&
                 record->event_id == kernel::EventId::sync &&
                 record->value == kernel::runtime_mailbox_receive_timeout_code &&
                 record->due == kWaitDuePlan[3] &&
                 record->budget == kDrainBudget && record->served == 0u &&
                 record->ok);
        }

        return status;
    }

    [[nodiscard]] DispatchTraceStatus inspect_dispatch_trace(
        MessageDispatchTrace& trace) noexcept
    {
        DispatchTraceStatus status{};

        for (std::size_t i = 0; i < trace.size(); ++i) {
            const auto* record = trace.at(i);
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
                        kernel::task_message_syscall_label(
                            kernel::TaskSyscallId::yield) &&
                    same_text(record->label_name, "yield"sv) &&
                    record->value == 0u &&
                    record->request_sequence == kRequestSequences[0] &&
                    record->accepted && record->matched &&
                    record->handler_valid && record->handled &&
                    record->replied && record->reply_value == kReplyValues[0];
                break;
            case 2u:
                status.sleep_ok =
                    record->from == kClientId &&
                    record->label ==
                        kernel::task_message_syscall_label(
                            kernel::TaskSyscallId::sleep_until) &&
                    same_text(record->label_name, "sleep-until"sv) &&
                    record->value == kRequestValues[1] &&
                    record->request_sequence == kRequestSequences[1] &&
                    record->accepted && record->matched &&
                    record->handler_valid && record->handled &&
                    record->replied && record->reply_value == kReplyValues[1];
                break;
            case 3u:
                status.debug_ok =
                    record->from == kClientId &&
                    record->label ==
                        kernel::task_message_syscall_label(
                            kernel::TaskSyscallId::debug_write) &&
                    same_text(record->label_name, "debug-write"sv) &&
                    record->value == kRequestValues[2] &&
                    record->request_sequence == kRequestSequences[2] &&
                    record->accepted && record->matched &&
                    record->handler_valid && record->handled &&
                    record->replied && record->reply_value == kReplyValues[2];
                break;
            case 4u:
                status.unsupported_ok =
                    record->from == kClientId &&
                    record->label ==
                        kernel::task_message_syscall_label(
                            kernel::TaskSyscallId::capability_call) &&
                    same_text(record->label_name, "capability-call"sv) &&
                    record->value == kRequestValues[3] &&
                    record->request_sequence == kRequestSequences[3] &&
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

    [[nodiscard]] BridgeTraceStatus inspect_bridge_trace(
        BridgeTrace& trace) noexcept
    {
        BridgeTraceStatus status{};

        for (std::size_t i = 0; i < trace.size(); ++i) {
            const auto* record = trace.at(i);
            if (record == nullptr) {
                continue;
            }

            const auto projection =
                kernel::task_message_syscall_semantic_projection(*record);

            std::printf(
                "[message-syscall-bridge-trace] seq=%llu label=%llu syscall=%s value=%llu req_seq=%llu supported=%d disposition=%s error=%s reply=%llu handled=%d\n",
                static_cast<unsigned long long>(record->sequence),
                static_cast<unsigned long long>(record->label),
                kernel::task_syscall_name(record->syscall),
                static_cast<unsigned long long>(record->value),
                static_cast<unsigned long long>(record->request_sequence),
                record->ingress_supported ? 1 : 0,
                kernel::trap_disposition_name(record->disposition),
                kernel::trap_error_name(record->error),
                static_cast<unsigned long long>(record->reply_value),
                record->handled ? 1 : 0);

            switch (record->sequence) {
            case 1u:
                status.yield_ok =
                    record->from == kClientId &&
                    record->syscall == kernel::TaskSyscallId::yield &&
                    record->ingress_supported &&
                    record->disposition == kernel::TrapDisposition::handled &&
                    record->error == kernel::TrapError::none &&
                    record->arg0 == 0u && record->reply_value == kReplyValues[0] &&
                    record->handled &&
                    same_text(projection.descriptor.syscall_name, "yield"sv) &&
                    projection.field_count == 0u;
                break;
            case 2u:
                status.sleep_ok =
                    record->from == kClientId &&
                    record->syscall == kernel::TaskSyscallId::sleep_until &&
                    record->ingress_supported &&
                    record->disposition == kernel::TrapDisposition::handled &&
                    record->error == kernel::TrapError::none &&
                    record->arg0 == kRequestValues[1] &&
                    record->reply_value == kReplyValues[1] && record->handled &&
                    same_text(projection.descriptor.syscall_name,
                              "sleep-until"sv) &&
                    projection.field_count == 1u &&
                    same_text(projection.fields[0].name, "due"sv) &&
                    projection.fields[0].value == kRequestValues[1];
                break;
            case 3u:
                status.debug_ok =
                    record->from == kClientId &&
                    record->syscall == kernel::TaskSyscallId::debug_write &&
                    record->ingress_supported &&
                    record->disposition == kernel::TrapDisposition::handled &&
                    record->error == kernel::TrapError::none &&
                    record->arg0 == kRequestValues[2] &&
                    record->reply_value == kReplyValues[2] && record->handled &&
                    same_text(projection.descriptor.syscall_name,
                              "debug-write"sv) &&
                    projection.field_count == 1u &&
                    same_text(projection.fields[0].name, "value"sv) &&
                    projection.fields[0].value == kRequestValues[2];
                break;
            case 4u:
                status.unsupported_ok =
                    record->from == kClientId &&
                    record->syscall == kernel::TaskSyscallId::capability_call &&
                    !record->ingress_supported &&
                    record->disposition ==
                        kernel::TrapDisposition::unsupported &&
                    record->error == kernel::TrapError::unsupported_service &&
                    record->arg0 == kRequestValues[3] && record->arg1 == 0u &&
                    record->arg2 == 0u && record->reply_value == 0u &&
                    !record->handled &&
                    same_text(projection.descriptor.syscall_name,
                              "capability-call"sv) &&
                    projection.field_count == 3u &&
                    same_text(projection.fields[0].name, "capability-id"sv) &&
                    projection.fields[0].value == kRequestValues[3] &&
                    same_text(projection.fields[1].name, "operation"sv) &&
                    projection.fields[1].value == 0u &&
                    same_text(projection.fields[2].name, "payload"sv) &&
                    projection.fields[2].value == 0u;
                break;
            default:
                break;
            }
        }

        return status;
    }

    [[nodiscard]] SyscallTraceStatus inspect_syscall_dispatch_trace(
        SyscallDispatchTrace& trace) noexcept
    {
        SyscallTraceStatus status{};

        for (std::size_t i = 0; i < trace.size(); ++i) {
            const auto* record = trace.at(i);
            if (record == nullptr) {
                continue;
            }

            std::printf(
                "[syscall-dispatch-trace] seq=%llu syscall=%s disposition=%s error=%s arg0=%llu value=%llu\n",
                static_cast<unsigned long long>(record->sequence),
                kernel::task_syscall_name(record->syscall),
                kernel::trap_disposition_name(record->disposition),
                kernel::trap_error_name(record->error),
                static_cast<unsigned long long>(record->arg0),
                static_cast<unsigned long long>(record->value));

            switch (record->sequence) {
            case 1u:
                status.yield_ok =
                    record->syscall == kernel::TaskSyscallId::yield &&
                    record->disposition == kernel::TrapDisposition::handled &&
                    record->error == kernel::TrapError::none &&
                    record->value == kReplyValues[0];
                break;
            case 2u:
                status.sleep_ok =
                    record->syscall == kernel::TaskSyscallId::sleep_until &&
                    record->arg0 == kRequestValues[1] &&
                    record->disposition == kernel::TrapDisposition::handled &&
                    record->error == kernel::TrapError::none &&
                    record->value == kReplyValues[1];
                break;
            case 3u:
                status.debug_ok =
                    record->syscall == kernel::TaskSyscallId::debug_write &&
                    record->arg0 == kRequestValues[2] &&
                    record->disposition == kernel::TrapDisposition::handled &&
                    record->error == kernel::TrapError::none &&
                    record->value == kReplyValues[2];
                break;
            default:
                break;
            }
        }

        return status;
    }

    [[nodiscard]] SyscallTraceStatus inspect_syscall_table_trace(
        SyscallTableTrace& trace) noexcept
    {
        SyscallTraceStatus status{};

        for (std::size_t i = 0; i < trace.size(); ++i) {
            const auto* record = trace.at(i);
            if (record == nullptr) {
                continue;
            }

            std::printf(
                "[syscall-table-trace] seq=%llu syscall=%s slot=%u matched=%d handler=%d disposition=%s error=%s value=%llu\n",
                static_cast<unsigned long long>(record->sequence),
                kernel::task_syscall_name(record->syscall),
                static_cast<unsigned int>(record->slot),
                record->matched ? 1 : 0,
                record->handler_valid ? 1 : 0,
                kernel::trap_disposition_name(record->disposition),
                kernel::trap_error_name(record->error),
                static_cast<unsigned long long>(record->value));

            switch (record->sequence) {
            case 1u:
                status.yield_ok =
                    record->syscall == kernel::TaskSyscallId::yield &&
                    record->slot == 0u && record->matched &&
                    record->handler_valid &&
                    record->disposition == kernel::TrapDisposition::handled &&
                    record->error == kernel::TrapError::none &&
                    record->value == kReplyValues[0];
                break;
            case 2u:
                status.sleep_ok =
                    record->syscall == kernel::TaskSyscallId::sleep_until &&
                    record->slot == 1u && record->matched &&
                    record->handler_valid && record->arg0 == kRequestValues[1] &&
                    record->disposition == kernel::TrapDisposition::handled &&
                    record->error == kernel::TrapError::none &&
                    record->value == kReplyValues[1];
                break;
            case 3u:
                status.debug_ok =
                    record->syscall == kernel::TaskSyscallId::debug_write &&
                    record->slot == 2u && record->matched &&
                    record->handler_valid && record->arg0 == kRequestValues[2] &&
                    record->disposition == kernel::TrapDisposition::handled &&
                    record->error == kernel::TrapError::none &&
                    record->value == kReplyValues[2];
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
    demo::BridgeTrace bridge_trace{};
    demo::SyscallDispatchTrace syscall_dispatch_trace{};
    demo::SyscallTableTrace syscall_table_trace{};
    demo::FakeDispatchSurfaceState dispatch_state{};

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
        std::array<kernel::TaskSyscallHandlerEntry, demo::kReplyCount>{
            kernel::task_syscall_handler_entry(
                kernel::TaskSyscallId::yield,
                kernel::make_task_syscall_handler(syscall_dispatcher)),
            kernel::task_syscall_handler_entry(
                kernel::TaskSyscallId::sleep_until,
                kernel::make_task_syscall_handler(syscall_dispatcher)),
            kernel::task_syscall_handler_entry(
                kernel::TaskSyscallId::debug_write,
                kernel::make_task_syscall_handler(syscall_dispatcher)),
        },
        &syscall_table_trace);
    demo::SyscallBridge syscall_bridge =
        kernel::make_task_message_syscall_bridge(syscall_table, &bridge_trace);
    auto message_table = kernel::make_task_message_table(
        std::array<kernel::TaskMessageHandlerEntry, demo::kMessageCount>{
            kernel::task_message_handler_entry(
                kernel::task_message_syscall_label(
                    kernel::TaskSyscallId::yield),
                kernel::task_message_syscall_label_name(
                    kernel::TaskSyscallId::yield),
                kernel::make_task_message_handler(syscall_bridge)),
            kernel::task_message_handler_entry(
                kernel::task_message_syscall_label(
                    kernel::TaskSyscallId::sleep_until),
                kernel::task_message_syscall_label_name(
                    kernel::TaskSyscallId::sleep_until),
                kernel::make_task_message_handler(syscall_bridge)),
            kernel::task_message_handler_entry(
                kernel::task_message_syscall_label(
                    kernel::TaskSyscallId::debug_write),
                kernel::task_message_syscall_label_name(
                    kernel::TaskSyscallId::debug_write),
                kernel::make_task_message_handler(syscall_bridge)),
            kernel::task_message_handler_entry(
                kernel::task_message_syscall_label(
                    kernel::TaskSyscallId::capability_call),
                kernel::task_message_syscall_label_name(
                    kernel::TaskSyscallId::capability_call),
                kernel::make_task_message_handler(syscall_bridge)),
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

    while (running.run_once()) {
    }

    shared.mailbox_valid =
        mailbox.valid() && mailbox.server() == demo::kServerId &&
        server_task_messages.valid() && client_task_messages.valid();
    shared.dispatcher_valid = message_dispatcher.valid();
    shared.bridge_valid = syscall_bridge.valid();
    shared.service_loop_valid = service_loop.valid();
    shared.service_drain_valid = service_drain.valid();
    shared.service_pump_valid = service_pump.valid();
    shared.server_bootstrapped =
        runtime.bootstrap_worker(
            demo::kServerId,
            demo::make_server_bootstrap_event());

    std::size_t loops = 0;
    while ((!shared.unsupported_timeout_seen || shared.server_timeouts < 2u ||
            shared.replies_received < demo::kReplyCount) &&
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
    const auto dispatch_status =
        demo::inspect_dispatch_trace(message_dispatch_trace);
    const auto bridge_status = demo::inspect_bridge_trace(bridge_trace);
    const auto syscall_dispatch_status =
        demo::inspect_syscall_dispatch_trace(syscall_dispatch_trace);
    const auto syscall_table_status =
        demo::inspect_syscall_table_trace(syscall_table_trace);

    const bool traces_ok = runtime_status.ok() && pump_status.ok() &&
                           dispatch_status.ok() && bridge_status.ok() &&
                           syscall_dispatch_status.ok() &&
                           syscall_table_status.ok();
    const bool replies_ok =
        shared.replies_received == demo::kReplyCount &&
        shared.reply_sequences == std::array<std::uint64_t, demo::kReplyCount>{
                                     demo::kRequestSequences[0],
                                     demo::kRequestSequences[1],
                                     demo::kRequestSequences[2],
                                 } &&
        shared.reply_values == demo::kReplyValues;
    const bool surface_ok =
        dispatch_state.yield_calls == 1u &&
        dispatch_state.sleep_calls == 1u &&
        dispatch_state.debug_calls == 1u &&
        dispatch_state.capability_calls == 0u &&
        dispatch_state.last_due == demo::kRequestValues[1] &&
        dispatch_state.last_debug_value == demo::kRequestValues[2];
    const bool ok = shared.mailbox_valid && shared.dispatcher_valid &&
                    shared.bridge_valid && shared.service_loop_valid &&
                    shared.service_drain_valid && shared.service_pump_valid &&
                    shared.server_bootstrapped && shared.client_bootstrapped &&
                    shared.client_send_all && shared.client_reply_wait_armed &&
                    shared.client_unsupported_wait_armed &&
                    shared.unsupported_timeout_seen && shared.hold_ready_seen &&
                    shared.stale_ready_seen && shared.total_served == 4u &&
                    shared.server_timeouts >= 2u &&
                    shared.wait_arm_successes >= 4u &&
                    shared.wait_arm_index >= 4u &&
                    shared.idle_after_finish && shared.idle_runs >= 1u &&
                    shared.server_runs >= 7u && shared.client_runs == 5u &&
                    replies_ok && surface_ok && traces_ok &&
                    message_dispatch_trace.size() == demo::kMessageCount &&
                    bridge_trace.size() == demo::kMessageCount &&
                    syscall_dispatch_trace.size() == demo::kReplyCount &&
                    syscall_table_trace.size() == demo::kReplyCount &&
                    pump_trace.size() >= 7u &&
                    mailbox.pending_requests() == 0u &&
                    mailbox.pending_replies() == 0u &&
                    mailbox.reply_waiters() == 0u &&
                    mailbox.receive_waiting() && loops < 64u;

    std::printf(
        "[runtime-task-message-syscall-demo] ok=%d valid=%d bridge=%d loop=%d drain=%d pump=%d server_boot=%d client_boot=%d send=%d replies=%u timeout=%d served=%zu timeouts=%u arms=%zu idle=%d pending_req=%zu pending_reply=%zu reply_waiters=%zu waiting=%d loops=%llu\n",
        ok ? 1 : 0,
        shared.mailbox_valid ? 1 : 0,
        shared.bridge_valid ? 1 : 0,
        shared.service_loop_valid ? 1 : 0,
        shared.service_drain_valid ? 1 : 0,
        shared.service_pump_valid ? 1 : 0,
        shared.server_bootstrapped ? 1 : 0,
        shared.client_bootstrapped ? 1 : 0,
        shared.client_send_all ? 1 : 0,
        shared.replies_received,
        shared.unsupported_timeout_seen ? 1 : 0,
        shared.total_served,
        shared.server_timeouts,
        shared.wait_arm_successes,
        shared.idle_after_finish ? 1 : 0,
        mailbox.pending_requests(),
        mailbox.pending_replies(),
        mailbox.reply_waiters(),
        mailbox.receive_waiting() ? 1 : 0,
        static_cast<unsigned long long>(loops));
    std::printf(
        "[runtime-task-message-syscall-values] seq0=%llu seq1=%llu seq2=%llu reply0=%llu reply1=%llu reply2=%llu yield=%u sleep=%u debug=%u capability=%u stale=%d hold=%d\n",
        static_cast<unsigned long long>(shared.reply_sequences[0]),
        static_cast<unsigned long long>(shared.reply_sequences[1]),
        static_cast<unsigned long long>(shared.reply_sequences[2]),
        static_cast<unsigned long long>(shared.reply_values[0]),
        static_cast<unsigned long long>(shared.reply_values[1]),
        static_cast<unsigned long long>(shared.reply_values[2]),
        dispatch_state.yield_calls,
        dispatch_state.sleep_calls,
        dispatch_state.debug_calls,
        dispatch_state.capability_calls,
        shared.stale_ready_seen ? 1 : 0,
        shared.hold_ready_seen ? 1 : 0);
    std::printf(
        "[runtime-task-message-syscall-trace] ok=%d runtime=%d pump=%d dispatch=%d bridge=%d syscall_dispatch=%d syscall_table=%d hold_count=%zu\n",
        traces_ok ? 1 : 0,
        runtime_status.ok() ? 1 : 0,
        pump_status.ok() ? 1 : 0,
        dispatch_status.ok() ? 1 : 0,
        bridge_status.ok() ? 1 : 0,
        syscall_dispatch_status.ok() ? 1 : 0,
        syscall_table_status.ok() ? 1 : 0,
        pump_status.hold_count);
    return ok ? 0 : 1;
}
