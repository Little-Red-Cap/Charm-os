#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <utility>

import kernel.capabilities;
import kernel.config;
import kernel.eda;
import kernel.evt;
import kernel.runtime_bridge;
import kernel.runtime_mailbox;
import kernel.scheduler;
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
        bool server_bootstrapped{false};
        bool client_bootstrapped{false};
        bool server_wait_armed{false};
        bool server_timeout_seen{false};
        bool server_rewait_armed{false};
        bool request_sent{false};
        bool request_delivered{false};
        bool reply_sent{false};
        bool client_wait_armed{false};
        bool reply_received{false};
        bool reply_timeout_seen{false};
        bool idle_after_finish{false};
        bool trace_server_bootstrap_ok{false};
        bool trace_client_bootstrap_ok{false};
        bool trace_tick_ok{false};
        bool trace_idle_ok{false};
        std::uint32_t idle_runs{0};
        std::uint32_t server_runs{0};
        std::uint32_t client_runs{0};
        std::uint32_t server_timeouts{0};
        std::uint64_t last_request_from{0};
        std::uint64_t last_request_label{0};
        std::uint64_t last_request_value{0};
        std::uint64_t last_request_sequence{0};
        std::uint64_t last_reply_from{0};
        std::uint64_t last_reply_value{0};
        std::uint64_t last_reply_sequence{0};
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

    inline constexpr auto kIdleId = Registry::id_of<IdleTask>();
    inline constexpr auto kServerId = Registry::id_of<ServerTask>();
    inline constexpr auto kClientId = Registry::id_of<ClientTask>();

    inline constexpr std::uint32_t kServerBootstrapPayload{1u};
    inline constexpr std::uint32_t kClientBootstrapPayload{2u};
    inline constexpr std::uint32_t kIdlePayload{0x77u};
    inline constexpr ManualTimeSource::Tick kInitialReceiveDue{3u};
    inline constexpr ManualTimeSource::Tick kSecondReceiveDue{7u};
    inline constexpr ManualTimeSource::Tick kReplyDue{10u};
    inline constexpr std::uint64_t kRequestLabel{0xCA11u};
    inline constexpr std::uint64_t kRequestValue{42u};
    inline constexpr std::uint64_t kRequestSequence{0x55u};
    inline constexpr std::uint64_t kReplyValue{kRequestValue + 1000u};

    inline Mailbox* mailbox{nullptr};

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

        std::printf("[idle] run=%u client_done=%d now=%llu\n",
                    context.shared->idle_runs,
                    context.shared->reply_received ? 1 : 0,
                    static_cast<unsigned long long>(ManualTimeSource::now()));
    }

    void server_step(ServerContext& context,
                     kernel::ThreadControl& control,
                     kernel::Event event)
    {
        if (context.shared == nullptr || mailbox == nullptr) {
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
                mailbox->wait_receive_current_until(kInitialReceiveDue);
            std::printf("[server] wait due=%llu armed=%d\n",
                        static_cast<unsigned long long>(kInitialReceiveDue),
                        context.shared->server_wait_armed ? 1 : 0);
            return;
        }

        if (mailbox->consume_receive_timeout_current(event)) {
            ++context.shared->server_runs;
            ++context.shared->server_timeouts;
            context.shared->server_timeout_seen = true;
            context.shared->server_rewait_armed =
                mailbox->wait_receive_current_until(kSecondReceiveDue);
            std::printf(
                "[server] recv-timeout count=%u now=%llu rewait=%d due=%llu\n",
                context.shared->server_timeouts,
                static_cast<unsigned long long>(ManualTimeSource::now()),
                context.shared->server_rewait_armed ? 1 : 0,
                static_cast<unsigned long long>(kSecondReceiveDue));
            return;
        }

        kernel::RuntimeMailboxRequest request{};
        if (!mailbox->receive_current(request)) {
            return;
        }

        ++context.shared->server_runs;
        context.shared->last_request_from =
            static_cast<std::uint64_t>(request.from.value);
        context.shared->last_request_label = request.label;
        context.shared->last_request_value = request.value;
        context.shared->last_request_sequence = request.sequence;
        context.shared->request_delivered =
            request.from == kClientId && request.label == kRequestLabel &&
            request.value == kRequestValue &&
            request.sequence == kRequestSequence;
        context.shared->reply_sent =
            mailbox->reply_current(request.from, request.sequence, kReplyValue);

        std::printf(
            "[server] recv from=%llu seq=%llu label=%llu value=%llu reply=%d\n",
            static_cast<unsigned long long>(request.from.value),
            static_cast<unsigned long long>(request.sequence),
            static_cast<unsigned long long>(request.label),
            static_cast<unsigned long long>(request.value),
            context.shared->reply_sent ? 1 : 0);
        control.finish();
    }

    void client_step(ClientContext& context,
                     kernel::ThreadControl& control,
                     kernel::Event event)
    {
        if (context.shared == nullptr || mailbox == nullptr) {
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
            context.shared->request_sent = mailbox->send_current(
                kRequestLabel, kRequestValue, kRequestSequence);
            context.shared->client_wait_armed =
                mailbox->wait_reply_current_until(kReplyDue);
            std::printf("[client] send=%d wait=%d due=%llu\n",
                        context.shared->request_sent ? 1 : 0,
                        context.shared->client_wait_armed ? 1 : 0,
                        static_cast<unsigned long long>(kReplyDue));
            return;
        }

        if (mailbox->consume_reply_timeout_current(event)) {
            ++context.shared->client_runs;
            context.shared->reply_timeout_seen = true;
            std::printf("[client] reply-timeout now=%llu\n",
                        static_cast<unsigned long long>(ManualTimeSource::now()));
            control.finish();
            return;
        }

        kernel::RuntimeMailboxReply reply{};
        if (!mailbox->receive_reply_current(reply)) {
            return;
        }

        ++context.shared->client_runs;
        context.shared->last_reply_from =
            static_cast<std::uint64_t>(reply.from.value);
        context.shared->last_reply_sequence = reply.sequence;
        context.shared->last_reply_value = reply.value;
        context.shared->reply_received =
            reply.from == kServerId && reply.sequence == kRequestSequence &&
            reply.value == kReplyValue;

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
                shared.trace_server_bootstrap_ok =
                    shared.trace_server_bootstrap_ok ||
                    (record->task_valid && record->task == kServerId &&
                     record->event_id == kernel::EventId::user0 &&
                     record->value == kServerBootstrapPayload && record->ok);
                shared.trace_client_bootstrap_ok =
                    shared.trace_client_bootstrap_ok ||
                    (record->task_valid && record->task == kClientId &&
                     record->event_id == kernel::EventId::user0 &&
                     record->value == kClientBootstrapPayload && record->ok);
                break;
            case kernel::RuntimeTraceKind::tick:
                shared.trace_tick_ok =
                    shared.trace_tick_ok ||
                    (!record->task_valid &&
                     record->event_id == kernel::EventId::tick &&
                     record->value >= 1u && record->ok);
                break;
            case kernel::RuntimeTraceKind::idle_bootstrap:
                shared.trace_idle_ok =
                    shared.trace_idle_ok ||
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
}

