module;

#include <array>
#include <string_view>

export module kernel.task_message_session_endpoint;

export import kernel.task_message_session_acceptor;
import semantic.core;
import util.core;

export namespace kernel {
    struct TaskMessageSessionEndpoint {
        util::u64 service_id{0};
        const char* service_name{"session-service"};
        util::u64 session_handle{0};
        util::u64 open_payload{0};
        util::u16 channel_slot{task_message_session_channel_unmapped_slot};
    };

    struct TaskMessageSessionEndpointRequestView {
        TaskMessageSessionEndpoint endpoint{};
        util::u64 operation{0};
        util::u64 payload{0};
    };

    struct TaskMessageSessionEndpointCloseView {
        TaskMessageSessionEndpoint endpoint{};
        util::u64 reason{0};
    };

    struct TaskMessageSessionEndpointBinding {
        TaskMessageSessionChannelHandler* out_handler{nullptr};

        [[nodiscard]] constexpr bool valid() const noexcept
        {
            return out_handler != nullptr;
        }

        void clear() const noexcept
        {
            if (out_handler != nullptr) {
                *out_handler = {};
            }
        }
    };

    enum class TaskMessageSessionEndpointWitnessKind : util::u8 {
        endpoint = 0,
        request,
        close,
        accept,
    };

    struct TaskMessageSessionEndpointWitness {
        bool ready{false};
        TaskMessageSessionEndpointWitnessKind kind{
            TaskMessageSessionEndpointWitnessKind::endpoint};
        util::u64 service_id{0};
        util::u64 session_handle{0};
        util::u64 open_payload{0};
        util::u16 channel_slot{task_message_session_channel_unmapped_slot};
        util::u64 operation{0};
        util::u64 payload{0};
        util::u64 reason{0};
        bool binding_valid{false};
        bool handler_valid{false};
        TrapDisposition disposition{TrapDisposition::rejected};
        TrapError error{TrapError::none};
        util::u64 value{0};

        [[nodiscard]] constexpr bool endpoint_branch_ok() const noexcept
        {
            return kind == TaskMessageSessionEndpointWitnessKind::endpoint &&
                   service_id != 0u && session_handle != 0u &&
                   channel_slot != task_message_session_channel_unmapped_slot;
        }

        [[nodiscard]] constexpr bool request_branch_ok() const noexcept
        {
            return kind == TaskMessageSessionEndpointWitnessKind::request &&
                   service_id != 0u && session_handle != 0u &&
                   channel_slot != task_message_session_channel_unmapped_slot &&
                   operation != 0u &&
                   disposition != TrapDisposition::rejected;
        }

        [[nodiscard]] constexpr bool close_branch_ok() const noexcept
        {
            return kind == TaskMessageSessionEndpointWitnessKind::close &&
                   service_id != 0u && session_handle != 0u &&
                   channel_slot != task_message_session_channel_unmapped_slot &&
                   reason != 0u &&
                   disposition != TrapDisposition::rejected;
        }

        [[nodiscard]] constexpr bool accept_branch_ok() const noexcept
        {
            if (kind != TaskMessageSessionEndpointWitnessKind::accept ||
                service_id == 0u || session_handle == 0u ||
                channel_slot == task_message_session_channel_unmapped_slot) {
                return false;
            }

            if (!binding_valid) {
                return disposition == TrapDisposition::rejected &&
                       error == TrapError::invalid_argument;
            }

            if (disposition == TrapDisposition::handled &&
                error == TrapError::none) {
                return handler_valid;
            }

            if (disposition == TrapDisposition::rejected &&
                error == TrapError::unbound_adapter) {
                return !handler_valid;
            }

            return false;
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

            if (endpoint_branch_ok() || request_branch_ok() ||
                close_branch_ok() || accept_branch_ok()) {
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

            if (kind == TaskMessageSessionEndpointWitnessKind::accept) {
                return binding_valid ? semantic::FailureDomain::handoff
                                     : semantic::FailureDomain::input;
            }

            if (service_id == 0u || session_handle == 0u ||
                channel_slot == task_message_session_channel_unmapped_slot) {
                return semantic::FailureDomain::selection;
            }

            return semantic::FailureDomain::handoff;
        }

        [[nodiscard]] constexpr std::string_view summary_path() const noexcept
        {
            return "task-message-session-endpoint-witness.summary";
        }
    };

