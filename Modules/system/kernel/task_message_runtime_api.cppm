module;

#include <array>
#include <cstddef>
#include <string_view>

export module kernel.task_message_runtime_api;

export import kernel.task_message_runtime_service;
import semantic.core;
import util.core;

export namespace kernel {
    enum class TaskMessageRuntimeApiWitnessKind : util::u8 {
        issue = 0,
        reply,
        timeout,
        completion_drop,
    };

    [[nodiscard]] constexpr const char* task_message_runtime_api_witness_kind_name(
        TaskMessageRuntimeApiWitnessKind kind) noexcept
    {
        switch (kind) {
        case TaskMessageRuntimeApiWitnessKind::issue:
            return "issue";
        case TaskMessageRuntimeApiWitnessKind::reply:
            return "reply";
        case TaskMessageRuntimeApiWitnessKind::timeout:
            return "timeout";
        case TaskMessageRuntimeApiWitnessKind::completion_drop:
            return "completion-drop";
        }
        return "unknown";
    }

    struct TaskMessageRuntimeApiWitness {
        bool ready{false};
        bool state_observed{false};
        bool valid{false};
        bool busy{false};
        std::size_t pending_requests{0};
        std::size_t pending_completions{0};
        TaskMessageRuntimeApiWitnessKind kind{
            TaskMessageRuntimeApiWitnessKind::issue};
        TaskId owner{};
        util::u64 token{0};
        util::u64 request_sequence{0};
        util::u64 wait_due{0};
        bool issued{false};
        bool completion_ready{false};
        bool completion_pushed{false};
        bool completion_dropped{false};
        bool timeout{false};
        util::u64 reply_value{0};
        TrapDisposition disposition{TrapDisposition::rejected};
        TrapError error{TrapError::none};
        bool has_lower_provenance{false};
        TaskMessageRuntimeServiceWitness lower_provenance{};

        [[nodiscard]] constexpr bool issue_branch_ok() const noexcept
        {
            return kind == TaskMessageRuntimeApiWitnessKind::issue && issued &&
                   owner != TaskId{} && token != 0u &&
                   request_sequence != 0u;
        }

        [[nodiscard]] constexpr bool reply_branch_ok() const noexcept
        {
            return kind == TaskMessageRuntimeApiWitnessKind::reply &&
                   completion_ready && completion_pushed &&
                   !completion_dropped && !timeout;
        }

        [[nodiscard]] constexpr bool timeout_branch_ok() const noexcept
        {
            return kind == TaskMessageRuntimeApiWitnessKind::timeout &&
                   completion_ready && completion_pushed &&
                   !completion_dropped && timeout;
        }

        [[nodiscard]] constexpr bool completion_drop_branch() const noexcept
        {
            return kind == TaskMessageRuntimeApiWitnessKind::completion_drop &&
                   completion_ready && completion_dropped &&
                   !completion_pushed;
        }