int main()
{
    demo::ManualTimeSource::reset();

    demo::Registry registry{};
    demo::Caps caps{};
    auto created = kernel::make_scheduler<demo::Config>(registry, caps);
    auto running = kernel::start(std::move(created));
    demo::RuntimeTrace runtime_trace{};
    demo::SharedState shared{};
    kernel::RuntimeBridge runtime{
        running,
        demo::kIdleId,
        demo::make_idle_event(),
        &runtime_trace,
    };
    demo::Mailbox mailbox{running, demo::kServerId};
    demo::mailbox = &mailbox;

    auto& idle = registry.get<demo::IdleTask>();
    idle.context.shared = &shared;

    auto& server = registry.get<demo::ServerTask>();
    server.context.shared = &shared;

    auto& client = registry.get<demo::ClientTask>();
    client.context.shared = &shared;

    while (running.run_once()) {
    }

    shared.mailbox_valid =
        mailbox.valid() && mailbox.server() == demo::kServerId;
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

    const bool trace_ok = shared.trace_server_bootstrap_ok &&
                          shared.trace_client_bootstrap_ok &&
                          shared.trace_tick_ok && shared.trace_idle_ok;
    const bool ok = shared.mailbox_valid && shared.server_bootstrapped &&
                    shared.client_bootstrapped && shared.server_wait_armed &&
                    shared.server_timeout_seen && shared.server_rewait_armed &&
                    shared.server_timeouts == 1u && shared.request_sent &&
                    shared.request_delivered && shared.reply_sent &&
                    shared.client_wait_armed && shared.reply_received &&
                    !shared.reply_timeout_seen && shared.idle_after_finish &&
                    mailbox.pending_requests() == 0u &&
                    mailbox.pending_replies() == 0u &&
                    !mailbox.receive_waiting() &&
                    mailbox.reply_waiters() == 0u &&
                    shared.last_request_from ==
                        static_cast<std::uint64_t>(demo::kClientId.value) &&
                    shared.last_request_label == demo::kRequestLabel &&
                    shared.last_request_value == demo::kRequestValue &&
                    shared.last_request_sequence == demo::kRequestSequence &&
                    shared.last_reply_from ==
                        static_cast<std::uint64_t>(demo::kServerId.value) &&
                    shared.last_reply_sequence == demo::kRequestSequence &&
                    shared.last_reply_value == demo::kReplyValue &&
                    shared.idle_runs >= 1u && shared.server_runs >= 3u &&
                    shared.client_runs >= 2u && trace_ok;

    std::printf(
        "[runtime-mailbox-demo] ok=%d valid=%d server_boot=%d client_boot=%d recv_wait=%d recv_timeout=%d send=%d recv=%d reply_send=%d reply_wait=%d reply_recv=%d idle=%d pending_req=%zu pending_reply=%zu reply_waiters=%zu loops=%llu\n",
        ok ? 1 : 0,
        shared.mailbox_valid ? 1 : 0,
        shared.server_bootstrapped ? 1 : 0,
        shared.client_bootstrapped ? 1 : 0,
        shared.server_wait_armed ? 1 : 0,
        shared.server_timeout_seen ? 1 : 0,
        shared.request_sent ? 1 : 0,
        shared.request_delivered ? 1 : 0,
        shared.reply_sent ? 1 : 0,
        shared.client_wait_armed ? 1 : 0,
        shared.reply_received ? 1 : 0,
        shared.idle_after_finish ? 1 : 0,
        mailbox.pending_requests(),
        mailbox.pending_replies(),
        mailbox.reply_waiters(),
        static_cast<unsigned long long>(loops));
    std::printf(
        "[runtime-mailbox-values] request_from=%llu label=%llu value=%llu seq=%llu reply_from=%llu reply=%llu server_timeouts=%u reply_timeout=%d idle_runs=%u server_runs=%u client_runs=%u\n",
        static_cast<unsigned long long>(shared.last_request_from),
        static_cast<unsigned long long>(shared.last_request_label),
        static_cast<unsigned long long>(shared.last_request_value),
        static_cast<unsigned long long>(shared.last_request_sequence),
        static_cast<unsigned long long>(shared.last_reply_from),
        static_cast<unsigned long long>(shared.last_reply_value),
        shared.server_timeouts,
        shared.reply_timeout_seen ? 1 : 0,
        shared.idle_runs,
        shared.server_runs,
        shared.client_runs);
    std::printf(
        "[runtime-mailbox-trace] ok=%d server_boot=%d client_boot=%d tick=%d idle=%d\n",
        trace_ok ? 1 : 0,
        shared.trace_server_bootstrap_ok ? 1 : 0,
        shared.trace_client_bootstrap_ok ? 1 : 0,
        shared.trace_tick_ok ? 1 : 0,
        shared.trace_idle_ok ? 1 : 0);
    return ok ? 0 : 1;
}
