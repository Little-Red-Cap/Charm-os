module;

#include <array>
#include <cstddef>
#include <string_view>

export module kernel.task_message_session_protocol_schema;

export import kernel.task_message_session_protocol;
import semantic.core;
import util.core;

export namespace kernel {
    enum class TaskMessageSessionProtocolSchemaViewKind : util::u8 {
        invalid = 0,
        payload_only,
        opaque,
    };

    [[nodiscard]] constexpr const char*
    task_message_session_protocol_schema_view_kind_name(
        TaskMessageSessionProtocolSchemaViewKind kind) noexcept
    {
        switch (kind) {
        case TaskMessageSessionProtocolSchemaViewKind::invalid:
            return "invalid";
        case TaskMessageSessionProtocolSchemaViewKind::payload_only:
            return "payload-only";
        case TaskMessageSessionProtocolSchemaViewKind::opaque:
            return "opaque";
        }
        return "unknown";
    }

    struct TaskMessageSessionProtocolSchemaEntry {
        util::u64 operation{0};
        const char* operation_name{"operation"};
        TaskMessageSessionProtocolSchemaViewKind view_kind{
            TaskMessageSessionProtocolSchemaViewKind::invalid};
        util::u8 field_count{0};
        std::array<const char*, 1> field_names{};
        const char* result_name{"value"};
        bool supported{false};
    };

    static_assert(
        semantic::reflected_member_names_match_when_enabled<
                  TaskMessageSessionProtocolSchemaEntry>(
        std::array<std::string_view, 7>{
            "operation",
            "operation_name",
            "view_kind",
            "field_count",
            "field_names",
            "result_name",
            "supported"}));

    [[nodiscard]] constexpr auto task_message_session_protocol_schema_entry(
        util::u64 operation,
        const char* operation_name,
        TaskMessageSessionProtocolSchemaViewKind view_kind,
        util::u8 field_count,
        std::array<const char*, 1> field_names = {},
        const char* result_name = "value",
        bool supported = true) noexcept
        -> TaskMessageSessionProtocolSchemaEntry
    {
        const auto bounded_field_count =
            field_count <= field_names.size()
                ? field_count
                : static_cast<util::u8>(field_names.size());
        return TaskMessageSessionProtocolSchemaEntry{
            .operation = operation,
            .operation_name =
                operation_name != nullptr ? operation_name : "operation",
            .view_kind = view_kind,
            .field_count = bounded_field_count,
            .field_names = field_names,
            .result_name = result_name != nullptr ? result_name : "value",
            .supported = supported,
        };
    }

    [[nodiscard]] constexpr auto task_message_session_protocol_schema_entry(
        util::u64 operation,
        const char* operation_name,
        const char* payload_name,
        const char* result_name = "value") noexcept
        -> TaskMessageSessionProtocolSchemaEntry
    {
        return task_message_session_protocol_schema_entry(
            operation,
            operation_name,
            TaskMessageSessionProtocolSchemaViewKind::payload_only,
            1u,
            std::array<const char*, 1>{
                payload_name != nullptr ? payload_name : "payload",
            },
            result_name,
            true);
    }

    [[nodiscard]] constexpr auto task_message_session_protocol_schema_opaque_entry(
        util::u64 operation,
        const char* operation_name = "unmapped",
        const char* result_name = "value") noexcept
        -> TaskMessageSessionProtocolSchemaEntry
    {
        return task_message_session_protocol_schema_entry(
            operation,
            operation_name,
            TaskMessageSessionProtocolSchemaViewKind::opaque,
            0u,
            {},
            result_name,
            false);
    }

    struct TaskMessageSessionProtocolSchemaLookup {
        const TaskMessageSessionProtocolSchemaEntry* entry{nullptr};
        util::u16 slot{task_message_session_protocol_unmapped_slot};
        bool matched{false};
    };

    using TaskMessageSessionProtocolSemanticField =
        semantic::NamedValue<util::u64>;
    using TaskMessageSessionProtocolSemanticTail =
        semantic::Projection<TaskMessageSessionProtocolSchemaEntry,
                             TaskMessageSessionProtocolSemanticField,
                             1>;