        [[nodiscard]] constexpr bool lower_route_consistent() const noexcept
        {
            if (!has_lower_provenance) {
                return true;
            }

            switch (kind) {
            case TaskMessageRuntimeApiWitnessKind::issue:
                return lower_provenance.verdict() ==
                           semantic::Verdict::standing &&
                       lower_provenance.kind ==
                           TaskMessageRuntimeServiceWitnessKind::issue &&
                       lower_provenance.owner == owner &&
                       lower_provenance.token == token &&
                       lower_provenance.request_sequence ==
                           request_sequence &&
                       lower_provenance.wait_due == wait_due;
            case TaskMessageRuntimeApiWitnessKind::reply:
                return lower_provenance.verdict() ==
                           semantic::Verdict::standing &&
                       lower_provenance.kind ==
                           TaskMessageRuntimeServiceWitnessKind::reply &&
                       lower_provenance.owner == owner &&
                       lower_provenance.token == token &&
                       lower_provenance.request_sequence ==
                           request_sequence &&
                       !lower_provenance.timeout &&
                       lower_provenance.reply_value == reply_value &&
                       lower_provenance.disposition == disposition &&
                       lower_provenance.error == error;
            case TaskMessageRuntimeApiWitnessKind::timeout:
                return lower_provenance.verdict() ==
                           semantic::Verdict::standing &&
                       lower_provenance.kind ==
                           TaskMessageRuntimeServiceWitnessKind::timeout &&
                       lower_provenance.owner == owner &&
                       lower_provenance.token == token &&
                       lower_provenance.request_sequence ==
                           request_sequence &&
                       lower_provenance.timeout &&
                       lower_provenance.reply_value == reply_value &&
                       lower_provenance.disposition == disposition &&
                       lower_provenance.error == error;
            case TaskMessageRuntimeApiWitnessKind::completion_drop:
                return lower_provenance.ready &&
                       lower_provenance.kind ==
                           TaskMessageRuntimeServiceWitnessKind::completion_drop &&
                       lower_provenance.owner == owner &&
                       lower_provenance.token == token &&
                       lower_provenance.request_sequence ==
                           request_sequence &&
                       lower_provenance.timeout == timeout &&
                       lower_provenance.reply_value == reply_value &&
                       lower_provenance.disposition == disposition &&
                       lower_provenance.error == error;
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

            if (completion_drop_branch()) {
                return semantic::Verdict::drifted;
            }

            if (!(issue_branch_ok() || reply_branch_ok() ||
                  timeout_branch_ok())) {
                return semantic::Verdict::drifted;
            }

            if (!lower_route_consistent()) {
                return semantic::Verdict::drifted;
            }

            return semantic::Verdict::standing;
        }

        [[nodiscard]] constexpr semantic::FailureDomain
        failure_domain() const noexcept
        {
            if (!ready) {
                return semantic::FailureDomain::input;
            }

            if (completion_drop_branch()) {
                return semantic::FailureDomain::handoff;
            }

            if (kind == TaskMessageRuntimeApiWitnessKind::issue) {
                if (!issued || owner == TaskId{} || token == 0u ||
                    request_sequence == 0u) {
                    return semantic::FailureDomain::selection;
                }
            } else {
                const bool completion_shape_mismatch =
                    !completion_ready ||
                    !completion_pushed ||
                    completion_dropped ||
                    (kind == TaskMessageRuntimeApiWitnessKind::reply &&
                     timeout) ||
                    (kind == TaskMessageRuntimeApiWitnessKind::timeout &&
                     !timeout);
                if (completion_shape_mismatch) {
                    return semantic::FailureDomain::handoff;
                }
            }

            if (!lower_route_consistent()) {
                return semantic::FailureDomain::route;
            }

            return semantic::FailureDomain::none;
        }

        [[nodiscard]] constexpr std::string_view summary_path() const noexcept
        {
            return "task-message-runtime-api-witness.summary";
        }
    };

    struct TaskMessageRuntimeApiWitnessHandoffTarget {
        const TaskMessageRuntimeApiWitness* witness{nullptr};

        [[nodiscard]] constexpr std::string_view entry_name() const noexcept
        {
            return "task-message-runtime-api-witness";
        }

        [[nodiscard]] constexpr std::string_view
        selected_summary_path() const noexcept
        {
            return witness != nullptr ? witness->summary_path()
                                      : std::string_view{
                                            "task-message-runtime-api-witness.summary"};
        }
    };

    static_assert(
        semantic::reflected_member_names_match_when_enabled<TaskMessageRuntimeApiWitness>(
            std::array<std::string_view, 21>{
                "ready",
                "state_observed",
                "valid",
                "busy",
                "pending_requests",
                "pending_completions",
                "kind",
                "owner",
                "token",
                "request_sequence",
                "wait_due",
                "issued",
                "completion_ready",
                "completion_pushed",
                "completion_dropped",
                "timeout",
                "reply_value",
                "disposition",
                "error",
                "has_lower_provenance",
                "lower_provenance",
            }));

