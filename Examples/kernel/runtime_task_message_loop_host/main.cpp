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
import kernel.task_message_service_loop;
import kernel.task_state;
import kernel.thread;

namespace demo {
    struct Config : kernel::KernelConfig {
        static constexpr bool enable_timer = true;
        static constexpr std::size_t priority_levels = 2;
        static constexpr std::size_t evtq_capacity = 48;
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

    struct SharedState {
        bool mailbox_valid{false};
        bool dispatcher_valid{false};
        bool service_loop_valid{false};
        bool server_bootstrapped{false};
        bool client_bootstrapped{false};
        bool server_wait_armed{false};
        bool server_timeout_seen{false};
        bool server_rewait_armed{false};
        bool request_sent{false};
        bool client_wait_armed{false};
        bool dispatch_accepted{false};
        bool dispatch_matched{false};
        bool dispatch_handled{false};
        bool dispatch_replied{false};
        bool reply_received{false};
        bool reply_timeout_seen{false};
        bool idle_after_finish{false};
        bool runtime_trace_server_bootstrap_ok{false};
        bool runtime_trace_client_bootstrap_ok{false};
        bool runtime_trace_tick_ok{false};
        bool runtime_trace_idle_ok{false};
        bool service_trace_wait_ok{false};
        bool service_trace_timeout_ok{false};
        bool service_trace_rewait_ok{false};
        bool service_trace_dispatch_ok{false};
        std::uint32_t idle_runs{0};
        std::uint32_t server_runs{0};
        std::uint32_t client_runs{0};
        std::uint32_t server_timeouts{0};
        std::uint64_t last_request_from{0};
        std::uint64_t last_request_label{0};
        std::uint64_t last_request_value{0};
        std::uint64_t last_request_sequence{0};
        std::uint64_t last_reply_from{0};
        std::uint64_t last_reply_sequence{0};
        std::uint64_t last_reply_value{0};
    };

    struct EchoHandlerState {
        std::uint32_t calls{0};
        std::uint64_t last_from{0};
        std::uint64_t last_value{0};
        std::uint64_t last_sequence{0};
    };

    struct EchoHandler {
        EchoHandlerState* state{nullptr};

        [[nodiscard]] kernel::TaskMessageHandleResult dispatch(
            const kernel::RuntimeMailboxRequest& request) const noexcept
        {
            if (state == nullptr) {
                return kernel::unhandled_task_message();
            }

            ++state->calls;
            state->last_from = request.from.value;
            state->last_value = request.value;
            state->last_sequence = request.sequence;
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
                     kernel::ThreadControl& control,
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
        kernel::RuntimeTraceBuffer<ManualTimeSource::Tick, 32>;
    using Mailbox = kernel::RuntimeMailbox<RunningScheduler, 4, 4, 4>;
    using TaskMessages = kernel::TaskMessageApi<Mailbox>;
    using ServerTable = kernel::TaskMessageTable<1>;
    using Dispatcher = kernel::TaskMessageDispatcher<TaskMessages, ServerTable>;
    using ServiceTrace = kernel::TaskMessageServiceLoopTraceBuffer<8>;
    using ServiceLoop =
        kernel::TaskMessageServiceLoop<Dispatcher, ServiceTrace>;

    inline constexpr auto kIdleId = Registry::id_of<IdleTask>();
    inline constexpr auto kServerId = Registry::id_of<ServerTask>();
    inline constexpr auto kClientId = Registry::id_of<ClientTask>();

    inline constexpr std::uint32_t kServerBootstrapPayload{1u};
    inline constexpr std::uint32_t kClientBootstrapPayload{2u};
    inline constexpr std::uint32_t kIdlePayload{0x77u};
    inline constexpr ManualTimeSource::Tick kInitialReceiveDue{3u};
    inline constexpr ManualTimeSource::Tick kSecondReceiveDue{7u};
    inline constexpr ManualTimeSource::Tick kReplyDue{10u};
    inline constexpr std::uint64_t kEchoLabel{0xCA11u};
    inline constexpr std::uint64_t kEchoValue{42u};
    inline constexpr std::uint64_t kRequestSequence{0x55u};
    inline constexpr std::uint64_t kReplyValue{kEchoValue + 1000u};

    inline ServiceLoop* server_service_loop{nullptr};
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
        if (context.shared->reply_received) {
            context.shared->idle_after_finish = true;
        }

        std::printf("[idle] run=%u reply=%d now=%llu\n",
                    context.shared->idle_runs,
                    context.shared->reply_received ? 1 : 0,
                    static_cast<unsigned long long>(ManualTimeSource::now()));
    }

