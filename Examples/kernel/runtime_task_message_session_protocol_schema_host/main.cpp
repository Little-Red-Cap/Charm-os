#include <array>
#include <cstdint>
#include <cstdio>
#include <string_view>

import kernel.task_message_session_protocol_schema;
import semantic.core;

namespace demo {
    using namespace std::literals;

    inline constexpr std::uint64_t kServiceId{0x51u};
    inline constexpr std::uint64_t kSessionHandle{0x9000u};
    inline constexpr std::uint64_t kOpenPayload{0xAAu};
    inline constexpr std::uint64_t kEchoOperation{0x21u};
    inline constexpr std::uint64_t kStatusOperation{0x22u};
    inline constexpr std::uint64_t kUnknownOperation{0x44u};
    inline constexpr std::uint64_t kEchoPayload{33u};

    struct SchemaState {
        std::uint32_t calls{0};
        std::uint64_t last_service_id{0};
        std::uint64_t last_session_handle{0};
        std::uint64_t last_open_payload{0};
        std::uint64_t last_operation{0};
        std::uint64_t last_payload{0};
        std::uint16_t last_channel_slot{
            kernel::task_message_session_channel_unmapped_slot};
        const char* last_operation_name{"unmapped"};
        const char* last_field_name{"payload"};
        const char* last_result_name{"value"};
        kernel::TaskMessageSessionProtocolSchemaViewKind last_view_kind{
            kernel::TaskMessageSessionProtocolSchemaViewKind::invalid};
        std::uint8_t last_field_count{0};
        bool last_supported{false};
    };

    struct EchoRequest {
        SchemaState* state{nullptr};

