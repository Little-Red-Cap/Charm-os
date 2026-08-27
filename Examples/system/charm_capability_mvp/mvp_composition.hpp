#pragma once

#include "mvp_contracts.hpp"
#include "../../../Modules/core/capability/relations.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace charm::mvp {
    enum class ContractKey : std::uint8_t {
        text_sink = 0,
        clock,
        block_device,
    };

    enum class RequirementKey : std::uint8_t {
        report = 0,
        monotonic_time,
        record_store,
        unknown,
    };

    enum class ProvisionKey : std::uint8_t {
        text_sink = 0,
        clock,
        block_device,
        unknown,
    };

    using Requirement = capability::Requirement<ContractKey, RequirementKey>;
    using ProvisionRelation = capability::Provision<ContractKey, ProvisionKey>;
    using Binding = capability::Binding<RequirementKey, ProvisionKey>;
    using ResolvedBinding = capability::ResolvedBinding<RequirementKey, ProvisionKey>;
    using ResolutionFailure = capability::ResolutionFailure;

    struct Provision {
        ProvisionRelation relation{};
        const TextSink* text_sink{nullptr};
        const Clock* clock{nullptr};
        const BlockDevice* block_device{nullptr};

        [[nodiscard]] static constexpr Provision for_text_sink(
            const TextSink& endpoint) noexcept {
            return Provision{
                {ProvisionKey::text_sink, ContractKey::text_sink},
                &endpoint,
                nullptr,
                nullptr,
            };
        }

        [[nodiscard]] static constexpr Provision for_clock(
            const Clock& endpoint) noexcept {
            return Provision{
                {ProvisionKey::clock, ContractKey::clock},
                nullptr,
                &endpoint,
                nullptr,
            };
        }

        [[nodiscard]] static constexpr Provision for_block_device(
            const BlockDevice& endpoint) noexcept {
            return Provision{
                {ProvisionKey::block_device, ContractKey::block_device},
                nullptr,
                nullptr,
                &endpoint,
            };
        }

        [[nodiscard]] static constexpr Provision invalid(
            const ProvisionKey key,
            const ContractKey contract) noexcept {
            return Provision{{key, contract}, nullptr, nullptr, nullptr};
        }

        [[nodiscard]] constexpr bool valid() const noexcept {
            switch (relation.contract) {
            case ContractKey::text_sink:
                return text_sink != nullptr && clock == nullptr && block_device == nullptr &&
                       text_sink->valid();
            case ContractKey::clock:
                return text_sink == nullptr && clock != nullptr && block_device == nullptr &&
                       clock->valid();
            case ContractKey::block_device:
                return text_sink == nullptr && clock == nullptr && block_device != nullptr &&
                       block_device->valid();
            }
            return false;
        }
    };

    struct ProfileView {
        std::span<const Provision> provisions{};
        std::span<const Binding> bindings{};
    };

    [[nodiscard]] constexpr const char* resolution_failure_name(
        ResolutionFailure failure) noexcept {
        switch (failure) {
        case ResolutionFailure::none:
            return "none";
        case ResolutionFailure::duplicate_requirement:
            return "duplicate_requirement";
        case ResolutionFailure::duplicate_provision:
            return "duplicate_provision";
        case ResolutionFailure::missing_binding:
            return "missing_binding";
        case ResolutionFailure::duplicate_binding:
            return "duplicate_binding";
        case ResolutionFailure::unknown_requirement:
            return "unknown_requirement";
        case ResolutionFailure::unknown_provision:
            return "unknown_provision";
        case ResolutionFailure::contract_mismatch:
            return "contract_mismatch";
        case ResolutionFailure::invalid_provision:
            return "invalid_provision";
        }
        return "unknown";
    }

    struct ResolvedContext {
        const TextSink* report{nullptr};
        const Clock* monotonic_time{nullptr};
        const BlockDevice* record_store{nullptr};

        [[nodiscard]] constexpr bool valid() const noexcept {
            return report != nullptr && monotonic_time != nullptr && record_store != nullptr;
        }
    };

    struct ResolutionResult {
        ResolutionFailure failure{ResolutionFailure::none};
        std::size_t requirement_index{0};
        ResolvedContext context{};

        [[nodiscard]] constexpr bool is_ok() const noexcept {
            return failure == ResolutionFailure::none && context.valid();
        }
    };

    [[nodiscard]] inline ResolutionResult resolve(
        std::span<const Requirement> requirements,
        ProfileView profile) noexcept {
        ResolvedContext context{};

        for (std::size_t left = 0; left < requirements.size(); ++left) {
            for (std::size_t right = left + 1U; right < requirements.size(); ++right) {
                if (requirements[left].key == requirements[right].key) {
                    return {ResolutionFailure::duplicate_requirement, right, {}};
                }
            }
        }
        for (std::size_t left = 0; left < profile.provisions.size(); ++left) {
            for (std::size_t right = left + 1U; right < profile.provisions.size(); ++right) {
                if (profile.provisions[left].relation.key ==
                    profile.provisions[right].relation.key) {
                    return {ResolutionFailure::duplicate_provision, right, {}};
                }
            }
        }

        for (std::size_t binding_index = 0;
             binding_index < profile.bindings.size();
             ++binding_index) {
            const auto& binding = profile.bindings[binding_index];
            bool requirement_known = false;
            bool provision_known = false;
            for (const auto& requirement : requirements) {
                requirement_known = requirement_known || requirement.key == binding.requirement;
            }
            for (const auto& provision : profile.provisions) {
                provision_known = provision_known || provision.relation.key == binding.provision;
            }
            if (!requirement_known) {
                return {ResolutionFailure::unknown_requirement, binding_index, {}};
            }
            if (!provision_known) {
                return {ResolutionFailure::unknown_provision, binding_index, {}};
            }
        }

        for (std::size_t requirement_index = 0;
             requirement_index < requirements.size();
             ++requirement_index) {
            const auto requirement = requirements[requirement_index];
            const Binding* selected = nullptr;

            for (const auto& binding : profile.bindings) {
                if (binding.requirement != requirement.key) {
                    continue;
                }
                if (selected != nullptr) {
                    return {ResolutionFailure::duplicate_binding, requirement_index, {}};
                }
                selected = &binding;
            }

            if (selected == nullptr) {
                return {ResolutionFailure::missing_binding, requirement_index, {}};
            }

            const Provision* provision = nullptr;
            for (const auto& candidate : profile.provisions) {
                if (candidate.relation.key == selected->provision) {
                    provision = &candidate;
                    break;
                }
            }
            if (provision == nullptr) {
                return {ResolutionFailure::unknown_provision, requirement_index, {}};
            }
            if (provision->relation.contract != requirement.contract) {
                return {ResolutionFailure::contract_mismatch, requirement_index, {}};
            }
            if (!provision->valid()) {
                return {ResolutionFailure::invalid_provision, requirement_index, {}};
            }
            const ResolvedBinding resolved{requirement.key, provision->relation.key};
            switch (resolved.requirement) {
            case RequirementKey::report: {
                if (requirement.contract != ContractKey::text_sink) {
                    return {ResolutionFailure::invalid_provision, requirement_index, {}};
                }
                context.report = provision->text_sink;
                break;
            }
            case RequirementKey::monotonic_time: {
                if (requirement.contract != ContractKey::clock) {
                    return {ResolutionFailure::invalid_provision, requirement_index, {}};
                }
                context.monotonic_time = provision->clock;
                break;
            }
            case RequirementKey::record_store: {
                if (requirement.contract != ContractKey::block_device) {
                    return {ResolutionFailure::invalid_provision, requirement_index, {}};
                }
                context.record_store = provision->block_device;
                break;
            }
            case RequirementKey::unknown:
                return {ResolutionFailure::unknown_requirement, requirement_index, {}};
                break;
            }
        }

        if (!context.valid()) {
            return {ResolutionFailure::missing_binding, requirements.size(), {}};
        }
        return {ResolutionFailure::none, requirements.size(), context};
    }
}