    void server_step(ServerContext& context,
                     kernel::ThreadControl& control,
                     kernel::Event event)
    {
        if (context.shared == nullptr || server_service_loop == nullptr ||
            !server_service_loop->valid()) {
            return;
        }

        if (event.id == kernel::EventId::init) {
            std::printf("[server] init\n");
            return;
        }

        if (event.id == kernel::EventId::terminate) {
            control.finish();
            return;
        }

        if (event.id == kernel::EventId::user0 &&
            kernel::payload_u32(event) == kServerBootstrapPayload) {
            ++context.shared->server_runs;
            context.shared->server_wait_armed =
                server_service_loop->wait_receive_until(kInitialReceiveDue);
            std::printf("[server] wait due=%llu armed=%d\n",
                        static_cast<unsigned long long>(kInitialReceiveDue),
                        context.shared->server_wait_armed ? 1 : 0);
            return;
        }

        const auto result =
            server_service_loop->step_and_wait_until(event, kSecondReceiveDue);
        if (result.timeout_consumed) {
            ++context.shared->server_runs;
            ++context.shared->server_timeouts;
            context.shared->server_timeout_seen = true;
            context.shared->server_rewait_armed = result.wait_armed;
            std::printf(
                "[server] recv-timeout count=%u now=%llu rewait=%d due=%llu\n",
                context.shared->server_timeouts,
                static_cast<unsigned long long>(ManualTimeSource::now()),
                context.shared->server_rewait_armed ? 1 : 0,
                static_cast<unsigned long long>(kSecondReceiveDue));
            return;
        }

        if (!result.dispatch.accepted) {
            return;
        }

        ++context.shared->server_runs;
        context.shared->dispatch_accepted = result.dispatch.accepted;
        context.shared->dispatch_matched = result.dispatch.matched;
        context.shared->dispatch_handled = result.dispatch.handled;
        context.shared->dispatch_replied = result.dispatch.replied;
        context.shared->last_request_from =
            static_cast<std::uint64_t>(result.dispatch.request.from.value);
        context.shared->last_request_label = result.dispatch.request.label;
        context.shared->last_request_value = result.dispatch.request.value;
        context.shared->last_request_sequence = result.dispatch.request.sequence;

        std::printf(
            "[server] dispatch from=%llu label=%llu value=%llu seq=%llu matched=%d handled=%d replied=%d\n",
            static_cast<unsigned long long>(result.dispatch.request.from.value),
            static_cast<unsigned long long>(result.dispatch.request.label),
            static_cast<unsigned long long>(result.dispatch.request.value),
            static_cast<unsigned long long>(result.dispatch.request.sequence),
            result.dispatch.matched ? 1 : 0,
            result.dispatch.handled ? 1 : 0,
            result.dispatch.replied ? 1 : 0);
        control.finish();
    }

    void client_step(ClientContext& context,
                     kernel::ThreadControl& control,
                     kernel::Event event)
    {
        if (context.shared == nullptr || client_messages == nullptr ||
            !client_messages->valid()) {
            return;
        }

        if (event.id == kernel::EventId::init) {
            std::printf("[client] init\n");
            return;
        }

        if (event.id == kernel::EventId::terminate) {
            control.finish();
            return;
        }

        if (event.id == kernel::EventId::user0 &&
            kernel::payload_u32(event) == kClientBootstrapPayload) {
            ++context.shared->client_runs;
            context.shared->request_sent = client_messages->send(
                kEchoLabel, kEchoValue, kRequestSequence);
            context.shared->client_wait_armed =
                client_messages->wait_reply_until(kReplyDue);
            std::printf("[client] send=%d wait=%d due=%llu\n",
                        context.shared->request_sent ? 1 : 0,
                        context.shared->client_wait_armed ? 1 : 0,
                        static_cast<unsigned long long>(kReplyDue));
            return;
        }

        if (client_messages->consume_reply_timeout(event)) {
            ++context.shared->client_runs;
            context.shared->reply_timeout_seen = true;
            std::printf("[client] reply-timeout now=%llu\n",
                        static_cast<unsigned long long>(ManualTimeSource::now()));
            control.finish();
            return;
        }

        kernel::RuntimeMailboxReply reply{};
        if (!client_messages->receive_reply(reply)) {
            return;
        }

        ++context.shared->client_runs;
        context.shared->last_reply_from =
            static_cast<std::uint64_t>(reply.from.value);
        context.shared->last_reply_sequence = reply.sequence;
        context.shared->last_reply_value = reply.value;
        context.shared->reply_received =
            reply.from == kServerId && reply.to == kClientId &&
            reply.sequence == kRequestSequence && reply.value == kReplyValue;

        std::printf("[client] reply from=%llu seq=%llu value=%llu ok=%d\n",
                    static_cast<unsigned long long>(reply.from.value),
                    static_cast<unsigned long long>(reply.sequence),
                    static_cast<unsigned long long>(reply.value),
                    context.shared->reply_received ? 1 : 0);
        control.finish();
    }