    struct TaskMessageSessionEndpointWitnessHandoffTarget {
        const TaskMessageSessionEndpointWitness* witness{nullptr};

        [[nodiscard]] constexpr std::string_view entry_name() const noexcept
        {
            return "task-message-session-endpoint-witness";
        }

        [[nodiscard]] constexpr std::string_view
        selected_summary_path() const noexcept
        {
            return witness != nullptr
                       ? witness->summary_path()
                       : std::string_view{
                             "task-message-session-endpoint-witness.summary"};
        }
    };

    static_assert(
        semantic::reflected_member_names_match_when_enabled<
            TaskMessageSessionEndpointWitness>(
            std::array<std::string_view, 14>{
                "ready",
                "kind",
                "service_id",
                "session_handle",
                "open_payload",
                "channel_slot",
                "operation",
                "payload",
                "reason",
                "binding_valid",
                "handler_valid",
                "disposition",
                "error",
                "value",
            }));

    static_assert(semantic::WitnessCarrier<TaskMessageSessionEndpointWitness>);
    static_assert(
        semantic::HandoffTarget<
            TaskMessageSessionEndpointWitnessHandoffTarget>);

    [[nodiscard]] constexpr auto make_task_message_session_endpoint(
        const TaskMessageSessionChannel& channel) noexcept
        -> TaskMessageSessionEndpoint
    {
        return TaskMessageSessionEndpoint{
            .service_id = channel.service_id,
            .service_name = channel.service_name,
            .session_handle = channel.session_handle,
            .open_payload = channel.open_payload,
            .channel_slot = channel.channel_slot,
        };
    }

    [[nodiscard]] constexpr TaskMessageSessionEndpointWitness
    task_message_session_endpoint_witness(
        const TaskMessageSessionEndpoint& endpoint) noexcept
    {
        return TaskMessageSessionEndpointWitness{
            .ready = true,
            .kind = TaskMessageSessionEndpointWitnessKind::endpoint,
            .service_id = endpoint.service_id,
            .session_handle = endpoint.session_handle,
            .open_payload = endpoint.open_payload,
            .channel_slot = endpoint.channel_slot,
        };
    }

    [[nodiscard]] constexpr TaskMessageSessionEndpointWitness
    task_message_session_endpoint_request_witness(
        const TaskMessageSessionEndpointRequestView& request,
        const TrapResult& result) noexcept
    {
        return TaskMessageSessionEndpointWitness{
            .ready = true,
            .kind = TaskMessageSessionEndpointWitnessKind::request,
            .service_id = request.endpoint.service_id,
            .session_handle = request.endpoint.session_handle,
            .open_payload = request.endpoint.open_payload,
            .channel_slot = request.endpoint.channel_slot,
            .operation = request.operation,
            .payload = request.payload,
            .disposition = result.disposition,
            .error = result.error,
            .value = result.value,
        };
    }

    [[nodiscard]] constexpr TaskMessageSessionEndpointWitness
    task_message_session_endpoint_close_witness(
        const TaskMessageSessionEndpointCloseView& close,
        const TrapResult& result) noexcept
    {
        return TaskMessageSessionEndpointWitness{
            .ready = true,
            .kind = TaskMessageSessionEndpointWitnessKind::close,
            .service_id = close.endpoint.service_id,
            .session_handle = close.endpoint.session_handle,
            .open_payload = close.endpoint.open_payload,
            .channel_slot = close.endpoint.channel_slot,
            .reason = close.reason,
            .disposition = result.disposition,
            .error = result.error,
            .value = result.value,
        };
    }

    [[nodiscard]] constexpr TaskMessageSessionEndpointWitness
    task_message_session_endpoint_accept_witness(
        const TaskMessageSessionEndpoint& endpoint,
        TaskMessageSessionEndpointBinding binding,
        const TaskMessageSessionChannelHandler& handler,
        const TrapResult& result) noexcept
    {
        return TaskMessageSessionEndpointWitness{
            .ready = true,
            .kind = TaskMessageSessionEndpointWitnessKind::accept,
            .service_id = endpoint.service_id,
            .session_handle = endpoint.session_handle,
            .open_payload = endpoint.open_payload,
            .channel_slot = endpoint.channel_slot,
            .binding_valid = binding.valid(),
            .handler_valid = handler.valid(),
            .disposition = result.disposition,
            .error = result.error,
            .value = result.value,
        };
    }

