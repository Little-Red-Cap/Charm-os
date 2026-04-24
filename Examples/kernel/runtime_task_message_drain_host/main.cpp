#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <utility>

import kernel.capabilities;
import kernel.config;
import kernel.eda;
import kernel.evt;
import kernel.runtime_bridge;
import kernel.scheduler;
import kernel.task_message_service_drain;
import kernel.task_state;
import kernel.thread;

namespace demo {
    struct Config : kernel::KernelConfig {
        static constexpr std::size_t priority_levels = 2;
        static constexpr std::size_t evtq_capacity = 48;
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

    inline constexpr std::size_t kMessageCount{3u};
    inline constexpr std::size_t kDrainBudget{2u};
    inline constexpr std::uint32_t kServerBootstrapPayload{1u};
    inline constexpr std::uint32_t kClientBootstrapPayload{2u};
    inline constexpr std::uint32_t kIdlePayload{0x91u};
    inline constexpr std::uint64_t kEchoLabel{0xCA11u};
    inline constexpr std::array<std::uint64_t, kMessageCount> kRequestValues{
        11u,
        22u,
        33u,
    };
    inline constexpr std::array<std::uint64_t, kMessageCount> kRequestSequences{
        0x41u,
        0x42u,
        0x43u,
    };
    inline constexpr std::array<std::uint64_t, kMessageCount> kReplyValues{
        kRequestValues[0] + 1000u,
        kRequestValues[1] + 1000u,
        kRequestValues[2] + 1000u,
    };

    struct SharedState {
        bool mailbox_valid{false};
        bool dispatcher_valid{false};
        bool service_loop_valid{false};
        bool service_drain_valid{false};
        bool server_bootstrapped{false};
        bool client_bootstrapped{false};
        bool server_bootstrap_seen{false};
        bool client_send_all{false};
        bool idle_after_finish{false};
        bool runtime_trace_server_bootstrap_ok{false};
        bool runtime_trace_client_bootstrap_ok{false};
        bool runtime_trace_idle_ok{false};
        bool drain_trace_dispatch_1_ok{false};
        bool drain_trace_dispatch_2_ok{false};
        bool drain_trace_budget_stop_ok{false};
        bool drain_trace_dispatch_3_ok{false};
        bool drain_trace_queue_empty_ok{false};
        bool drain_trace_stale_ready_ok{false};
        std::uint32_t idle_runs{0};
        std::uint32_t server_runs{0};
        std::uint32_t client_runs{0};
        std::uint32_t replies_received{0};
        std::size_t total_served{0};
        std::array<std::uint64_t, kMessageCount> reply_sequences{};
        std::array<std::uint64_t, kMessageCount> reply_values{};
    };

    struct EchoHandlerState {
        std::uint32_t calls{0};
        std::array<std::uint64_t, kMessageCount> froms{};
        std::array<std::uint64_t, kMessageCount> sequences{};
        std::array<std::uint64_t, kMessageCount> values{};
    };

    struct EchoHandler {
        EchoHandlerState* state{nullptr};