    void inspect_runtime_trace(RuntimeTrace& trace, SharedState& shared) noexcept
    {
        for (std::size_t i = 0; i < trace.size(); ++i) {
            const auto* record = trace.at(i);
            if (record == nullptr) {
                continue;
            }

            if (record->task_valid) {
                std::printf(
                    "[runtime-trace] t=%llu kind=%s task=%llu event=%u value=%llu ok=%d\n",
                    static_cast<unsigned long long>(record->stamp),
                    kernel::runtime_trace_kind_name(record->kind),
                    static_cast<unsigned long long>(record->task.value),
                    static_cast<unsigned int>(record->event_id),
                    static_cast<unsigned long long>(record->value),
                    record->ok ? 1 : 0);
            } else {
                std::printf(
                    "[runtime-trace] t=%llu kind=%s task=- event=%u value=%llu ok=%d\n",
                    static_cast<unsigned long long>(record->stamp),
                    kernel::runtime_trace_kind_name(record->kind),
                    static_cast<unsigned int>(record->event_id),
                    static_cast<unsigned long long>(record->value),
                    record->ok ? 1 : 0);
            }

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
            case kernel::RuntimeTraceKind::tick:
                shared.runtime_trace_tick_ok =
                    shared.runtime_trace_tick_ok ||
                    (!record->task_valid &&
                     record->event_id == kernel::EventId::tick &&
                     record->value >= 1u && record->ok);
                break;
            case kernel::RuntimeTraceKind::idle_bootstrap:
                shared.runtime_trace_idle_ok =
                    shared.runtime_trace_idle_ok ||
                    (record->task_valid && record->task == kIdleId &&
                     record->event_id == kernel::EventId::user0 &&
                     record->value == kIdlePayload && record->ok);
                break;
            case kernel::RuntimeTraceKind::yield:
            case kernel::RuntimeTraceKind::sleep:
            case kernel::RuntimeTraceKind::isr_defer:
                break;
            }
        }
    }

