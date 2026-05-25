#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string_view>

import kernel.capabilities;
import kernel.config;
import kernel.eda;
import kernel.evt;
import kernel.runtime_bridge;
import kernel.runtime_mailbox;
import kernel.scheduler;
import kernel.task_message_runtime_service;
import kernel.task_message_service_pump;
import kernel.task_message_session_dispatch;
import kernel.task_message_session_endpoint;
import kernel.task_message_session_ownership_corridor;
import kernel.task_message_session_protocol_schema;
import kernel.task_message_session_service;
import kernel.task_message_session_service_loop;
import kernel.task_state;
import kernel.task_syscall_table;
import kernel.thread;
import semantic.core;

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

    inline constexpr std::size_t kCompletionCount{4u};
    inline constexpr std::size_t kDrainBudget{2u};
    inline constexpr std::uint32_t kServerBootstrapPayload{1u};
    inline constexpr std::uint32_t kClientBootstrapPayload{2u};
    inline constexpr std::uint32_t kIdlePayload{0xD9u};
    inline constexpr std::array<ManualTimeSource::Tick, 6> kWaitDuePlan{
        3u,
        7u,
        11u,
        15u,
        19u,
        23u,
    };
    inline constexpr std::array<ManualTimeSource::Tick, kCompletionCount>
        kReplyDuePlan{
            14u,
            18u,
            22u,
            26u,
        };
    inline constexpr std::uint64_t kBaseToken{0xC1u};
    inline constexpr std::uint64_t kBaseSequence{0x61u};
    inline constexpr std::array<std::uint64_t, kCompletionCount> kExpectedTokens{
        kBaseToken,
        kBaseToken + 1u,
        kBaseToken + 2u,
        kBaseToken + 3u,
    };
    inline constexpr std::array<std::uint64_t, kCompletionCount>
        kExpectedSequences{
            kBaseSequence,
            kBaseSequence + 1u,
            kBaseSequence + 2u,
            kBaseSequence + 3u,
        };

    inline constexpr std::uint64_t kServiceId{0x51u};
    inline constexpr std::uint64_t kGhostServiceId{0x99u};
    inline constexpr std::uint64_t kOpenPayload{0xAAu};
    inline constexpr std::uint64_t kRequestOperation{0x21u};
    inline constexpr std::uint64_t kRequestPayload{33u};
    inline constexpr std::uint64_t kCloseReason{0x77u};
    inline constexpr std::uint64_t kGhostOpenPayload{0xEEu};
    inline constexpr std::uint64_t kBaseSessionHandle{0x9000u};
    inline constexpr std::uint64_t kExpectedRequestValue{
        kBaseSessionHandle + kRequestOperation + kRequestPayload};
    inline constexpr std::uint64_t kExpectedCloseValue{kCloseReason + 1u};
    using ProtocolTrace = kernel::TaskMessageSessionProtocolTraceBuffer<8>;
    using ServerSessionProtocol =
        kernel::TaskMessageSessionProtocol<1, ProtocolTrace>;
    inline constexpr std::array<std::uint64_t, kCompletionCount>
        kExpectedCapabilityIds{
            kServiceId,
            kBaseSessionHandle,
            kBaseSessionHandle,
            kGhostServiceId,
        };
    inline constexpr std::array<std::uint64_t, kCompletionCount>
        kExpectedOperations{
            kernel::task_message_session_open_operation,
            kRequestOperation,
            kernel::task_message_session_close_operation,
            kernel::task_message_session_open_operation,
        };
    inline constexpr std::array<std::uint64_t, kCompletionCount>
        kExpectedPayloads{
            kOpenPayload,
            kRequestPayload,
            kCloseReason,
            kGhostOpenPayload,
        };
    inline constexpr std::array<kernel::TrapDisposition, kCompletionCount>
        kExpectedDispositions{
            kernel::TrapDisposition::handled,
            kernel::TrapDisposition::handled,
            kernel::TrapDisposition::handled,
            kernel::TrapDisposition::unsupported,
        };
    inline constexpr std::array<kernel::TrapError, kCompletionCount>
        kExpectedErrors{
            kernel::TrapError::none,
            kernel::TrapError::none,
            kernel::TrapError::none,
            kernel::TrapError::unsupported_service,
        };
    inline constexpr std::array<std::uint64_t, kCompletionCount>
        kExpectedReplyValues{
            kBaseSessionHandle,
            kExpectedRequestValue,
            kExpectedCloseValue,
            0u,
        };

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

    struct PendingClientCall {
        bool valid{false};
        std::uint64_t capability_id{0};
        std::uint64_t operation{0};
        std::uint64_t payload{0};
    };

    struct ClientCompletionRecord {
        bool timeout{false};
        kernel::TaskId owner{};
        kernel::TaskId reply_from{};
        kernel::TaskId reply_to{};
        std::uint64_t token{0};
        std::uint64_t request_sequence{0};
        std::uint64_t capability_id{0};
        std::uint64_t operation{0};
        std::uint64_t payload{0};
        std::uint64_t reply_value{0};
        kernel::TrapResult trap{
            .disposition = kernel::TrapDisposition::rejected,
            .error = kernel::TrapError::none,
            .value = 0,
        };
    };

    struct SharedState {
        bool mailbox_valid{false};
        bool message_dispatcher_valid{false};
        bool frame_store_valid{false};
        bool frame_bridge_valid{false};
        bool message_frame_bridge_valid{false};
        bool service_loop_valid{false};
        bool service_drain_valid{false};
        bool service_pump_valid{false};
        bool session_service_valid{false};
        bool client_syscalls_valid{false};
        bool server_bootstrapped{false};
        bool client_bootstrapped{false};
        bool open_issue_ok{false};
        bool open_kick_ok{false};
        bool request_issue_ok{false};
        bool request_kick_ok{false};
        bool close_issue_ok{false};
        bool close_kick_ok{false};
        bool ghost_issue_ok{false};
        bool ghost_kick_ok{false};
        bool finished{false};
        bool unexpected_timeout{false};
        std::uint32_t idle_runs{0};
        std::uint32_t server_runs{0};
        std::uint32_t client_runs{0};
        std::uint32_t server_timeouts{0};
        std::size_t completions_received{0};
        std::size_t total_served{0};
        std::size_t wait_arm_index{0};
        std::size_t wait_arm_successes{0};
        std::uint64_t active_session_handle{0};
        PendingClientCall pending_call{};
        std::array<ClientCompletionRecord, kCompletionCount> completions{};
    };

    struct EchoSessionProtocolState {
        std::uint32_t request_calls{0};
        std::uint32_t close_calls{0};
        std::uint64_t last_service_id{0};
        std::uint64_t last_session_handle{0};
        std::uint64_t last_open_payload{0};
        std::uint64_t last_request_operation{0};
        std::uint64_t last_request_payload{0};
        std::uint64_t last_close_reason{0};
        std::uint16_t last_channel_slot{
            kernel::task_message_session_channel_unmapped_slot};
        const char* last_operation_name{"unmapped"};
        const char* last_field_name{"payload"};
        const char* last_result_name{"value"};
        kernel::TaskMessageSessionProtocolSchemaViewKind last_view_kind{
            kernel::TaskMessageSessionProtocolSchemaViewKind::invalid};
        std::uint8_t last_field_count{0};
        bool last_schema_supported{false};
    };

    struct EchoSessionRequestOperation {
        EchoSessionProtocolState* state{nullptr};

        [[nodiscard]] kernel::TrapResult dispatch(
            kernel::TaskMessageSessionProtocolSemanticProjection request) const
            noexcept
        {
            ++state->request_calls;
            state->last_service_id = request.endpoint.service_id;
            state->last_session_handle = request.endpoint.session_handle;
            state->last_open_payload = request.endpoint.open_payload;
            state->last_request_operation = request.operation;
            state->last_request_payload = request.payload;
            state->last_channel_slot = request.endpoint.channel_slot;
            state->last_operation_name = request.descriptor.operation_name;
            state->last_field_name =
                request.field_count != 0u ? request.fields[0].name : "";
            state->last_result_name = request.result_name;
            state->last_view_kind = request.descriptor.view_kind;
            state->last_field_count = request.field_count;
            state->last_schema_supported = request.descriptor.supported;
            return kernel::task_message_session_endpoint_handled(
                request.endpoint.session_handle +
                request.operation +
                request.payload +
                request.endpoint.channel_slot);
        }
    };

    struct EchoSessionCloseHook {
        EchoSessionProtocolState* state{nullptr};

        [[nodiscard]] kernel::TrapResult dispatch(
            kernel::TaskMessageSessionEndpointCloseView close) const noexcept
        {
            ++state->close_calls;
            state->last_service_id = close.endpoint.service_id;
            state->last_session_handle = close.endpoint.session_handle;
            state->last_open_payload = close.endpoint.open_payload;
            state->last_close_reason = close.reason;
            state->last_channel_slot = close.endpoint.channel_slot;
            return kernel::task_message_session_endpoint_handled(
                close.reason + close.endpoint.channel_slot + 1u);
        }
    };

    struct EchoSessionAcceptorState {
        std::uint32_t accept_calls{0};
        std::uint64_t last_service_id{0};
        std::uint64_t last_session_handle{0};
        std::uint64_t last_open_payload{0};
        std::uint16_t last_channel_slot{
            kernel::task_message_session_channel_unmapped_slot};
    };

    struct EchoSessionAcceptor {
        EchoSessionAcceptorState* state{nullptr};
        std::array<ServerSessionProtocol, 2>* protocols{nullptr};

        [[nodiscard]] kernel::TrapResult accept(
            kernel::TaskMessageSessionEndpoint endpoint,
            kernel::TaskMessageSessionEndpointBinding binding) const noexcept
        {
            ++state->accept_calls;
            state->last_service_id = endpoint.service_id;
            state->last_session_handle = endpoint.session_handle;
            state->last_open_payload = endpoint.open_payload;
            state->last_channel_slot = endpoint.channel_slot;

            if (!binding.valid() || protocols == nullptr ||
                endpoint.channel_slot >= protocols->size()) {
                return rejected_invalid_argument();
            }

            kernel::bind_task_message_session_endpoint(
                binding,
                (*protocols)[endpoint.channel_slot]);
            return kernel::task_message_session_endpoint_handled(
                endpoint.session_handle + endpoint.channel_slot);
        }

    private:
        [[nodiscard]] static constexpr kernel::TrapResult
        rejected_invalid_argument() noexcept
        {
            return kernel::task_message_session_endpoint_invalid_argument();
        }
    };

    inline constexpr auto kEchoRequestSchema =
        kernel::task_message_session_protocol_schema_entry(
            kRequestOperation,
            "echo-request",
            "request-value",
            "reply-value");

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
    using MessageDispatcher =
        kernel::TaskMessageDispatcher<TaskMessages, MessageTable>;
    using ServiceLoop = kernel::TaskMessageServiceLoop<MessageDispatcher>;
    using ServiceDrain = kernel::TaskMessageServiceDrain<ServiceLoop>;
    using ServicePump = kernel::TaskMessageServicePump<ServiceDrain>;
    using SessionTrace = kernel::TaskMessageSessionDispatchTraceBuffer<8>;
    using SessionAcceptorTrace =
        kernel::TaskMessageSessionServiceAcceptorTraceBuffer<8>;
    using ServiceTrace = kernel::TaskMessageSessionServiceTraceBuffer<8>;
    using ServerSessionAcceptor =
        kernel::TaskMessageSessionServiceAcceptor<2, SessionAcceptorTrace>;
    using ServerSessionDispatcher =
        kernel::TaskMessageSessionDispatcher<1, 2, SessionTrace>;
    using ServerSessionService = kernel::TaskMessageSessionService<
        ServicePump,
        ServerSessionDispatcher,
        ServerSessionAcceptor,
        ServiceTrace>;
    using SyscallTable = kernel::TaskSyscallTable<1>;
    using FrameBridge =
        kernel::TaskSyscallFrameBridge<SyscallTable, MessageSyscallFrame>;
    using MessageFrameBridge = kernel::TaskMessageSyscallFrameBridge<
        FrameStore,
        kernel::TaskSyscallFramePort<MessageSyscallFrame>,
        kernel::TaskMessageSyscallFrameResultAdapter<MessageSyscallFrame>>;
    using CallerAdapter =
        kernel::TaskMessageSyscallFrameCallAdapter<MessageSyscallFrame>;
    using FrameCaller =
        kernel::TaskMessageSyscallFrameCaller<TaskMessages,
                                              FrameStore,
                                              CallerAdapter>;
    using ClientTransport = kernel::TaskMessageSyscallClient<FrameCaller>;
    using PumpTrace = kernel::TaskMessageSyscallPumpTraceBuffer<12>;
    using ClientPump =
        kernel::TaskMessageSyscallPump<ClientTransport, 8, 8, PumpTrace>;
    using ClientServices = kernel::TaskMessageRuntimeServiceFacade<ClientPump>;
    using ClientRuntime = kernel::TaskMessageRuntimeApi<ClientServices>;
    using ClientSyscalls = kernel::TaskMessageSyscallApi<ClientRuntime>;

    inline constexpr auto kIdleId = Registry::id_of<IdleTask>();
    inline constexpr auto kServerId = Registry::id_of<ServerTask>();
    inline constexpr auto kClientId = Registry::id_of<ClientTask>();

    inline ServerSessionService* server_session_service{nullptr};
    inline ClientSyscalls* client_syscalls{nullptr};

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

    [[nodiscard]] constexpr bool same_text(const char* actual,
                                           std::string_view expected) noexcept
    {
        return actual != nullptr && std::string_view{actual} == expected;
    }

    [[nodiscard]] constexpr bool trap_result_matches(
        const kernel::TrapResult& result,
        kernel::TrapDisposition disposition,
        kernel::TrapError error,
        std::uint64_t value = 0u) noexcept
    {
        return result.disposition == disposition && result.error == error &&
               result.value == value;
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

    [[nodiscard]] bool store_completion(
        SharedState& shared,
        const ClientSyscalls::completion_type& completion) noexcept
    {
        if (!shared.pending_call.valid ||
            shared.completions_received >= shared.completions.size()) {
            return false;
        }

        auto& record = shared.completions[shared.completions_received++];
        record = ClientCompletionRecord{
            .timeout = completion.timeout,
            .owner = completion.owner,
            .reply_from = completion.reply.from,
            .reply_to = completion.reply.to,
            .token = completion.token,
            .request_sequence = completion.request_sequence,
            .capability_id = shared.pending_call.capability_id,
            .operation = shared.pending_call.operation,
            .payload = shared.pending_call.payload,
            .reply_value = completion.trap.value,
            .trap = completion.trap,
        };
        shared.pending_call = PendingClientCall{};
        return true;
    }

    [[nodiscard]] bool issue_capability_call(SharedState& shared,
                                             std::uint64_t capability_id,
                                             std::uint64_t operation,
                                             std::uint64_t payload,
                                             ManualTimeSource::Tick due) noexcept
    {
        if (client_syscalls == nullptr || !client_syscalls->valid() ||
            shared.pending_call.valid) {
            return false;
        }

        const bool issued = client_syscalls->sys_capability_call(
            capability_id, operation, payload, due);
        if (!issued) {
            return false;
        }

        shared.pending_call = PendingClientCall{
            .valid = true,
            .capability_id = capability_id,
            .operation = operation,
            .payload = payload,
        };
        return true;
    }

    [[nodiscard]] bool issue_open(SharedState& shared) noexcept
    {
        shared.open_issue_ok = issue_capability_call(shared,
                                                     kServiceId,
                                                     kernel::task_message_session_open_operation,
                                                     kOpenPayload,
                                                     kReplyDuePlan[0]);
        shared.open_kick_ok = shared.open_issue_ok && client_syscalls->kick();
        std::printf(
            "[client] open issue=%d kick=%d service=%llu payload=%llu due=%llu\n",
            shared.open_issue_ok ? 1 : 0,
            shared.open_kick_ok ? 1 : 0,
            static_cast<unsigned long long>(kServiceId),
            static_cast<unsigned long long>(kOpenPayload),
            static_cast<unsigned long long>(kReplyDuePlan[0]));
        return shared.open_kick_ok;
    }

    [[nodiscard]] bool issue_request(SharedState& shared) noexcept
    {
        shared.request_issue_ok = issue_capability_call(shared,
                                                        shared.active_session_handle,
                                                        kRequestOperation,
                                                        kRequestPayload,
                                                        kReplyDuePlan[1]);
        shared.request_kick_ok =
            shared.request_issue_ok && client_syscalls->kick();
        std::printf(
            "[client] request issue=%d kick=%d handle=%llu op=%llu payload=%llu due=%llu\n",
            shared.request_issue_ok ? 1 : 0,
            shared.request_kick_ok ? 1 : 0,
            static_cast<unsigned long long>(shared.active_session_handle),
            static_cast<unsigned long long>(kRequestOperation),
            static_cast<unsigned long long>(kRequestPayload),
            static_cast<unsigned long long>(kReplyDuePlan[1]));
        return shared.request_kick_ok;
    }

    [[nodiscard]] bool issue_close(SharedState& shared) noexcept
    {
        shared.close_issue_ok = issue_capability_call(shared,
                                                      shared.active_session_handle,
                                                      kernel::task_message_session_close_operation,
                                                      kCloseReason,
                                                      kReplyDuePlan[2]);
        shared.close_kick_ok = shared.close_issue_ok && client_syscalls->kick();
        std::printf(
            "[client] close issue=%d kick=%d handle=%llu reason=%llu due=%llu\n",
            shared.close_issue_ok ? 1 : 0,
            shared.close_kick_ok ? 1 : 0,
            static_cast<unsigned long long>(shared.active_session_handle),
            static_cast<unsigned long long>(kCloseReason),
            static_cast<unsigned long long>(kReplyDuePlan[2]));
        return shared.close_kick_ok;
    }

    [[nodiscard]] bool issue_ghost_open(SharedState& shared) noexcept
    {
        shared.ghost_issue_ok = issue_capability_call(shared,
                                                      kGhostServiceId,
                                                      kernel::task_message_session_open_operation,
                                                      kGhostOpenPayload,
                                                      kReplyDuePlan[3]);
        shared.ghost_kick_ok =
            shared.ghost_issue_ok && client_syscalls->kick();
        std::printf(
            "[client] ghost-open issue=%d kick=%d service=%llu payload=%llu due=%llu\n",
            shared.ghost_issue_ok ? 1 : 0,
            shared.ghost_kick_ok ? 1 : 0,
            static_cast<unsigned long long>(kGhostServiceId),
            static_cast<unsigned long long>(kGhostOpenPayload),
            static_cast<unsigned long long>(kReplyDuePlan[3]));
        return shared.ghost_kick_ok;
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
        std::printf("[idle] run=%u completions=%zu finished=%d now=%llu\n",
                    context.shared->idle_runs,
                    context.shared->completions_received,
                    context.shared->finished ? 1 : 0,
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

        if (context.shared == nullptr || server_session_service == nullptr ||
            !server_session_service->valid()) {
            return;
        }

        const auto due = next_server_due(*context.shared);
        const auto result = server_session_service->step(
            event, kDrainBudget, due);
        if (result.reason == kernel::TaskMessageServicePumpReason::none &&
            !result.progressed && !result.bootstrap_consumed &&
            !result.hold_ready) {
            return;
        }

        ++context.shared->server_runs;
        context.shared->total_served += result.raw.drain.served;
        mark_wait_arm(*context.shared, result.wait_armed);

        if (result.reason == kernel::TaskMessageServicePumpReason::timeout) {
            ++context.shared->server_timeouts;
            std::printf("[server] timeout count=%u due=%llu armed=%d\n",
                        context.shared->server_timeouts,
                        static_cast<unsigned long long>(due),
                        result.wait_armed ? 1 : 0);
            return;
        }

        if (result.reason == kernel::TaskMessageServicePumpReason::bootstrap) {
            std::printf("[server] bootstrap due=%llu armed=%d\n",
                        static_cast<unsigned long long>(due),
                        result.wait_armed ? 1 : 0);
            return;
        }

        std::printf(
            "[server] step service=%s reason=%s served=%zu due=%llu armed=%d hold=%d sessions=%zu channels=%zu\n",
            server_session_service->service_name(),
            kernel::task_message_service_pump_reason_name(result.reason),
            result.raw.drain.served,
            static_cast<unsigned long long>(due),
            result.wait_armed ? 1 : 0,
            result.hold_ready ? 1 : 0,
            result.active_sessions,
            result.active_channels);
    }

    void client_step(ClientContext& context,
                     kernel::ThreadControl&,
                     kernel::Event event)
    {
        if (event.id == kernel::EventId::init) {
            std::printf("[client] init\n");
            return;
        }

        if (context.shared == nullptr || client_syscalls == nullptr ||
            !client_syscalls->valid()) {
            return;
        }

        if (event.id == kernel::EventId::user0 &&
            kernel::payload_u32(event) == kClientBootstrapPayload) {
            (void)issue_open(*context.shared);
            return;
        }

        (void)client_syscalls->step(event);

        ClientSyscalls::completion_type completion{};
        while (client_syscalls->receive_completion(completion)) {
            ++context.shared->client_runs;
            if (!store_completion(*context.shared, completion)) {
                context.shared->unexpected_timeout = true;
                return;
            }

            const auto index = context.shared->completions_received - 1u;
            const auto& record = context.shared->completions[index];
            std::printf(
                "[client] completion idx=%zu cap=%llu op=%llu payload=%llu value=%llu disp=%s err=%s timeout=%d\n",
                index,
                static_cast<unsigned long long>(record.capability_id),
                static_cast<unsigned long long>(record.operation),
                static_cast<unsigned long long>(record.payload),
                static_cast<unsigned long long>(record.reply_value),
                kernel::trap_disposition_name(record.trap.disposition),
                kernel::trap_error_name(record.trap.error),
                record.timeout ? 1 : 0);

            if (record.timeout) {
                context.shared->unexpected_timeout = true;
                context.shared->finished = true;
                return;
            }

            switch (index) {
            case 0u:
                context.shared->active_session_handle = record.reply_value;
                (void)issue_request(*context.shared);
                break;
            case 1u:
                (void)issue_close(*context.shared);
                break;
            case 2u:
                context.shared->active_session_handle = 0u;
                (void)issue_ghost_open(*context.shared);
                break;
            case 3u:
                context.shared->finished = true;
                break;
            default:
                break;
            }
        }
    }

    [[nodiscard]] bool inspect_dispatch_trace(SessionTrace& trace) noexcept
    {
        const auto* open = trace.at(0u);
        const auto* request = trace.at(1u);
        const auto* close = trace.at(2u);
        const auto* ghost = trace.at(3u);
        if (open == nullptr || request == nullptr || close == nullptr ||
            ghost == nullptr) {
            return false;
        }

        for (std::size_t index = 0; index < trace.size(); ++index) {
            const auto* record = trace.at(index);
            if (record == nullptr) {
                continue;
            }

            std::printf(
                "[session-dispatch-trace] seq=%llu action=%s service=%llu name=%s handle=%llu service_slot=%u session_slot=%u matched=%d handler=%d found=%d allocated=%d closed=%d value=%llu err=%s\n",
                static_cast<unsigned long long>(record->sequence),
                kernel::task_message_session_action_kind_name(record->action),
                static_cast<unsigned long long>(record->service_id),
                record->service_name,
                static_cast<unsigned long long>(record->session_handle),
                static_cast<unsigned>(record->service_slot),
                static_cast<unsigned>(record->session_slot),
                record->matched ? 1 : 0,
                record->handler_valid ? 1 : 0,
                record->session_found ? 1 : 0,
                record->session_allocated ? 1 : 0,
                record->session_closed ? 1 : 0,
                static_cast<unsigned long long>(record->value),
                kernel::trap_error_name(record->error));
        }

        return trace.size() == 4u &&
               open->sequence == 1u &&
               open->action == kernel::TaskMessageSessionActionKind::open &&
               open->service_id == kServiceId &&
               same_text(open->service_name, "echo-session"sv) &&
               open->session_handle == kBaseSessionHandle &&
               open->service_slot == 0u && open->session_slot == 0u &&
               open->matched && open->handler_valid &&
               open->session_allocated && !open->session_closed &&
               trap_result_matches(kernel::TrapResult{
                                       .disposition = open->disposition,
                                       .error = open->error,
                                       .value = open->value,
                                   },
                                   kernel::TrapDisposition::handled,
                                   kernel::TrapError::none,
                                   kBaseSessionHandle) &&
               request->sequence == 2u &&
               request->action ==
                   kernel::TaskMessageSessionActionKind::request &&
               request->service_id == kServiceId &&
               request->session_handle == kBaseSessionHandle &&
               request->session_found && !request->session_allocated &&
               !request->session_closed &&
               trap_result_matches(kernel::TrapResult{
                                       .disposition = request->disposition,
                                       .error = request->error,
                                       .value = request->value,
                                   },
                                   kernel::TrapDisposition::handled,
                                   kernel::TrapError::none,
                                   kExpectedRequestValue) &&
               close->sequence == 3u &&
               close->action == kernel::TaskMessageSessionActionKind::close &&
               close->service_id == kServiceId &&
               close->session_handle == kBaseSessionHandle &&
               close->session_found && close->session_closed &&
               trap_result_matches(kernel::TrapResult{
                                       .disposition = close->disposition,
                                       .error = close->error,
                                       .value = close->value,
                                   },
                                   kernel::TrapDisposition::handled,
                                   kernel::TrapError::none,
                                   kExpectedCloseValue) &&
               ghost->sequence == 4u &&
               ghost->action == kernel::TaskMessageSessionActionKind::open &&
               ghost->service_id == kGhostServiceId &&
               same_text(ghost->service_name, "unmapped"sv) &&
               ghost->session_handle == 0u &&
               !ghost->matched && !ghost->handler_valid &&
               !ghost->session_allocated && !ghost->session_closed &&
               !ghost->session_found &&
               ghost->service_slot ==
                   kernel::task_message_session_service_unmapped_slot &&
               ghost->session_slot ==
                   kernel::task_message_session_unmapped_slot &&
               trap_result_matches(kernel::TrapResult{
                                       .disposition = ghost->disposition,
                                       .error = ghost->error,
                                       .value = ghost->value,
                                   },
                                   kernel::TrapDisposition::unsupported,
                                   kernel::TrapError::unsupported_service);
    }

    [[nodiscard]] bool inspect_acceptor_trace(
        SessionAcceptorTrace& trace) noexcept
    {
        const auto* open = trace.at(0u);
        const auto* request = trace.at(1u);
        const auto* close = trace.at(2u);
        if (open == nullptr || request == nullptr || close == nullptr) {
            return false;
        }

        for (std::size_t index = 0; index < trace.size(); ++index) {
            const auto* record = trace.at(index);
            if (record == nullptr) {
                continue;
            }

            std::printf(
                "[session-acceptor-trace] seq=%llu action=%s service=%llu name=%s handle=%llu op=%llu payload=%llu channel_slot=%u acceptor=%d found=%d bound=%d closed=%d value=%llu err=%s\n",
                static_cast<unsigned long long>(record->sequence),
                kernel::task_message_session_action_kind_name(record->action),
                static_cast<unsigned long long>(record->service_id),
                record->service_name,
                static_cast<unsigned long long>(record->session_handle),
                static_cast<unsigned long long>(record->operation),
                static_cast<unsigned long long>(record->payload),
                static_cast<unsigned>(record->channel_slot),
                record->acceptor_valid ? 1 : 0,
                record->channel_found ? 1 : 0,
                record->channel_bound ? 1 : 0,
                record->channel_closed ? 1 : 0,
                static_cast<unsigned long long>(record->value),
                kernel::trap_error_name(record->error));
        }

        return trace.size() == 3u &&
               open->sequence == 1u &&
               open->action == kernel::TaskMessageSessionActionKind::open &&
               open->service_id == kServiceId &&
               same_text(open->service_name, "echo-session"sv) &&
               open->session_handle == kBaseSessionHandle &&
               open->payload == kOpenPayload &&
               open->channel_slot == 0u &&
               open->acceptor_valid && open->channel_bound &&
               !open->channel_found && !open->channel_closed &&
               trap_result_matches(kernel::TrapResult{
                                       .disposition = open->disposition,
                                       .error = open->error,
                                       .value = open->value,
                                   },
                                   kernel::TrapDisposition::handled,
                                   kernel::TrapError::none,
                                   kBaseSessionHandle) &&
               request->sequence == 2u &&
               request->action ==
                   kernel::TaskMessageSessionActionKind::request &&
               request->service_id == kServiceId &&
               request->session_handle == kBaseSessionHandle &&
               request->operation == kRequestOperation &&
               request->payload == kRequestPayload &&
               request->channel_slot == 0u &&
               request->acceptor_valid && request->channel_found &&
               !request->channel_bound && !request->channel_closed &&
               trap_result_matches(kernel::TrapResult{
                                       .disposition = request->disposition,
                                       .error = request->error,
                                       .value = request->value,
                                   },
                                   kernel::TrapDisposition::handled,
                                   kernel::TrapError::none,
                                   kExpectedRequestValue) &&
               close->sequence == 3u &&
               close->action == kernel::TaskMessageSessionActionKind::close &&
               close->service_id == kServiceId &&
               close->session_handle == kBaseSessionHandle &&
               close->payload == kCloseReason &&
               close->channel_slot == 0u &&
               close->acceptor_valid && close->channel_found &&
               !close->channel_bound && close->channel_closed &&
               trap_result_matches(kernel::TrapResult{
                                       .disposition = close->disposition,
                                       .error = close->error,
                                       .value = close->value,
                                   },
                                   kernel::TrapDisposition::handled,
                                   kernel::TrapError::none,
                                   kExpectedCloseValue);
    }

    [[nodiscard]] bool inspect_protocol_trace(
        ProtocolTrace& trace) noexcept
    {
        const auto* request = trace.at(0u);
        const auto* close = trace.at(1u);
        if (request == nullptr || close == nullptr) {
            return false;
        }

        for (std::size_t index = 0; index < trace.size(); ++index) {
            const auto* record = trace.at(index);
            if (record == nullptr) {
                continue;
            }

            std::printf(
                "[session-service-loop-protocol-trace] seq=%llu kind=%s service=%llu name=%s handle=%llu open=%llu slot=%u op=%llu op_name=%s payload=%llu matched=%d handler=%d close=%d value=%llu err=%s\n",
                static_cast<unsigned long long>(record->sequence),
                kernel::task_message_session_protocol_trace_kind_name(
                    record->kind),
                static_cast<unsigned long long>(record->service_id),
                record->service_name,
                static_cast<unsigned long long>(record->session_handle),
                static_cast<unsigned long long>(record->open_payload),
                static_cast<unsigned>(record->channel_slot),
                static_cast<unsigned long long>(record->operation),
                record->operation_name,
                static_cast<unsigned long long>(record->payload),
                record->matched ? 1 : 0,
                record->handler_valid ? 1 : 0,
                record->close_handler_valid ? 1 : 0,
                static_cast<unsigned long long>(record->value),
                kernel::trap_error_name(record->error));
        }

        return trace.size() == 2u &&
               request->sequence == 1u &&
               request->kind ==
                   kernel::TaskMessageSessionProtocolTraceKind::request &&
               request->service_id == kServiceId &&
               same_text(request->service_name, "echo-session"sv) &&
               request->session_handle == kBaseSessionHandle &&
               request->open_payload == kOpenPayload &&
               request->channel_slot == 0u &&
               request->operation == kRequestOperation &&
               same_text(request->operation_name, "echo-request"sv) &&
               request->payload == kRequestPayload &&
               request->slot == 0u &&
               request->matched && request->handler_valid &&
               !request->close_handler_valid &&
               trap_result_matches(kernel::TrapResult{
                                       .disposition = request->disposition,
                                       .error = request->error,
                                       .value = request->value,
                                   },
                                   kernel::TrapDisposition::handled,
                                   kernel::TrapError::none,
                                   kExpectedRequestValue) &&
               close->sequence == 2u &&
               close->kind ==
                   kernel::TaskMessageSessionProtocolTraceKind::close &&
               close->service_id == kServiceId &&
               same_text(close->service_name, "echo-session"sv) &&
               close->session_handle == kBaseSessionHandle &&
               close->open_payload == kOpenPayload &&
               close->channel_slot == 0u &&
               close->operation == kernel::task_message_session_close_operation &&
               same_text(close->operation_name, "close"sv) &&
               close->payload == kCloseReason &&
               close->slot ==
                   kernel::task_message_session_protocol_unmapped_slot &&
               !close->matched && !close->handler_valid &&
               close->close_handler_valid &&
               trap_result_matches(kernel::TrapResult{
                                       .disposition = close->disposition,
                                       .error = close->error,
                                       .value = close->value,
                                   },
                                   kernel::TrapDisposition::handled,
                                   kernel::TrapError::none,
                                   kExpectedCloseValue);
    }

    [[nodiscard]] bool inspect_service_trace(ServiceTrace& trace) noexcept
    {
        const auto* bootstrap = trace.at(0u);
        const auto* timeout = trace.at(1u);
        const auto* open = trace.at(2u);
        const auto* request = trace.at(3u);
        const auto* close = trace.at(4u);
        const auto* ghost = trace.at(5u);
        if (bootstrap == nullptr || timeout == nullptr || open == nullptr ||
            request == nullptr || close == nullptr || ghost == nullptr) {
            return false;
        }

        for (std::size_t index = 0; index < trace.size(); ++index) {
            const auto* record = trace.at(index);
            if (record == nullptr) {
                continue;
            }

            std::printf(
                "[session-service-trace] seq=%llu reason=%s event=%u value=%llu due=%llu budget=%zu served=%zu sessions=%zu channels=%zu progressed=%d bootstrap=%d armed=%d hold=%d accepted=%d handled=%d replied=%d reply=%llu\n",
                static_cast<unsigned long long>(record->sequence),
                kernel::task_message_service_pump_reason_name(record->reason),
                static_cast<unsigned>(record->event_id),
                static_cast<unsigned long long>(record->event_value),
                static_cast<unsigned long long>(record->due),
                record->budget,
                record->served,
                record->active_sessions,
                record->active_channels,
                record->progressed ? 1 : 0,
                record->bootstrap_consumed ? 1 : 0,
                record->wait_armed ? 1 : 0,
                record->hold_ready ? 1 : 0,
                record->dispatch_accepted ? 1 : 0,
                record->dispatch_handled ? 1 : 0,
                record->dispatch_replied ? 1 : 0,
                static_cast<unsigned long long>(record->reply_value));
        }

        return trace.size() == 6u &&
               bootstrap->sequence == 1u &&
               bootstrap->reason ==
                   kernel::TaskMessageServicePumpReason::bootstrap &&
               bootstrap->event_id == kernel::EventId::user0 &&
               bootstrap->event_value == kServerBootstrapPayload &&
               bootstrap->due == kWaitDuePlan[0] &&
               bootstrap->budget == kDrainBudget &&
               bootstrap->served == 0u &&
               bootstrap->active_sessions == 0u &&
               bootstrap->active_channels == 0u &&
               bootstrap->progressed &&
               bootstrap->bootstrap_consumed &&
               bootstrap->wait_armed &&
               !bootstrap->hold_ready &&
               !bootstrap->dispatch_accepted &&
               timeout->sequence == 2u &&
               timeout->reason ==
                   kernel::TaskMessageServicePumpReason::timeout &&
               timeout->event_id == kernel::EventId::sync &&
               timeout->event_value ==
                   kernel::runtime_mailbox_receive_timeout_code &&
               timeout->due == kWaitDuePlan[1] &&
               timeout->budget == kDrainBudget &&
               timeout->served == 0u &&
               timeout->active_sessions == 0u &&
               timeout->active_channels == 0u &&
               timeout->progressed &&
               !timeout->bootstrap_consumed &&
               timeout->wait_armed &&
               !timeout->hold_ready &&
               !timeout->dispatch_accepted &&
               open->sequence == 3u &&
               open->reason ==
                   kernel::TaskMessageServicePumpReason::queue_empty &&
               open->event_id == kernel::EventId::message &&
               open->event_value == kernel::runtime_mailbox_receive_ready_code &&
               open->due == kWaitDuePlan[2] &&
               open->budget == kDrainBudget &&
               open->served == 1u &&
               open->active_sessions == 1u &&
               open->active_channels == 1u &&
               open->progressed &&
               !open->bootstrap_consumed &&
               open->wait_armed &&
               !open->hold_ready &&
               open->dispatch_accepted &&
               open->dispatch_handled &&
               open->dispatch_replied &&
               open->reply_value == kBaseSessionHandle &&
               request->sequence == 4u &&
               request->reason ==
                   kernel::TaskMessageServicePumpReason::queue_empty &&
               request->event_id == kernel::EventId::message &&
               request->event_value ==
                   kernel::runtime_mailbox_receive_ready_code &&
               request->due == kWaitDuePlan[3] &&
               request->budget == kDrainBudget &&
               request->served == 1u &&
               request->active_sessions == 1u &&
               request->active_channels == 1u &&
               request->progressed &&
               request->wait_armed &&
               !request->hold_ready &&
               request->dispatch_accepted &&
               request->dispatch_handled &&
               request->dispatch_replied &&
               request->reply_value == kExpectedRequestValue &&
               close->sequence == 5u &&
               close->reason ==
                   kernel::TaskMessageServicePumpReason::queue_empty &&
               close->event_id == kernel::EventId::message &&
               close->event_value ==
                   kernel::runtime_mailbox_receive_ready_code &&
               close->due == kWaitDuePlan[4] &&
               close->budget == kDrainBudget &&
               close->served == 1u &&
               close->active_sessions == 0u &&
               close->active_channels == 0u &&
               close->progressed &&
               close->wait_armed &&
               !close->hold_ready &&
               close->dispatch_accepted &&
               close->dispatch_handled &&
               close->dispatch_replied &&
               close->reply_value == kExpectedCloseValue &&
               ghost->sequence == 6u &&
               ghost->reason ==
                   kernel::TaskMessageServicePumpReason::queue_empty &&
               ghost->event_id == kernel::EventId::message &&
               ghost->event_value ==
                   kernel::runtime_mailbox_receive_ready_code &&
               ghost->due == kWaitDuePlan[5] &&
               ghost->budget == kDrainBudget &&
               ghost->served == 1u &&
               ghost->active_sessions == 0u &&
               ghost->active_channels == 0u &&
               ghost->progressed &&
               ghost->wait_armed &&
               !ghost->hold_ready &&
               ghost->dispatch_accepted &&
               ghost->dispatch_handled &&
               ghost->dispatch_replied &&
               ghost->reply_value == 0u;
    }

    [[nodiscard]] bool inspect_pump_trace(PumpTrace& trace) noexcept
    {
        if (trace.size() != 8u) {
            return false;
        }

        for (std::size_t index = 0; index < trace.size(); ++index) {
            const auto* record = trace.at(index);
            if (record == nullptr) {
                return false;
            }

            std::printf(
                "[session-service-loop-pump-trace] seq=%llu kind=%s owner=%llu token=%llu req_seq=%llu due=%llu ok=%d timeout=%d value=%llu disp=%s err=%s\n",
                static_cast<unsigned long long>(record->sequence),
                kernel::task_message_syscall_pump_trace_kind_name(record->kind),
                static_cast<unsigned long long>(record->owner.value),
                static_cast<unsigned long long>(record->token),
                static_cast<unsigned long long>(record->request_sequence),
                static_cast<unsigned long long>(record->wait_due),
                record->ok ? 1 : 0,
                record->timeout ? 1 : 0,
                static_cast<unsigned long long>(record->reply_value),
                kernel::trap_disposition_name(record->disposition),
                kernel::trap_error_name(record->error));
        }

        for (std::size_t index = 0; index < kCompletionCount; ++index) {
            const auto* issue = trace.at(index * 2u);
            const auto* reply = trace.at(index * 2u + 1u);
            if (issue == nullptr || reply == nullptr) {
                return false;
            }

            if (issue->kind != kernel::TaskMessageSyscallPumpTraceKind::issue ||
                issue->owner != kClientId ||
                issue->token != kExpectedTokens[index] ||
                issue->request_sequence != kExpectedSequences[index] ||
                issue->wait_due != kReplyDuePlan[index] || !issue->ok ||
                issue->timeout) {
                return false;
            }

            if (reply->kind != kernel::TaskMessageSyscallPumpTraceKind::reply ||
                reply->owner != kClientId ||
                reply->token != kExpectedTokens[index] ||
                reply->request_sequence != kExpectedSequences[index] ||
                !reply->ok || reply->timeout ||
                reply->reply_value != kExpectedReplyValues[index] ||
                reply->disposition != kExpectedDispositions[index] ||
                reply->error != kExpectedErrors[index]) {
                return false;
            }
        }

        return true;
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
    demo::EchoSessionAcceptorState acceptor_state{};
    std::array<demo::EchoSessionProtocolState, 2> protocol_states{};
    std::array<demo::EchoSessionRequestOperation, 2> request_handlers{
        demo::EchoSessionRequestOperation{
            .state = &protocol_states[0],
        },
        demo::EchoSessionRequestOperation{
            .state = &protocol_states[1],
        },
    };
    std::array<kernel::TaskMessageSessionProtocolSchemaBinding, 2>
        request_bindings{
            kernel::make_task_message_session_protocol_schema_binding(
                demo::kEchoRequestSchema,
                request_handlers[0]),
            kernel::make_task_message_session_protocol_schema_binding(
                demo::kEchoRequestSchema,
                request_handlers[1]),
        };
    std::array<demo::EchoSessionCloseHook, 2> close_handlers{
        demo::EchoSessionCloseHook{
            .state = &protocol_states[0],
        },
        demo::EchoSessionCloseHook{
            .state = &protocol_states[1],
        },
    };
    std::array<demo::ProtocolTrace, 2> protocol_traces{};
    std::array<demo::ServerSessionProtocol, 2> protocols{
        kernel::make_task_message_session_protocol<1>(
            std::array<kernel::TaskMessageSessionProtocolEntry, 1>{
                kernel::task_message_session_protocol_entry(
                    request_bindings[0]),
            },
            kernel::make_task_message_session_protocol_close_handler(
                close_handlers[0]),
            &protocol_traces[0]),
        kernel::make_task_message_session_protocol<1>(
            std::array<kernel::TaskMessageSessionProtocolEntry, 1>{
                kernel::task_message_session_protocol_entry(
                    request_bindings[1]),
            },
            kernel::make_task_message_session_protocol_close_handler(
                close_handlers[1]),
            &protocol_traces[1]),
    };
    demo::SessionTrace session_trace{};
    demo::SessionAcceptorTrace acceptor_trace{};
    demo::ServiceTrace service_trace{};
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

    demo::EchoSessionAcceptor echo_acceptor{
        .state = &acceptor_state,
        .protocols = &protocols,
    };
    demo::ServerSessionAcceptor session_acceptor =
        kernel::make_task_message_session_service_acceptor<2>(
            kernel::make_task_message_session_endpoint_acceptor(
                echo_acceptor),
            "echo-session",
            &acceptor_trace);
    demo::ServerSessionDispatcher session_dispatcher =
        kernel::make_task_message_session_dispatcher<1, 2>(
            std::array<kernel::TaskMessageSessionHandlerEntry, 1>{
                kernel::task_message_session_service_acceptor_entry(
                    demo::kServiceId,
                    "echo-session",
                    session_acceptor),
            },
            &session_trace);
    session_dispatcher.bind_next_session_handle(demo::kBaseSessionHandle);

    auto syscall_table = kernel::make_task_syscall_table(
        std::array<kernel::TaskSyscallHandlerEntry, 1>{
            kernel::task_syscall_handler_entry(
                kernel::TaskSyscallId::capability_call,
                kernel::make_task_syscall_handler(session_dispatcher)),
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
    demo::ServicePump service_pump = kernel::make_task_message_service_pump(
        service_drain,
        demo::make_server_bootstrap_event());
    demo::ServerSessionService session_service =
        kernel::make_task_message_session_service(
            service_pump,
            session_dispatcher,
            session_acceptor,
            &service_trace);
    demo::FrameCaller frame_caller =
        kernel::make_task_message_syscall_frame_caller(
            client_task_messages,
            frame_store,
            demo::CallerAdapter{
                .ctx = &caller_adapter_state,
                .make_frame = &demo::make_message_syscall_call_frame,
                .capture_result = &demo::capture_message_syscall_call_result,
            });
    auto client_transport =
        kernel::make_task_message_syscall_client(frame_caller);
    auto client_pump =
        kernel::make_task_message_syscall_pump<demo::ClientTransport, 8, 8>(
            client_transport,
            &pump_trace);
    auto syscalls = kernel::make_task_message_syscall_api(
        kernel::make_task_message_runtime_api(
            kernel::make_task_message_runtime_service_facade(client_pump)));
    syscalls.bind_cursors(demo::kBaseToken, demo::kBaseSequence);

    demo::server_session_service = &session_service;
    demo::client_syscalls = &syscalls;

    while (running.run_once()) {
    }

    shared.mailbox_valid =
        mailbox.valid() && mailbox.server() == demo::kServerId &&
        server_task_messages.valid() && client_task_messages.valid();
    shared.message_dispatcher_valid = message_dispatcher.valid();
    shared.frame_store_valid = frame_store.valid();
    shared.frame_bridge_valid = frame_bridge.valid();
    shared.message_frame_bridge_valid = message_frame_bridge.valid();
    shared.service_loop_valid = service_loop.valid();
    shared.service_drain_valid = service_drain.valid();
    shared.service_pump_valid = service_pump.valid();
    shared.session_service_valid = session_service.valid();
    shared.client_syscalls_valid = syscalls.valid();
    shared.server_bootstrapped =
        runtime.bootstrap_worker(
            demo::kServerId,
            demo::make_server_bootstrap_event());

    std::size_t loops = 0;
    while ((!shared.finished ||
            shared.completions_received < demo::kCompletionCount) &&
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

    const bool dispatch_trace_ok = demo::inspect_dispatch_trace(session_trace);
    const bool acceptor_trace_ok = demo::inspect_acceptor_trace(acceptor_trace);
    const bool protocol_trace_ok =
        demo::inspect_protocol_trace(protocol_traces[0]);
    const bool service_trace_ok = demo::inspect_service_trace(service_trace);
    const bool pump_trace_ok = demo::inspect_pump_trace(pump_trace);
    const auto service_loop_witness =
        kernel::task_message_session_service_loop_witness(session_trace,
                                                          acceptor_trace,
                                                          protocol_traces[0],
                                                          service_trace,
                                                          pump_trace);
    const auto ownership_corridor_witness =
        kernel::task_message_session_ownership_corridor_witness(
            service_loop_witness);
    const auto session_slot = session_service.session(0u);
    const auto lookup =
        session_service.lookup_session(demo::kBaseSessionHandle);
    const auto* channel_slot = session_service.channel(0u);
    const auto channel_lookup =
        session_service.lookup_channel(demo::kBaseSessionHandle);
    auto& nested_pump = syscalls.runtime().services().pump();

    const bool completion_ok =
        shared.completions_received == demo::kCompletionCount &&
        !shared.pending_call.valid &&
        shared.active_session_handle == 0u &&
        shared.completions[0].owner == demo::kClientId &&
        shared.completions[1].owner == demo::kClientId &&
        shared.completions[2].owner == demo::kClientId &&
        shared.completions[3].owner == demo::kClientId &&
        shared.completions[0].reply_from == demo::kServerId &&
        shared.completions[1].reply_from == demo::kServerId &&
        shared.completions[2].reply_from == demo::kServerId &&
        shared.completions[3].reply_from == demo::kServerId &&
        shared.completions[0].reply_to == demo::kClientId &&
        shared.completions[1].reply_to == demo::kClientId &&
        shared.completions[2].reply_to == demo::kClientId &&
        shared.completions[3].reply_to == demo::kClientId;
    bool completion_records_ok = completion_ok;
    for (std::size_t index = 0; index < demo::kCompletionCount; ++index) {
        const auto& record = shared.completions[index];
        completion_records_ok =
            completion_records_ok && !record.timeout &&
            record.token == demo::kExpectedTokens[index] &&
            record.request_sequence == demo::kExpectedSequences[index] &&
            record.capability_id == demo::kExpectedCapabilityIds[index] &&
            record.operation == demo::kExpectedOperations[index] &&
            record.payload == demo::kExpectedPayloads[index] &&
            record.reply_value == demo::kExpectedReplyValues[index] &&
            demo::trap_result_matches(record.trap,
                                      demo::kExpectedDispositions[index],
                                      demo::kExpectedErrors[index],
                                      demo::kExpectedReplyValues[index]);
    }
    const bool service_ok =
        acceptor_state.accept_calls == 1u &&
        acceptor_state.last_service_id == demo::kServiceId &&
        acceptor_state.last_session_handle == demo::kBaseSessionHandle &&
        acceptor_state.last_open_payload == demo::kOpenPayload &&
        acceptor_state.last_channel_slot == 0u &&
        protocol_states[0].request_calls == 1u &&
        protocol_states[0].close_calls == 1u &&
        protocol_states[0].last_service_id == demo::kServiceId &&
        protocol_states[0].last_session_handle == demo::kBaseSessionHandle &&
        protocol_states[0].last_open_payload == demo::kOpenPayload &&
        protocol_states[0].last_request_operation == demo::kRequestOperation &&
        protocol_states[0].last_request_payload == demo::kRequestPayload &&
        protocol_states[0].last_close_reason == demo::kCloseReason &&
        protocol_states[0].last_channel_slot == 0u &&
        demo::same_text(protocol_states[0].last_operation_name,
                        std::string_view{"echo-request"}) &&
        demo::same_text(protocol_states[0].last_field_name,
                        std::string_view{"request-value"}) &&
        demo::same_text(protocol_states[0].last_result_name,
                        std::string_view{"reply-value"}) &&
        protocol_states[0].last_view_kind ==
            kernel::TaskMessageSessionProtocolSchemaViewKind::payload_only &&
        protocol_states[0].last_field_count == 1u &&
        protocol_states[0].last_schema_supported &&
        protocol_states[1].request_calls == 0u &&
        protocol_states[1].close_calls == 0u &&
        protocol_traces[1].size() == 0u;
    const bool surface_ok =
        frame_adapter_state.capture_calls == demo::kCompletionCount &&
        frame_adapter_state.writeback_calls == demo::kCompletionCount &&
        result_ready_state.ready_calls == demo::kCompletionCount &&
        result_ready_state.last_value == 0u &&
        result_ready_state.last_disposition ==
            kernel::TrapDisposition::unsupported &&
        result_ready_state.last_error ==
            kernel::TrapError::unsupported_service &&
        result_ready_state.last_writeback_seen &&
        caller_adapter_state.make_calls == demo::kCompletionCount &&
        caller_adapter_state.capture_result_calls == demo::kCompletionCount &&
        caller_adapter_state.last_syscall ==
            kernel::TaskSyscallId::capability_call &&
        caller_adapter_state.last_arg0 == demo::kGhostServiceId &&
        caller_adapter_state.last_arg1 ==
            kernel::task_message_session_open_operation &&
        caller_adapter_state.last_arg2 == demo::kGhostOpenPayload &&
        caller_adapter_state.last_reply_value == 0u &&
        caller_adapter_state.last_disposition ==
            kernel::TrapDisposition::unsupported &&
        caller_adapter_state.last_error ==
            kernel::TrapError::unsupported_service &&
        caller_adapter_state.last_writeback_seen;
    const bool runtime_ok =
        shared.mailbox_valid && shared.message_dispatcher_valid &&
        shared.frame_store_valid && shared.frame_bridge_valid &&
        shared.message_frame_bridge_valid && shared.service_loop_valid &&
        shared.service_drain_valid && shared.service_pump_valid &&
        shared.session_service_valid && shared.client_syscalls_valid &&
        shared.server_bootstrapped && shared.client_bootstrapped &&
        shared.open_issue_ok && shared.open_kick_ok &&
        shared.request_issue_ok && shared.request_kick_ok &&
        shared.close_issue_ok && shared.close_kick_ok &&
        shared.ghost_issue_ok && shared.ghost_kick_ok && shared.finished &&
        !shared.unexpected_timeout && shared.total_served == 4u &&
        shared.server_timeouts >= 1u && shared.wait_arm_successes >= 1u &&
        shared.idle_runs >= 1u && shared.server_runs >= 5u &&
        shared.client_runs == demo::kCompletionCount &&
        frame_store.pending() == 0u && mailbox.pending_requests() == 0u &&
        mailbox.pending_replies() == 0u && mailbox.reply_waiters() == 0u &&
        mailbox.receive_waiting() && syscalls.pending_requests() == 0u &&
        syscalls.pending_completions() == 0u && !syscalls.busy() &&
        nested_pump.pending_requests() == 0u &&
        nested_pump.pending_completions() == 0u &&
        !nested_pump.busy() && loops < 96u;
    const bool dispatcher_ok =
        demo::same_text(session_service.service_name(),
                        std::string_view{"echo-session"}) &&
        session_service.active_sessions() == 0u &&
        session_service.active_channels() == 0u &&
        session_slot != nullptr && !session_slot->active &&
        channel_slot != nullptr && !channel_slot->active &&
        !lookup.matched && !channel_lookup.matched;
    const bool ok =
        completion_records_ok && service_ok && surface_ok && runtime_ok &&
        dispatcher_ok && dispatch_trace_ok && acceptor_trace_ok &&
        protocol_trace_ok && service_trace_ok && pump_trace_ok &&
        ownership_corridor_witness.ok();

    std::printf(
        "[runtime-task-message-session-service-loop-witness] ok=%d verdict=%s domain=%s bootstrap=%s timeout=%s open_dispatch=%s open_service=%s roundtrip=%s close_dispatch=%s close_service=%s ghost_dispatch=%s ghost_service=%s summary=%s handoff=%d ownership=%d\n",
        service_loop_witness.ok() ? 1 : 0,
        semantic::verdict_name(service_loop_witness.verdict()),
        semantic::failure_domain_name(
            service_loop_witness.failure_domain()),
        semantic::verdict_name(service_loop_witness.bootstrap.verdict()),
        semantic::verdict_name(service_loop_witness.timeout.verdict()),
        semantic::verdict_name(service_loop_witness.open_dispatch.verdict()),
        semantic::verdict_name(service_loop_witness.open_service.verdict()),
        semantic::verdict_name(service_loop_witness.roundtrip.verdict()),
        semantic::verdict_name(service_loop_witness.close_dispatch.verdict()),
        semantic::verdict_name(service_loop_witness.close_service.verdict()),
        semantic::verdict_name(service_loop_witness.ghost_dispatch.verdict()),
        semantic::verdict_name(service_loop_witness.ghost_service.verdict()),
        service_loop_witness.summary_path().data(),
        service_loop_witness.handoff_ready ? 1 : 0,
        service_loop_witness.ownership_path ? 1 : 0);
    std::printf(
        "[runtime-task-message-session-ownership-corridor-witness] ok=%d verdict=%s domain=%s roundtrip=%s service_loop=%s summary=%s handoff=%d client=%d server=%d shared=%d lifecycle=%d\n",
        ownership_corridor_witness.ok() ? 1 : 0,
        semantic::verdict_name(ownership_corridor_witness.verdict()),
        semantic::failure_domain_name(
            ownership_corridor_witness.failure_domain()),
        semantic::verdict_name(ownership_corridor_witness.roundtrip.verdict()),
        semantic::verdict_name(
            ownership_corridor_witness.service_loop.verdict()),
        ownership_corridor_witness.summary_path().data(),
        ownership_corridor_witness.handoff_ready ? 1 : 0,
        ownership_corridor_witness.client_continuity_path ? 1 : 0,
        ownership_corridor_witness.server_ownership_path ? 1 : 0,
        ownership_corridor_witness.shared_roundtrip_path ? 1 : 0,
        ownership_corridor_witness.lifecycle_path ? 1 : 0);
    std::printf(
        "[runtime-task-message-session-service-loop-demo] ok=%d valid=%d server_boot=%d client_boot=%d completions=%zu served=%zu timeouts=%u idle=%u active_sessions=%zu active_channels=%zu pending_frames=%zu pending_req=%zu pending_reply=%zu pump_req=%zu pump_completion=%zu waiting=%d loops=%llu\n",
        ok ? 1 : 0,
        shared.session_service_valid ? 1 : 0,
        shared.server_bootstrapped ? 1 : 0,
        shared.client_bootstrapped ? 1 : 0,
        shared.completions_received,
        shared.total_served,
        shared.server_timeouts,
        shared.idle_runs,
        session_service.active_sessions(),
        session_service.active_channels(),
        frame_store.pending(),
        mailbox.pending_requests(),
        mailbox.pending_replies(),
        nested_pump.pending_requests(),
        nested_pump.pending_completions(),
        mailbox.receive_waiting() ? 1 : 0,
        static_cast<unsigned long long>(loops));
    std::printf(
        "[runtime-task-message-session-service-loop-values] handle=%llu open=%llu request=%llu close=%llu ghost_err=%s token0=%llu token1=%llu token2=%llu token3=%llu seq0=%llu seq1=%llu seq2=%llu seq3=%llu\n",
        static_cast<unsigned long long>(demo::kBaseSessionHandle),
        static_cast<unsigned long long>(demo::kExpectedReplyValues[0]),
        static_cast<unsigned long long>(demo::kExpectedReplyValues[1]),
        static_cast<unsigned long long>(demo::kExpectedReplyValues[2]),
        kernel::trap_error_name(shared.completions[3].trap.error),
        static_cast<unsigned long long>(demo::kExpectedTokens[0]),
        static_cast<unsigned long long>(demo::kExpectedTokens[1]),
        static_cast<unsigned long long>(demo::kExpectedTokens[2]),
        static_cast<unsigned long long>(demo::kExpectedTokens[3]),
        static_cast<unsigned long long>(demo::kExpectedSequences[0]),
        static_cast<unsigned long long>(demo::kExpectedSequences[1]),
        static_cast<unsigned long long>(demo::kExpectedSequences[2]),
        static_cast<unsigned long long>(demo::kExpectedSequences[3]));
    std::printf(
        "[runtime-task-message-session-service-loop-trace] ok=%d service=%d dispatch=%d acceptor=%d protocol=%d pump=%d\n",
        ok ? 1 : 0,
        service_trace_ok ? 1 : 0,
        dispatch_trace_ok ? 1 : 0,
        acceptor_trace_ok ? 1 : 0,
        protocol_trace_ok ? 1 : 0,
        pump_trace_ok ? 1 : 0);
    return ok ? 0 : 1;
}