        [[nodiscard]] kernel::TaskMessageHandleResult dispatch(
            const kernel::RuntimeMailboxRequest& request) const noexcept
        {
            if (state == nullptr || state->calls >= kMessageCount) {
                return kernel::unhandled_task_message();
            }

            const auto index = static_cast<std::size_t>(state->calls);
            state->froms[index] = request.from.value;
            state->sequences[index] = request.sequence;
            state->values[index] = request.value;
            ++state->calls;
            return kernel::handled_task_message(request.value + 1000u);
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
        kernel::RuntimeTraceBuffer<ManualTimeSource::Tick, 16>;
    using Mailbox = kernel::RuntimeMailbox<RunningScheduler, 8, 8, 4>;
    using TaskMessages = kernel::TaskMessageApi<Mailbox>;
    using ServerTable = kernel::TaskMessageTable<1>;
    using Dispatcher = kernel::TaskMessageDispatcher<TaskMessages, ServerTable>;
    using ServiceLoop = kernel::TaskMessageServiceLoop<Dispatcher>;
    using DrainTrace = kernel::TaskMessageServiceDrainTraceBuffer<8>;
    using ServiceDrain = kernel::TaskMessageServiceDrain<ServiceLoop, DrainTrace>;

    inline constexpr auto kIdleId = Registry::id_of<IdleTask>();
    inline constexpr auto kServerId = Registry::id_of<ServerTask>();
    inline constexpr auto kClientId = Registry::id_of<ClientTask>();

    inline ServiceDrain* server_service_drain{nullptr};
    inline TaskMessages* client_messages{nullptr};

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
        if (context.shared->replies_received == kMessageCount) {
            context.shared->idle_after_finish = true;
        }

        std::printf("[idle] run=%u replies=%u now=%llu\n",
                    context.shared->idle_runs,
                    context.shared->replies_received,
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

        if (context.shared == nullptr || server_service_drain == nullptr ||
            !server_service_drain->valid()) {
            return;
        }

        if (event.id == kernel::EventId::user0 &&
            kernel::payload_u32(event) == kServerBootstrapPayload) {
            context.shared->server_bootstrap_seen = true;
            std::printf("[server] bootstrap\n");
            return;
        }

        const auto result = server_service_drain->step(event, kDrainBudget);
        if (!result.progressed &&
            result.stop_reason == kernel::TaskMessageServiceDrainStopReason::none) {
            return;
        }

        ++context.shared->server_runs;
        context.shared->total_served += result.served;

        std::printf(
            "[server] event=%u served=%zu stop=%s reply=%llu total=%zu progressed=%d\n",
            static_cast<unsigned int>(event.id),
            result.served,
            kernel::task_message_service_drain_stop_reason_name(
                result.stop_reason),
            static_cast<unsigned long long>(result.last_dispatch.reply_value),
            context.shared->total_served,
            result.progressed ? 1 : 0);
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
            bool all_sent = true;
            for (std::size_t i = 0; i < kMessageCount; ++i) {
                all_sent = client_messages->send(
                               kEchoLabel,
                               kRequestValues[i],
                               kRequestSequences[i]) &&
                           all_sent;
            }
            context.shared->client_send_all = all_sent;
            std::printf("[client] send-all=%d count=%zu\n",
                        all_sent ? 1 : 0,
                        kMessageCount);
            return;
        }

        kernel::RuntimeMailboxReply reply{};
        if (!client_messages->receive_reply(reply)) {
            return;
        }

        ++context.shared->client_runs;
        const auto index =
            static_cast<std::size_t>(context.shared->replies_received);
        if (index < kMessageCount) {
            context.shared->reply_sequences[index] = reply.sequence;
            context.shared->reply_values[index] = reply.value;
        }
        ++context.shared->replies_received;

        std::printf("[client] reply seq=%llu value=%llu count=%u\n",
                    static_cast<unsigned long long>(reply.sequence),
                    static_cast<unsigned long long>(reply.value),
                    context.shared->replies_received);

        if (context.shared->replies_received == kMessageCount) {
            control.finish();
        }
    }

    void inspect_runtime_trace(RuntimeTrace& trace, SharedState& shared) noexcept
    {
        for (std::size_t i = 0; i < trace.size(); ++i) {
            const auto* record = trace.at(i);
            if (record == nullptr) {
                continue;
            }

            std::printf(
                "[runtime-trace] t=%llu kind=%s task=%llu event=%u value=%llu ok=%d\n",
                static_cast<unsigned long long>(record->stamp),
                kernel::runtime_trace_kind_name(record->kind),
                static_cast<unsigned long long>(record->task.value),
                static_cast<unsigned int>(record->event_id),
                static_cast<unsigned long long>(record->value),
                record->ok ? 1 : 0);

            switch (record->kind) {
            case kernel::RuntimeTraceKind::worker_bootstrap:
                shared.runtime_trace_server_bootstrap_ok =
                    shared.runtime_trace_server_bootstrap_ok ||
                    (record->task_valid && record->task == kServerId &&
                     record->event_id == kernel::EventId::user0 &&
                     record->value == kServerBootstrapPayload && record->ok);
                shared.runtime_trace_client_bootstrap_ok =
                    shared.runtime_trace_client_bootstrap_ok ||
                    (record->task_valid && record->task == kClientId &&
                     record->event_id == kernel::EventId::user0 &&
                     record->value == kClientBootstrapPayload && record->ok);
                break;
            case kernel::RuntimeTraceKind::idle_bootstrap:
                shared.runtime_trace_idle_ok =
                    shared.runtime_trace_idle_ok ||
                    (record->task_valid && record->task == kIdleId &&
                     record->event_id == kernel::EventId::user0 &&
                     record->value == kIdlePayload && record->ok);
                break;
            case kernel::RuntimeTraceKind::tick:
            case kernel::RuntimeTraceKind::isr_defer:
            case kernel::RuntimeTraceKind::yield:
            case kernel::RuntimeTraceKind::sleep:
                break;
            }
        }
    }