    void inspect_service_trace(ServiceTrace& trace, SharedState& shared) noexcept
    {
        for (std::size_t i = 0; i < trace.size(); ++i) {
            const auto* record = trace.at(i);
            if (record == nullptr) {
                continue;
            }

            std::printf(
                "[service-loop-trace] seq=%llu kind=%s event=%u value=%llu ok=%d accepted=%d matched=%d handled=%d replied=%d request=%d from=%llu label=%llu req_seq=%llu\n",
                static_cast<unsigned long long>(record->sequence),
                kernel::task_message_service_loop_trace_kind_name(record->kind),
                static_cast<unsigned int>(record->event_id),
                static_cast<unsigned long long>(record->value),
                record->ok ? 1 : 0,
                record->dispatch_accepted ? 1 : 0,
                record->matched ? 1 : 0,
                record->handled ? 1 : 0,
                record->replied ? 1 : 0,
                record->request_valid ? 1 : 0,
                static_cast<unsigned long long>(record->request.from.value),
                static_cast<unsigned long long>(record->request.label),
                static_cast<unsigned long long>(record->request.sequence));

            shared.service_trace_wait_ok =
                shared.service_trace_wait_ok ||
                (record->sequence == 1u &&
                 record->kind == kernel::TaskMessageServiceLoopTraceKind::wait &&
                 record->event_id == kernel::EventId::message &&
                 record->value == kInitialReceiveDue && record->ok);
            shared.service_trace_timeout_ok =
                shared.service_trace_timeout_ok ||
                (record->sequence == 2u &&
                 record->kind ==
                     kernel::TaskMessageServiceLoopTraceKind::timeout &&
                 record->event_id == kernel::EventId::sync &&
                 record->value ==
                     kernel::runtime_mailbox_receive_timeout_code &&
                 record->ok);
            shared.service_trace_rewait_ok =
                shared.service_trace_rewait_ok ||
                (record->sequence == 3u &&
                 record->kind == kernel::TaskMessageServiceLoopTraceKind::wait &&
                 record->event_id == kernel::EventId::message &&
                 record->value == kSecondReceiveDue && record->ok);
            shared.service_trace_dispatch_ok =
                shared.service_trace_dispatch_ok ||
                (record->sequence == 4u &&
                 record->kind ==
                     kernel::TaskMessageServiceLoopTraceKind::dispatch &&
                 record->event_id == kernel::EventId::message &&
                 record->value == kReplyValue && record->ok &&
                 record->dispatch_accepted && record->matched &&
                 record->handler_valid && record->handled &&
                 record->replied && record->request_valid &&
                 record->request.from == kClientId &&
                 record->request.label == kEchoLabel &&
                 record->request.value == kEchoValue &&
                 record->request.sequence == kRequestSequence);
        }
    }
}

int main()
{
    demo::ManualTimeSource::reset();

    demo::Registry registry{};
    demo::Caps caps{};
    auto created = kernel::make_scheduler<demo::Config>(registry, caps);
    auto running = kernel::start(std::move(created));
    demo::RuntimeTrace runtime_trace{};
    demo::ServiceTrace service_trace{};
    demo::SharedState shared{};
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
        kernel::make_task_message_service_loop(dispatcher, &service_trace);

    demo::server_service_loop = &service_loop;
    demo::client_messages = &client_task_messages;

    auto& idle = registry.get<demo::IdleTask>();
    idle.context.shared = &shared;

    auto& server = registry.get<demo::ServerTask>();
    server.context.shared = &shared;

    auto& client = registry.get<demo::ClientTask>();
    client.context.shared = &shared;

    while (running.run_once()) {
    }

    shared.mailbox_valid =
        mailbox.valid() && mailbox.server() == demo::kServerId &&
        server_task_messages.valid() && client_task_messages.valid();
    shared.dispatcher_valid = dispatcher.valid();
    shared.service_loop_valid = service_loop.valid();
    shared.server_bootstrapped =
        runtime.bootstrap_worker(
            demo::kServerId, demo::make_server_bootstrap_event());

    std::size_t loops = 0;
    while ((demo::ManualTimeSource::now() <= demo::kReplyDue + 1u ||
            !shared.idle_after_finish) &&
           loops < 32u) {
        if (shared.server_timeout_seen && !shared.client_bootstrapped) {
            shared.client_bootstrapped =
                runtime.bootstrap_worker(
                    demo::kClientId,
                    demo::make_client_bootstrap_event());
        }

        (void)runtime.run_once_or_idle(demo::ManualTimeSource::now());
        demo::ManualTimeSource::advance(1u);
        ++loops;
    }

    demo::inspect_runtime_trace(runtime_trace, shared);
    demo::inspect_service_trace(service_trace, shared);

    const bool runtime_trace_ok = shared.runtime_trace_server_bootstrap_ok &&
                                  shared.runtime_trace_client_bootstrap_ok &&
                                  shared.runtime_trace_tick_ok &&
                                  shared.runtime_trace_idle_ok;
    const bool service_trace_ok = shared.service_trace_wait_ok &&
                                  shared.service_trace_timeout_ok &&
                                  shared.service_trace_rewait_ok &&
                                  shared.service_trace_dispatch_ok;
    const bool ok = shared.mailbox_valid && shared.dispatcher_valid &&
                    shared.service_loop_valid && shared.server_bootstrapped &&
                    shared.client_bootstrapped && shared.server_wait_armed &&
                    shared.server_timeout_seen && shared.server_rewait_armed &&
                    shared.server_timeouts == 1u && shared.request_sent &&
                    shared.client_wait_armed && shared.dispatch_accepted &&
                    shared.dispatch_matched && shared.dispatch_handled &&
                    shared.dispatch_replied && shared.reply_received &&
                    !shared.reply_timeout_seen && shared.idle_after_finish &&
                    runtime_trace_ok && service_trace_ok &&
                    mailbox.pending_requests() == 0u &&
                    mailbox.pending_replies() == 0u &&
                    mailbox.reply_waiters() == 0u &&
                    !mailbox.receive_waiting() &&
                    shared.last_request_from ==
                        static_cast<std::uint64_t>(demo::kClientId.value) &&
                    shared.last_request_label == demo::kEchoLabel &&
                    shared.last_request_value == demo::kEchoValue &&
                    shared.last_request_sequence == demo::kRequestSequence &&
                    shared.last_reply_from ==
                        static_cast<std::uint64_t>(demo::kServerId.value) &&
                    shared.last_reply_sequence == demo::kRequestSequence &&
                    shared.last_reply_value == demo::kReplyValue &&
                    echo_state.calls == 1u &&
                    echo_state.last_from ==
                        static_cast<std::uint64_t>(demo::kClientId.value) &&
                    echo_state.last_value == demo::kEchoValue &&
                    echo_state.last_sequence == demo::kRequestSequence &&
                    service_trace.size() == 4u && shared.idle_runs >= 1u &&
                    shared.server_runs == 3u && shared.client_runs == 2u;

    std::printf(
        "[runtime-task-message-loop-demo] ok=%d valid=%d dispatcher=%d loop=%d server_boot=%d client_boot=%d recv_wait=%d recv_timeout=%d send=%d reply_wait=%d dispatch=%d reply_recv=%d idle=%d pending_req=%zu pending_reply=%zu reply_waiters=%zu loops=%llu\n",
        ok ? 1 : 0,
        shared.mailbox_valid ? 1 : 0,
        shared.dispatcher_valid ? 1 : 0,
        shared.service_loop_valid ? 1 : 0,
        shared.server_bootstrapped ? 1 : 0,
        shared.client_bootstrapped ? 1 : 0,
        shared.server_wait_armed ? 1 : 0,
        shared.server_timeout_seen ? 1 : 0,
        shared.request_sent ? 1 : 0,
        shared.client_wait_armed ? 1 : 0,
        (shared.dispatch_accepted && shared.dispatch_matched &&
         shared.dispatch_handled && shared.dispatch_replied)
            ? 1
            : 0,
        shared.reply_received ? 1 : 0,
        shared.idle_after_finish ? 1 : 0,
        mailbox.pending_requests(),
        mailbox.pending_replies(),
        mailbox.reply_waiters(),
        static_cast<unsigned long long>(loops));
    std::printf(
        "[runtime-task-message-loop-values] request_from=%llu label=%llu value=%llu seq=%llu reply_from=%llu reply=%llu idle_runs=%u server_runs=%u client_runs=%u\n",
        static_cast<unsigned long long>(shared.last_request_from),
        static_cast<unsigned long long>(shared.last_request_label),
        static_cast<unsigned long long>(shared.last_request_value),
        static_cast<unsigned long long>(shared.last_request_sequence),
        static_cast<unsigned long long>(shared.last_reply_from),
        static_cast<unsigned long long>(shared.last_reply_value),
        shared.idle_runs,
        shared.server_runs,
        shared.client_runs);
    std::printf(
        "[runtime-task-message-loop-trace] ok=%d runtime=%d service=%d server_boot=%d client_boot=%d tick=%d idle=%d wait=%d timeout=%d rewait=%d dispatch=%d\n",
        (runtime_trace_ok && service_trace_ok) ? 1 : 0,
        runtime_trace_ok ? 1 : 0,
        service_trace_ok ? 1 : 0,
        shared.runtime_trace_server_bootstrap_ok ? 1 : 0,
        shared.runtime_trace_client_bootstrap_ok ? 1 : 0,
        shared.runtime_trace_tick_ok ? 1 : 0,
        shared.runtime_trace_idle_ok ? 1 : 0,
        shared.service_trace_wait_ok ? 1 : 0,
        shared.service_trace_timeout_ok ? 1 : 0,
        shared.service_trace_rewait_ok ? 1 : 0,
        shared.service_trace_dispatch_ok ? 1 : 0);
    return ok ? 0 : 1;
}