        [[nodiscard]] kernel::TrapResult dispatch(
            kernel::TaskMessageSessionProtocolSemanticProjection request) const
            noexcept
        {
            ++state->calls;
            state->last_service_id = request.endpoint.service_id;
            state->last_session_handle = request.endpoint.session_handle;
            state->last_open_payload = request.endpoint.open_payload;
            state->last_operation = request.operation;
            state->last_payload = request.payload;
            state->last_channel_slot = request.endpoint.channel_slot;
            state->last_operation_name = request.descriptor.operation_name;
            state->last_field_name =
                request.field_count != 0u ? request.fields[0].name : "";
            state->last_result_name = request.result_name;
            state->last_view_kind = request.descriptor.view_kind;
            state->last_field_count = request.field_count;
            state->last_supported = request.descriptor.supported;

            return kernel::task_message_session_endpoint_handled(
                request.endpoint.session_handle +
                request.operation +
                request.payload +
                request.endpoint.channel_slot);
        }
    };

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
        return result.disposition == disposition &&
               result.error == error &&
               result.value == value;
    }

    [[nodiscard]] bool probe_schema_catalog() noexcept
    {
        auto catalog = kernel::make_task_message_session_protocol_schema_catalog(
            std::array<kernel::TaskMessageSessionProtocolSchemaEntry, 2>{
                kernel::task_message_session_protocol_schema_entry(
                    kEchoOperation,
                    "echo-request",
                    "request-value",
                    "reply-value"),
                kernel::task_message_session_protocol_schema_entry(
                    kStatusOperation,
                    "status-query",
                    "status-token",
                    "status-code"),
            });

        const auto lookup_echo = catalog.lookup(kEchoOperation);
        const auto lookup_unknown = catalog.lookup(kUnknownOperation);
        const auto projection = catalog.semantic_projection(
            kernel::TaskMessageSessionEndpointRequestView{
                .endpoint = kernel::TaskMessageSessionEndpoint{
                    .service_id = kServiceId,
                    .service_name = "echo-schema",
                    .session_handle = kSessionHandle,
                    .open_payload = kOpenPayload,
                    .channel_slot = 1u,
                },
                .operation = kEchoOperation,
                .payload = kEchoPayload,
            });
        const auto unknown = catalog.describe(kUnknownOperation);

        return catalog.valid() &&
               decltype(catalog)::capacity() == 2u &&
               lookup_echo.matched &&
               lookup_echo.slot == 0u &&
               lookup_echo.entry != nullptr &&
               same_text(lookup_echo.entry->operation_name, "echo-request"sv) &&
               lookup_echo.entry->view_kind ==
                   kernel::TaskMessageSessionProtocolSchemaViewKind::
                       payload_only &&
               lookup_echo.entry->field_count == 1u &&
               same_text(lookup_echo.entry->field_names[0], "request-value"sv) &&
               same_text(lookup_echo.entry->result_name, "reply-value"sv) &&
               lookup_echo.entry->supported &&
               !lookup_unknown.matched &&
               lookup_unknown.slot ==
                   kernel::task_message_session_protocol_unmapped_slot &&
               projection.operation == kEchoOperation &&
               projection.payload == kEchoPayload &&
               projection.endpoint.service_id == kServiceId &&
               projection.endpoint.session_handle == kSessionHandle &&
               projection.descriptor.operation == kEchoOperation &&
               same_text(projection.descriptor.operation_name,
                         "echo-request"sv) &&
               projection.descriptor.view_kind ==
                   kernel::TaskMessageSessionProtocolSchemaViewKind::
                       payload_only &&
               projection.field_count == 1u &&
               same_text(projection.fields[0].name, "request-value"sv) &&
               projection.fields[0].value == kEchoPayload &&
               same_text(projection.result_name, "reply-value"sv) &&
               unknown.operation == kUnknownOperation &&
               same_text(unknown.operation_name, "unmapped"sv) &&
               unknown.view_kind ==
                   kernel::TaskMessageSessionProtocolSchemaViewKind::opaque &&
               unknown.field_count == 0u &&
               same_text(unknown.result_name, "value"sv) &&
               !unknown.supported &&
               same_text(
                   kernel::task_message_session_protocol_schema_view_kind_name(
                       kernel::TaskMessageSessionProtocolSchemaViewKind::
                           payload_only),
                   "payload-only"sv);
    }

    [[nodiscard]] bool probe_schema_binding() noexcept
    {
        SchemaState state{};
        EchoRequest echo{
            .state = &state,
        };
        auto schema = kernel::task_message_session_protocol_schema_entry(
            kEchoOperation,
            "echo-request",
            "request-value",
            "reply-value");
        auto binding =
            kernel::make_task_message_session_protocol_schema_binding(
                schema,
                echo);
        kernel::TaskMessageSessionProtocolSchemaBinding unbound{
            schema,
        };
        using ProtocolTrace = kernel::TaskMessageSessionProtocolTraceBuffer<4>;
        ProtocolTrace trace{};
        auto protocol = kernel::make_task_message_session_protocol<1>(
            std::array<kernel::TaskMessageSessionProtocolEntry, 1>{
                kernel::task_message_session_protocol_entry(binding),
            },
            &trace);

        const auto request = kernel::TaskMessageSessionEndpointRequestView{
            .endpoint = kernel::TaskMessageSessionEndpoint{
                .service_id = kServiceId,
                .service_name = "echo-schema",
                .session_handle = kSessionHandle,
                .open_payload = kOpenPayload,
                .channel_slot = 0u,
            },
            .operation = kEchoOperation,
            .payload = kEchoPayload,
        };
        const auto result = protocol.dispatch_request(request);
        const auto unbound_result = unbound.dispatch(request);
        const auto* event = trace.at(0u);
        if (event == nullptr) {
            return false;
        }

        const auto result_witness =
            kernel::task_message_session_protocol_witness(result);
        const auto event_witness =
            kernel::task_message_session_protocol_witness(*event);
        const auto terminal_witness =
            kernel::task_message_session_protocol_witness(trace);
        const auto handoff =
            kernel::task_message_session_protocol_witness_handoff_target(
                terminal_witness);

        return binding.valid() &&
               same_text(binding.schema().operation_name, "echo-request"sv) &&
               trap_result_matches(result.trap,
                                   kernel::TrapDisposition::handled,
                                   kernel::TrapError::none,
                                   kSessionHandle + kEchoOperation +
                                       kEchoPayload) &&
               result.matched && result.handler_valid &&
               same_text(result.operation_name, "echo-request"sv) &&
               trap_result_matches(unbound_result,
                                   kernel::TrapDisposition::rejected,
                                   kernel::TrapError::unbound_adapter) &&
               state.calls == 1u &&
               state.last_service_id == kServiceId &&
               state.last_session_handle == kSessionHandle &&
               state.last_open_payload == kOpenPayload &&
               state.last_operation == kEchoOperation &&
               state.last_payload == kEchoPayload &&
               state.last_channel_slot == 0u &&
               same_text(state.last_operation_name, "echo-request"sv) &&
               same_text(state.last_field_name, "request-value"sv) &&
               same_text(state.last_result_name, "reply-value"sv) &&
               state.last_view_kind ==
                   kernel::TaskMessageSessionProtocolSchemaViewKind::
                       payload_only &&
               state.last_field_count == 1u &&
               state.last_supported &&
               trace.size() == 1u &&
               event->sequence == 1u &&
               event->kind ==
                   kernel::TaskMessageSessionProtocolTraceKind::request &&
               same_text(event->operation_name, "echo-request"sv) &&
               event->payload == kEchoPayload &&
               event->slot == 0u &&
               event->matched &&
               event->handler_valid &&
               event->value == kSessionHandle + kEchoOperation + kEchoPayload &&
               result_witness.verdict() == semantic::Verdict::standing &&
               result_witness.failure_domain() ==
                   semantic::FailureDomain::none &&
               result_witness.request_handler_branch_ok() &&
               kernel::task_message_session_protocol_witness_ready(
                   event_witness) &&
               event_witness.verdict() == semantic::Verdict::standing &&
               event_witness.failure_domain() ==
                   semantic::FailureDomain::none &&
               event_witness.request_handler_branch_ok() &&
               terminal_witness.verdict() == semantic::Verdict::standing &&
               terminal_witness.failure_domain() ==
                   semantic::FailureDomain::none &&
               std::string_view{handoff.entry_name()} ==
                   "task-message-session-protocol-witness"sv &&
               std::string_view{handoff.selected_summary_path()} ==
                   "task-message-session-protocol-witness.summary"sv;
    }
}

int main()
{
    const bool catalog_ok = demo::probe_schema_catalog();
    const bool binding_ok = demo::probe_schema_binding();
    const bool ok = catalog_ok && binding_ok;

    std::printf(
        "[runtime-task-message-session-protocol-schema-demo] ok=%d catalog=%d binding=%d\n",
        ok ? 1 : 0,
        catalog_ok ? 1 : 0,
        binding_ok ? 1 : 0);
    std::printf(
        "[runtime-task-message-session-protocol-schema-witness] ok=%d summary=%s\n",
        ok ? 1 : 0,
        kernel::TaskMessageSessionProtocolWitness{}.summary_path().data());
    return ok ? 0 : 1;
}