    static_assert(semantic::WitnessCarrier<TaskMessageRuntimeApiWitness>);
    static_assert(
        semantic::HandoffTarget<TaskMessageRuntimeApiWitnessHandoffTarget>);

    template <typename Result>
    [[nodiscard]] constexpr TaskMessageRuntimeApiWitness
    task_message_runtime_api_witness(const Result& result) noexcept
    {
        auto witness = TaskMessageRuntimeApiWitness{};
        witness.ready = result.issued || result.completion_ready ||
                        result.completion_dropped;
        witness.issued = result.issued;
        witness.completion_ready = result.completion_ready;
        witness.completion_pushed = result.completion_pushed;
        witness.completion_dropped = result.completion_dropped;
        if (result.completion_dropped) {
            witness.kind = TaskMessageRuntimeApiWitnessKind::completion_drop;
            witness.owner = result.completion.owner;
            witness.token = result.completion.token;
            witness.request_sequence = result.completion.request_sequence;
            witness.timeout = result.completion.timeout;
            witness.reply_value = result.completion.trap.value;
            witness.disposition = result.completion.trap.disposition;
            witness.error = result.completion.trap.error;
            return witness;
        }

        if (result.completion_ready) {
            witness.kind = result.completion.timeout
                               ? TaskMessageRuntimeApiWitnessKind::timeout
                               : TaskMessageRuntimeApiWitnessKind::reply;
            witness.owner = result.completion.owner;
            witness.token = result.completion.token;
            witness.request_sequence = result.completion.request_sequence;
            witness.timeout = result.completion.timeout;
            witness.reply_value = result.completion.trap.value;
            witness.disposition = result.completion.trap.disposition;
            witness.error = result.completion.trap.error;
            return witness;
        }

        if (result.issued) {
            witness.kind = TaskMessageRuntimeApiWitnessKind::issue;
            witness.owner = result.issued_request.owner;
            witness.token = result.issued_request.token;
            witness.request_sequence = result.issued_request.request_sequence;
            witness.wait_due =
                static_cast<util::u64>(result.issued_request.wait_due);
        }

        return witness;
    }

    template <typename Runtime, typename Result>
    [[nodiscard]] constexpr TaskMessageRuntimeApiWitness
    task_message_runtime_api_witness(const Runtime& runtime,
                                     const Result& result) noexcept
    {
        auto witness = task_message_runtime_api_witness(result);
        witness.state_observed = true;
        witness.valid = runtime.valid();
        witness.busy = runtime.busy();
        witness.pending_requests = runtime.pending_requests();
        witness.pending_completions = runtime.pending_completions();
        return witness;
    }

    template <typename Result>
    [[nodiscard]] constexpr TaskMessageRuntimeApiWitness
    task_message_runtime_api_witness(
        const Result& result,
        const TaskMessageRuntimeServiceWitness& lower) noexcept
    {
        auto witness = task_message_runtime_api_witness(result);
        witness.has_lower_provenance = true;
        witness.lower_provenance = lower;
        return witness;
    }

    template <typename Runtime, typename Result>
    [[nodiscard]] constexpr TaskMessageRuntimeApiWitness
    task_message_runtime_api_witness(
        const Runtime& runtime,
        const Result& result,
        const TaskMessageRuntimeServiceWitness& lower) noexcept
    {
        auto witness = task_message_runtime_api_witness(runtime, result);
        witness.has_lower_provenance = true;
        witness.lower_provenance = lower;
        return witness;
    }

    [[nodiscard]] constexpr bool task_message_runtime_api_witness_ready(
        const TaskMessageRuntimeApiWitness& witness) noexcept
    {
        return witness.ready;
    }