    struct TaskMessageSessionProtocolSemanticProjection
        : public TaskMessageSessionProtocolSemanticTail {
        TaskMessageSessionEndpoint endpoint{};
        util::u64 operation{0};
        util::u64 payload{0};
    };

    static_assert(
        semantic::reflected_member_names_match_when_enabled<
                  TaskMessageSessionProtocolSemanticProjection>(
        std::array<std::string_view, 3>{
            "endpoint",
            "operation",
            "payload"}));

    [[nodiscard]] constexpr TaskMessageSessionProtocolSemanticProjection
    task_message_session_protocol_semantic_projection(
        TaskMessageSessionEndpointRequestView request,
        TaskMessageSessionProtocolSchemaEntry descriptor) noexcept
    {
        auto tail = descriptor.field_count > 0u
                        ? semantic::make_projection(
                              descriptor,
                              std::array<TaskMessageSessionProtocolSemanticField, 1>{
                                  semantic::named_value(
                                      descriptor.field_names[0] != nullptr
                                          ? descriptor.field_names[0]
                                          : "payload",
                                      request.payload),
                              },
                              1u,
                              descriptor.result_name)
                        : semantic::make_projection(
                              descriptor,
                              std::array<TaskMessageSessionProtocolSemanticField, 1>{},
                              0u,
                              descriptor.result_name);

        TaskMessageSessionProtocolSemanticProjection projection{};
        static_cast<TaskMessageSessionProtocolSemanticTail&>(projection) = tail;
        projection.endpoint = request.endpoint;
        projection.operation = request.operation;
        projection.payload = request.payload;

        return projection;
    }

    template <std::size_t Capacity>
    class TaskMessageSessionProtocolSchemaCatalog {
    public:
        using entry_type = TaskMessageSessionProtocolSchemaEntry;

        static_assert(Capacity > 0);

        constexpr TaskMessageSessionProtocolSchemaCatalog() noexcept = default;

        constexpr explicit TaskMessageSessionProtocolSchemaCatalog(
            std::array<entry_type, Capacity> entries) noexcept
            : entries_(entries)
        {
        }

        [[nodiscard]] bool valid() const noexcept
        {
            return true;
        }

        [[nodiscard]] static consteval std::size_t capacity() noexcept
        {
            return Capacity;
        }

        void bind_entry(std::size_t index, entry_type entry) noexcept
        {
            if (index >= Capacity) {
                return;
            }

            entries_[index] = entry;
        }

        [[nodiscard]] const entry_type* entry(std::size_t index) const noexcept
        {
            if (index >= Capacity) {
                return nullptr;
            }

            return &entries_[index];
        }

        [[nodiscard]] TaskMessageSessionProtocolSchemaLookup lookup(
            util::u64 operation) const noexcept
        {
            for (std::size_t index = 0; index < Capacity; ++index) {
                if (entries_[index].operation != operation) {
                    continue;
                }

                return TaskMessageSessionProtocolSchemaLookup{
                    .entry = &entries_[index],
                    .slot = static_cast<util::u16>(index),
                    .matched = true,
                };
            }

            return TaskMessageSessionProtocolSchemaLookup{};
        }

        [[nodiscard]] entry_type describe(util::u64 operation) const noexcept
        {
            const auto found = lookup(operation);
            if (found.entry != nullptr) {
                return *found.entry;
            }

            return task_message_session_protocol_schema_opaque_entry(operation);
        }

        [[nodiscard]] TaskMessageSessionProtocolSemanticProjection
        semantic_projection(TaskMessageSessionEndpointRequestView request)
            const noexcept
        {
            return task_message_session_protocol_semantic_projection(
                request,
                describe(request.operation));
        }

    private:
        std::array<entry_type, Capacity> entries_{};
    };

    template <std::size_t Capacity>
    [[nodiscard]] auto make_task_message_session_protocol_schema_catalog(
        std::array<TaskMessageSessionProtocolSchemaEntry, Capacity>
            entries) noexcept -> TaskMessageSessionProtocolSchemaCatalog<Capacity>
    {
        return TaskMessageSessionProtocolSchemaCatalog<Capacity>{entries};
    }

