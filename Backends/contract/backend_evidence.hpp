#pragma once

#include <cstddef>
#include <span>
#include <string_view>

namespace charm::backend::contract {
    enum class BackendKind {
        host,
        qemu,
        board,
    };

    enum class BackendStatus {
        prototype,
        reference,
        landing,
    };

    struct BackendIdentity {
        BackendKind kind{BackendKind::host};
        std::string_view name{};
        std::string_view architecture{};
        BackendStatus status{BackendStatus::prototype};
    };

    enum class FactRequirement {
        required,
        optional,
    };

    enum class FactState {
        provided,
        missing,
        unknown,
    };

    struct BackendFact {
        std::string_view kind{};
        std::string_view name{};
        FactRequirement requirement{FactRequirement::required};
        FactState state{FactState::unknown};
        std::string_view source{};
    };

    struct CapabilityExport {
        std::string_view capability_name{};
        std::string_view requirement_role{};
        std::string_view provider_instance{};
        std::string_view provider_type{};
        std::string_view runtime_domain{};
        std::string_view adapter{};
    };

    struct BindingEvidence {
        std::string_view capability_name{};
        std::string_view requirement_role{};
        std::string_view provider_instance{};
        std::string_view selection{};
    };

    struct BackendEvidenceView {
        BackendIdentity identity{};
        std::span<const CapabilityExport> capability_exports{};
        std::span<const BindingEvidence> selected_bindings{};
        std::span<const BackendFact> facts{};
    };

    [[nodiscard]] constexpr std::size_t count_required_facts(const std::span<const BackendFact> facts) noexcept {
        std::size_t count = 0;
        for (const auto& fact : facts) {
            if (fact.requirement == FactRequirement::required) {
                ++count;
            }
        }
        return count;
    }

    [[nodiscard]] constexpr std::size_t count_provided_facts(const std::span<const BackendFact> facts) noexcept {
        std::size_t count = 0;
        for (const auto& fact : facts) {
            if (fact.state == FactState::provided) {
                ++count;
            }
        }
        return count;
    }

    [[nodiscard]] constexpr std::size_t count_missing_required_facts(const std::span<const BackendFact> facts) noexcept {
        std::size_t count = 0;
        for (const auto& fact : facts) {
            if (fact.requirement == FactRequirement::required && fact.state == FactState::missing) {
                ++count;
            }
        }
        return count;
    }

    [[nodiscard]] constexpr std::size_t count_unknown_required_facts(const std::span<const BackendFact> facts) noexcept {
        std::size_t count = 0;
        for (const auto& fact : facts) {
            if (fact.requirement == FactRequirement::required && fact.state == FactState::unknown) {
                ++count;
            }
        }
        return count;
    }

    [[nodiscard]] constexpr bool required_facts_ready(const std::span<const BackendFact> facts) noexcept {
        return count_missing_required_facts(facts) == 0U &&
               count_unknown_required_facts(facts) == 0U;
    }

    struct BackendEvidenceSummary {
        std::size_t capability_export_count{0};
        std::size_t selected_binding_count{0};
        std::size_t required_fact_count{0};
        std::size_t provided_fact_count{0};
        std::size_t missing_required_fact_count{0};
        std::size_t unknown_required_fact_count{0};
        bool required_facts_are_ready{false};
    };

    [[nodiscard]] constexpr BackendEvidenceSummary summarize_backend_evidence(
        const BackendEvidenceView& view) noexcept {
        return BackendEvidenceSummary{
            .capability_export_count = view.capability_exports.size(),
            .selected_binding_count = view.selected_bindings.size(),
            .required_fact_count = count_required_facts(view.facts),
            .provided_fact_count = count_provided_facts(view.facts),
            .missing_required_fact_count = count_missing_required_facts(view.facts),
            .unknown_required_fact_count = count_unknown_required_facts(view.facts),
            .required_facts_are_ready = required_facts_ready(view.facts),
        };
    }
}