    [[nodiscard]] constexpr TaskMessageRuntimeApiWitnessHandoffTarget
    task_message_runtime_api_witness_handoff_target(
        const TaskMessageRuntimeApiWitness& witness) noexcept
    {
        return TaskMessageRuntimeApiWitnessHandoffTarget{
            .witness = &witness,
        };
    }

    template <typename Services>
    class TaskMessageRuntimeApi {
    public:
        using services_type = Services;
        using tick_type = typename Services::tick_type;
        using completion_type = typename Services::completion_type;
        using result_type = typename Services::result_type;

        constexpr TaskMessageRuntimeApi() noexcept = default;

        constexpr explicit TaskMessageRuntimeApi(Services services) noexcept
            : services_(services)
        {
        }

        [[nodiscard]] bool valid() const noexcept
        {
            return services_.valid();
        }

        [[nodiscard]] bool busy() const noexcept
        {
            return services_.busy();
        }

        [[nodiscard]] std::size_t pending_requests() const noexcept
        {
            return services_.pending_requests();
        }

        [[nodiscard]] std::size_t pending_completions() const noexcept
        {
            return services_.pending_completions();
        }

        [[nodiscard]] Services& services() noexcept
        {
            return services_;
        }

        [[nodiscard]] const Services& services() const noexcept
        {
            return services_;
        }

        void bind_services(Services services) noexcept
        {
            services_ = services;
        }

        void bind_cursors(util::u64 next_token,
                          util::u64 next_sequence) noexcept
        {
            services_.bind_cursors(next_token, next_sequence);
        }

        [[nodiscard]] bool yield(tick_type wait_due) noexcept
        {
            return yield(TrapYieldCurrentView{}, wait_due);
        }

        [[nodiscard]] bool yield(TrapYieldCurrentView yield_view,
                                 tick_type wait_due) noexcept
        {
            return services_.yield_current(yield_view, wait_due);
        }

        [[nodiscard]] bool sleep_until(tick_type due,
                                       tick_type wait_due) noexcept
        {
            return sleep_until(TrapSleepUntilView<tick_type>{
                                   .due = due,
                               },
                               wait_due);
        }

        template <typename Tick>
        [[nodiscard]] bool sleep_until(TrapSleepUntilView<Tick> sleep,
                                       tick_type wait_due) noexcept
        {
            return services_.sleep_current_until(sleep, wait_due);
        }

        [[nodiscard]] bool debug_write(util::u64 value,
                                       tick_type wait_due) noexcept
        {
            return debug_write(TrapDebugWriteView{
                                   .value = value,
                               },
                               wait_due);
        }

        [[nodiscard]] bool debug_write(TrapDebugWriteView write,
                                       tick_type wait_due) noexcept
        {
            return services_.debug_write(write, wait_due);
        }

        [[nodiscard]] bool capability_call(util::u64 capability_id,
                                           util::u64 operation,
                                           util::u64 payload,
                                           tick_type wait_due) noexcept
        {
            return capability_call(TrapCapabilityCallView{
                                       .capability_id = capability_id,
                                       .operation = operation,
                                       .payload = payload,
                                   },
                                   wait_due);
        }

        [[nodiscard]] bool capability_call(TrapCapabilityCallView capability,
                                           tick_type wait_due) noexcept
        {
            return services_.capability_call(capability, wait_due);
        }

        [[nodiscard]] bool kick() noexcept
        {
            return services_.kick();
        }

        [[nodiscard]] result_type step(Event event) noexcept
        {
            return services_.step(event);
        }

        [[nodiscard]] bool receive_completion(completion_type& out) noexcept
        {
            return services_.receive_completion(out);
        }

    private:
        Services services_{};
    };

    template <typename Services>
    [[nodiscard]] auto make_task_message_runtime_api(
        Services services) noexcept -> TaskMessageRuntimeApi<Services>
    {
        return TaskMessageRuntimeApi<Services>{services};
    }
}