    struct TaskMessageSessionProtocolSchemaHandler {
        void* self{nullptr};
        TrapResult (*dispatch_fn)(
            void* self,
            TaskMessageSessionProtocolSemanticProjection request) noexcept {
            nullptr};

        [[nodiscard]] bool valid() const noexcept
        {
            return dispatch_fn != nullptr;
        }

        [[nodiscard]] TrapResult dispatch(
            TaskMessageSessionProtocolSemanticProjection request) const noexcept
        {
            if (!valid()) {
                return task_message_session_endpoint_unbound_adapter();
            }

            return dispatch_fn(self, request);
        }
    };

    namespace detail {
        template <typename Target>
        [[nodiscard]] TrapResult
        task_message_session_protocol_schema_handler_adapter(
            void* self,
            TaskMessageSessionProtocolSemanticProjection request) noexcept
        {
            return static_cast<Target*>(self)->dispatch(request);
        }
    }

    template <typename Target>
    [[nodiscard]] auto make_task_message_session_protocol_schema_handler(
        Target& target) noexcept -> TaskMessageSessionProtocolSchemaHandler
    {
        return TaskMessageSessionProtocolSchemaHandler{
            .self = &target,
            .dispatch_fn =
                &detail::
                    task_message_session_protocol_schema_handler_adapter<Target>,
        };
    }

    class TaskMessageSessionProtocolSchemaBinding {
    public:
        using schema_type = TaskMessageSessionProtocolSchemaEntry;

        constexpr TaskMessageSessionProtocolSchemaBinding() noexcept = default;

        constexpr explicit TaskMessageSessionProtocolSchemaBinding(
            schema_type schema,
            TaskMessageSessionProtocolSchemaHandler handler = {}) noexcept
            : schema_(schema), handler_(handler)
        {
        }

        template <typename Target>
        constexpr TaskMessageSessionProtocolSchemaBinding(
            schema_type schema,
            Target& target) noexcept
            : schema_(schema),
              handler_(make_task_message_session_protocol_schema_handler(target))
        {
        }

        [[nodiscard]] bool valid() const noexcept
        {
            return handler_.valid();
        }

        [[nodiscard]] const schema_type& schema() const noexcept
        {
            return schema_;
        }

        void bind_schema(schema_type schema) noexcept
        {
            schema_ = schema;
        }

        void bind_handler(TaskMessageSessionProtocolSchemaHandler handler) noexcept
        {
            handler_ = handler;
        }

        template <typename Target>
        void bind_target(Target& target) noexcept
        {
            handler_ = make_task_message_session_protocol_schema_handler(target);
        }

        [[nodiscard]] TrapResult dispatch(
            TaskMessageSessionEndpointRequestView request) const noexcept
        {
            if (!handler_.valid()) {
                return task_message_session_endpoint_unbound_adapter();
            }

            return handler_.dispatch(
                task_message_session_protocol_semantic_projection(
                    request,
                    schema_));
        }

        [[nodiscard]] auto protocol_entry() noexcept
            -> TaskMessageSessionProtocolEntry
        {
            return task_message_session_protocol_entry(
                schema_.operation,
                schema_.operation_name,
                make_task_message_session_protocol_request_handler(*this));
        }

    private:
        schema_type schema_{};
        TaskMessageSessionProtocolSchemaHandler handler_{};
    };

    template <typename Target>
    [[nodiscard]] auto make_task_message_session_protocol_schema_binding(
        TaskMessageSessionProtocolSchemaEntry schema,
        Target& target) noexcept -> TaskMessageSessionProtocolSchemaBinding
    {
        return TaskMessageSessionProtocolSchemaBinding{schema, target};
    }

    [[nodiscard]] inline auto task_message_session_protocol_entry(
        TaskMessageSessionProtocolSchemaBinding& binding) noexcept
        -> TaskMessageSessionProtocolEntry
    {
        return binding.protocol_entry();
    }
}