    void inspect_drain_trace(DrainTrace& trace, SharedState& shared) noexcept
    {
        for (std::size_t i = 0; i < trace.size(); ++i) {
            const auto* record = trace.at(i);
            if (record == nullptr) {
                continue;
            }

            std::printf(
                "[drain-trace] seq=%llu kind=%s stop=%s served=%zu budget=%zu ok=%d matched=%d handled=%d replied=%d request=%d req_seq=%llu value=%llu reply=%llu\n",
                static_cast<unsigned long long>(record->sequence),
                kernel::task_message_service_drain_trace_kind_name(record->kind),
                kernel::task_message_service_drain_stop_reason_name(
                    record->stop_reason),
                record->served,
                record->budget,
                record->ok ? 1 : 0,
                record->matched ? 1 : 0,
                record->handled ? 1 : 0,
                record->replied ? 1 : 0,
                record->request_valid ? 1 : 0,
                static_cast<unsigned long long>(record->request.sequence),
                static_cast<unsigned long long>(record->request.value),
                static_cast<unsigned long long>(record->reply_value));

            shared.drain_trace_dispatch_1_ok =
                shared.drain_trace_dispatch_1_ok ||
                (record->sequence == 1u &&
                 record->kind == kernel::TaskMessageServiceDrainTraceKind::dispatch &&
                 record->served == 1u && record->budget == kDrainBudget &&
                 record->ok && record->matched && record->handler_valid &&
                 record->handled && record->replied && record->request_valid &&
                 record->request.from == kClientId &&
                 record->request.label == kEchoLabel &&
                 record->request.sequence == kRequestSequences[0] &&
                 record->request.value == kRequestValues[0] &&
                 record->reply_value == kReplyValues[0]);
            shared.drain_trace_dispatch_2_ok =
                shared.drain_trace_dispatch_2_ok ||
                (record->sequence == 2u &&
                 record->kind == kernel::TaskMessageServiceDrainTraceKind::dispatch &&
                 record->served == 2u && record->budget == kDrainBudget &&
                 record->ok && record->matched && record->handler_valid &&
                 record->handled && record->replied && record->request_valid &&
                 record->request.from == kClientId &&
                 record->request.label == kEchoLabel &&
                 record->request.sequence == kRequestSequences[1] &&
                 record->request.value == kRequestValues[1] &&
                 record->reply_value == kReplyValues[1]);
            shared.drain_trace_budget_stop_ok =
                shared.drain_trace_budget_stop_ok ||
                (record->sequence == 3u &&
                 record->kind == kernel::TaskMessageServiceDrainTraceKind::stop &&
                 record->stop_reason ==
                     kernel::TaskMessageServiceDrainStopReason::budget_reached &&
                 record->served == kDrainBudget && record->budget == kDrainBudget &&
                 record->ok);
            shared.drain_trace_dispatch_3_ok =
                shared.drain_trace_dispatch_3_ok ||
                (record->sequence == 4u &&
                 record->kind == kernel::TaskMessageServiceDrainTraceKind::dispatch &&
                 record->served == 1u && record->budget == kDrainBudget &&
                 record->ok && record->matched && record->handler_valid &&
                 record->handled && record->replied && record->request_valid &&
                 record->request.from == kClientId &&
                 record->request.label == kEchoLabel &&
                 record->request.sequence == kRequestSequences[2] &&
                 record->request.value == kRequestValues[2] &&
                 record->reply_value == kReplyValues[2]);
            shared.drain_trace_queue_empty_ok =
                shared.drain_trace_queue_empty_ok ||
                (record->sequence == 5u &&
                 record->kind == kernel::TaskMessageServiceDrainTraceKind::stop &&
                 record->stop_reason ==
                     kernel::TaskMessageServiceDrainStopReason::queue_empty &&
                 record->served == 1u && record->budget == kDrainBudget &&
                 record->ok);
            shared.drain_trace_stale_ready_ok =
                shared.drain_trace_stale_ready_ok ||
                (record->sequence == 6u &&
                 record->kind == kernel::TaskMessageServiceDrainTraceKind::stop &&
                 record->stop_reason ==
                     kernel::TaskMessageServiceDrainStopReason::queue_empty &&
                 record->served == 0u && record->budget == kDrainBudget &&
                 record->ok);
        }
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
    demo::DrainTrace drain_trace{};
    demo::EchoHandlerState echo_state{};
    demo::EchoHandler echo_handler{
        .state = &echo_state,
    };

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
    auto table = kernel::make_task_message_table(
        std::array<kernel::TaskMessageHandlerEntry, 1>{
            kernel::task_message_handler_entry(
                demo::kEchoLabel,
                "echo",
                kernel::make_task_message_handler(echo_handler)),
        });
    demo::Dispatcher dispatcher =
        kernel::make_task_message_dispatcher(server_task_messages, table);
    demo::ServiceLoop service_loop =
        kernel::make_task_message_service_loop(dispatcher);
    demo::ServiceDrain service_drain =
        kernel::make_task_message_service_drain(service_loop, &drain_trace);

    demo::server_service_drain = &service_drain;
    demo::client_messages = &client_task_messages;

    while (running.run_once()) {
    }

    shared.mailbox_valid =
        mailbox.valid() && mailbox.server() == demo::kServerId &&
        server_task_messages.valid() && client_task_messages.valid();
    shared.dispatcher_valid = dispatcher.valid();
    shared.service_loop_valid = service_loop.valid();
    shared.service_drain_valid = service_drain.valid();
    shared.server_bootstrapped =
        runtime.bootstrap_worker(
            demo::kServerId,
            demo::make_server_bootstrap_event());
    shared.client_bootstrapped =
        runtime.bootstrap_worker(
            demo::kClientId,
            demo::make_client_bootstrap_event());

    std::size_t loops = 0;
    while ((!shared.idle_after_finish ||
            shared.replies_received < demo::kMessageCount) &&
           loops < 64u) {
        (void)runtime.run_once_or_idle(demo::ManualTimeSource::now());
        demo::ManualTimeSource::advance(1u);
        ++loops;
    }

    demo::inspect_runtime_trace(runtime_trace, shared);
    demo::inspect_drain_trace(drain_trace, shared);

    const bool runtime_trace_ok = shared.runtime_trace_server_bootstrap_ok &&
                                  shared.runtime_trace_client_bootstrap_ok &&
                                  shared.runtime_trace_idle_ok;
    const bool drain_trace_ok = shared.drain_trace_dispatch_1_ok &&
                                shared.drain_trace_dispatch_2_ok &&
                                shared.drain_trace_budget_stop_ok &&
                                shared.drain_trace_dispatch_3_ok &&
                                shared.drain_trace_queue_empty_ok &&
                                shared.drain_trace_stale_ready_ok;
    const bool handler_ok =
        echo_state.calls == demo::kMessageCount &&
        echo_state.froms[0] ==
            static_cast<std::uint64_t>(demo::kClientId.value) &&
        echo_state.froms[1] ==
            static_cast<std::uint64_t>(demo::kClientId.value) &&
        echo_state.froms[2] ==
            static_cast<std::uint64_t>(demo::kClientId.value) &&
        echo_state.sequences == demo::kRequestSequences &&
        echo_state.values == demo::kRequestValues;
    const bool replies_ok =
        shared.replies_received == demo::kMessageCount &&
        shared.reply_sequences == demo::kRequestSequences &&
        shared.reply_values == demo::kReplyValues;
    const bool ok = shared.mailbox_valid && shared.dispatcher_valid &&
                    shared.service_loop_valid && shared.service_drain_valid &&
                    shared.server_bootstrapped && shared.client_bootstrapped &&
                    shared.server_bootstrap_seen && shared.client_send_all &&
                    shared.total_served == demo::kMessageCount &&
                    shared.server_runs == 3u && shared.client_runs == 4u &&
                    shared.idle_runs >= 1u && shared.idle_after_finish &&
                    runtime_trace_ok && drain_trace_ok && handler_ok &&
                    replies_ok && drain_trace.size() == 6u &&
                    mailbox.pending_requests() == 0u &&
                    mailbox.pending_replies() == 0u &&
                    mailbox.reply_waiters() == 0u &&
                    !mailbox.receive_waiting() && loops < 64u;

    std::printf(
        "[runtime-task-message-drain-demo] ok=%d valid=%d dispatcher=%d loop=%d drain=%d server_boot=%d client_boot=%d send=%d served=%zu replies=%u idle=%d pending_req=%zu pending_reply=%zu reply_waiters=%zu loops=%llu\n",
        ok ? 1 : 0,
        shared.mailbox_valid ? 1 : 0,
        shared.dispatcher_valid ? 1 : 0,
        shared.service_loop_valid ? 1 : 0,
        shared.service_drain_valid ? 1 : 0,
        shared.server_bootstrapped ? 1 : 0,
        shared.client_bootstrapped ? 1 : 0,
        shared.client_send_all ? 1 : 0,
        shared.total_served,
        shared.replies_received,
        shared.idle_after_finish ? 1 : 0,
        mailbox.pending_requests(),
        mailbox.pending_replies(),
        mailbox.reply_waiters(),
        static_cast<unsigned long long>(loops));
    std::printf(
        "[runtime-task-message-drain-values] seq0=%llu seq1=%llu seq2=%llu reply0=%llu reply1=%llu reply2=%llu server_runs=%u client_runs=%u idle_runs=%u\n",
        static_cast<unsigned long long>(shared.reply_sequences[0]),
        static_cast<unsigned long long>(shared.reply_sequences[1]),
        static_cast<unsigned long long>(shared.reply_sequences[2]),
        static_cast<unsigned long long>(shared.reply_values[0]),
        static_cast<unsigned long long>(shared.reply_values[1]),
        static_cast<unsigned long long>(shared.reply_values[2]),
        shared.server_runs,
        shared.client_runs,
        shared.idle_runs);
    std::printf(
        "[runtime-task-message-drain-trace] ok=%d runtime=%d drain=%d boot_server=%d boot_client=%d idle=%d dispatch1=%d dispatch2=%d budget=%d dispatch3=%d queue_empty=%d stale=%d\n",
        (runtime_trace_ok && drain_trace_ok) ? 1 : 0,
        runtime_trace_ok ? 1 : 0,
        drain_trace_ok ? 1 : 0,
        shared.runtime_trace_server_bootstrap_ok ? 1 : 0,
        shared.runtime_trace_client_bootstrap_ok ? 1 : 0,
        shared.runtime_trace_idle_ok ? 1 : 0,
        shared.drain_trace_dispatch_1_ok ? 1 : 0,
        shared.drain_trace_dispatch_2_ok ? 1 : 0,
        shared.drain_trace_budget_stop_ok ? 1 : 0,
        shared.drain_trace_dispatch_3_ok ? 1 : 0,
        shared.drain_trace_queue_empty_ok ? 1 : 0,
        shared.drain_trace_stale_ready_ok ? 1 : 0);
    return ok ? 0 : 1;
}