    [[nodiscard]] constexpr bool task_message_session_endpoint_witness_ready(
        const TaskMessageSessionEndpointWitness& witness) noexcept
    {
        return witness.ready;
    }

    [[nodiscard]] constexpr TaskMessageSessionEndpointWitnessHandoffTarget
    task_message_session_endpoint_witness_handoff_target(
        const TaskMessageSessionEndpointWitness& witness) noexcept
    {
        return TaskMessageSessionEndpointWitnessHandoffTarget{
            .witness = &witness,
        };
    }

    [[nodiscard]] constexpr auto task_message_session_endpoint_handled(
        util::u64 value) noexcept -> TrapResult
    {
        return TrapResult{
            .disposition = TrapDisposition::handled,
            .error = TrapError::none,
            .value = value,
        };
    }

    [[nodiscard]] constexpr auto task_message_session_endpoint_rejected(
        TrapError error,
        util::u64 value = 0) noexcept -> TrapResult
    {
        return TrapResult{
            .disposition = TrapDisposition::rejected,
            .error = error,
            .value = value,
        };
    }

    [[nodiscard]] constexpr auto task_message_session_endpoint_unsupported(
        TrapError error = TrapError::unsupported_service,
        util::u64 value = 0) noexcept -> TrapResult
    {
        return TrapResult{
            .disposition = TrapDisposition::unsupported,
            .error = error,
            .value = value,
        };
    }

    [[nodiscard]] constexpr auto task_message_session_endpoint_invalid_argument(
        util::u64 value = 0) noexcept -> TrapResult
    {
        return task_message_session_endpoint_rejected(
            TrapError::invalid_argument,
            value);
    }

    [[nodiscard]] constexpr auto task_message_session_endpoint_unbound_adapter(
        util::u64 value = 0) noexcept -> TrapResult
    {
        return task_message_session_endpoint_rejected(
            TrapError::unbound_adapter,
            value);
    }

    namespace detail {
        template <typename Target>
        [[nodiscard]] TrapResult task_message_session_endpoint_request_adapter(
            void* self,
            const TaskMessageSessionChannel& channel,
            TaskMessageSessionRequestDispatchView request) noexcept
        {
            return static_cast<Target*>(self)->request(
                TaskMessageSessionEndpointRequestView{
                    .endpoint = make_task_message_session_endpoint(channel),
                    .operation = request.operation,
                    .payload = request.payload,
                });
        }

        template <typename Target>
        [[nodiscard]] TrapResult task_message_session_endpoint_close_adapter(
            void* self,
            const TaskMessageSessionChannel& channel,
            TaskMessageSessionCloseDispatchView close) noexcept
        {
            return static_cast<Target*>(self)->close(
                TaskMessageSessionEndpointCloseView{
                    .endpoint = make_task_message_session_endpoint(channel),
                    .reason = close.reason,
                });
        }

        template <typename Target>
        [[nodiscard]] TrapResult task_message_session_endpoint_accept_adapter(
            void* self,
            const TaskMessageSessionChannel& channel,
            TaskMessageSessionChannelHandler& out_handler) noexcept
        {
            return static_cast<Target*>(self)->accept(
                make_task_message_session_endpoint(channel),
                TaskMessageSessionEndpointBinding{
                    .out_handler = &out_handler,
                });
        }
    }

    template <typename Target>
    [[nodiscard]] auto make_task_message_session_endpoint_handler(
        Target& target) noexcept -> TaskMessageSessionChannelHandler
    {
        return TaskMessageSessionChannelHandler{
            .self = &target,
            .request_fn =
                &detail::task_message_session_endpoint_request_adapter<Target>,
            .close_fn =
                &detail::task_message_session_endpoint_close_adapter<Target>,
        };
    }

    template <typename Target>
    void bind_task_message_session_endpoint(
        TaskMessageSessionEndpointBinding binding,
        Target& target) noexcept
    {
        if (!binding.valid()) {
            return;
        }

        *binding.out_handler = make_task_message_session_endpoint_handler(target);
    }

    template <typename Target>
    [[nodiscard]] auto make_task_message_session_endpoint_acceptor(
        Target& target) noexcept -> TaskMessageSessionChannelAcceptor
    {
        return TaskMessageSessionChannelAcceptor{
            .self = &target,
            .accept_fn =
                &detail::task_message_session_endpoint_accept_adapter<Target>,
        };
    }
}
