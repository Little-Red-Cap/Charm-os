module;

export module kernel.task_message_session_endpoint;

export import kernel.task_message_session_acceptor;
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

        [[nodiscard]] bool valid() const noexcept
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
