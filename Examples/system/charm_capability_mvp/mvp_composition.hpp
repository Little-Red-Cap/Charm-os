#pragma once

#include "mvp_contracts.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace charm::mvp {
    enum class ContractId : std::uint8_t {
        text_sink = 0,
        clock,
        block_device,
    };

    enum class RoleId : std::uint8_t {
        report = 0,
        monotonic_time,
        record_store,
    };

    struct Requirement {
        ContractId contract{ContractId::text_sink};
        RoleId role{RoleId::report};

        [[nodiscard]] constexpr bool operator==(const Requirement&) const noexcept = default;
    };

    struct Provision {
        ContractId contract{ContractId::text_sink};
        const TextSink* text_sink{nullptr};
        const Clock* clock{nullptr};
        const BlockDevice* block_device{nullptr};

        [[nodiscard]] static constexpr Provision for_text_sink(
            const TextSink& endpoint) noexcept {
            return Provision{ContractId::text_sink, &endpoint, nullptr, nullptr};
        }

        [[nodiscard]] static constexpr Provision for_clock(
            const Clock& endpoint) noexcept {
            return Provision{ContractId::clock, nullptr, &endpoint, nullptr};
        }

        [[nodiscard]] static constexpr Provision for_block_device(
            const BlockDevice& endpoint) noexcept {
            return Provision{ContractId::block_device, nullptr, nullptr, &endpoint};
        }

        [[nodiscard]] constexpr bool valid() const noexcept {
            switch (contract) {
            case ContractId::text_sink:
                return text_sink != nullptr && clock == nullptr && block_device == nullptr &&
                       text_sink->valid();
            case ContractId::clock:
                return text_sink == nullptr && clock != nullptr && block_device == nullptr &&
                       clock->valid();
            case ContractId::block_device:
                return text_sink == nullptr && clock == nullptr && block_device != nullptr &&
                       block_device->valid();
            }
            return false;
        }
    };

    struct Binding {
        Requirement requirement{};
        std::size_t provision_index{0};
    };

    struct ProfileView {
        std::span<const Provision> provisions{};
        std::span<const Binding> bindings{};
    };

    enum class ResolutionFailure : std::uint8_t {
        none = 0,
        missing_binding,
        duplicate_binding,
        invalid_provision_index,
        contract_mismatch,
        invalid_provision,
    };

    [[nodiscard]] constexpr const char* resolution_failure_name(
        ResolutionFailure failure) noexcept {
        switch (failure) {
        case ResolutionFailure::none:
            return "none";
        case ResolutionFailure::missing_binding:
            return "missing_binding";
        case ResolutionFailure::duplicate_binding:
            return "duplicate_binding";
        case ResolutionFailure::invalid_provision_index:
            return "invalid_provision_index";
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

        for (std::size_t requirement_index = 0;
             requirement_index < requirements.size();
             ++requirement_index) {
            const auto requirement = requirements[requirement_index];
            const Binding* selected = nullptr;

            for (const auto& binding : profile.bindings) {
                if (binding.requirement != requirement) {
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
            if (selected->provision_index >= profile.provisions.size()) {
                return {ResolutionFailure::invalid_provision_index, requirement_index, {}};
            }

            const auto& provision = profile.provisions[selected->provision_index];
            if (provision.contract != requirement.contract) {
                return {ResolutionFailure::contract_mismatch, requirement_index, {}};
            }
            if (!provision.valid()) {
                return {ResolutionFailure::invalid_provision, requirement_index, {}};
            }
            switch (requirement.contract) {
            case ContractId::text_sink: {
                const auto* endpoint = provision.text_sink;
                if (requirement.role != RoleId::report) {
                    return {ResolutionFailure::invalid_provision, requirement_index, {}};
                }
                context.report = endpoint;
                break;
            }
            case ContractId::clock: {
                const auto* endpoint = provision.clock;
                if (requirement.role != RoleId::monotonic_time) {
                    return {ResolutionFailure::invalid_provision, requirement_index, {}};
                }
                context.monotonic_time = endpoint;
                break;
            }
            case ContractId::block_device: {
                const auto* endpoint = provision.block_device;
                if (requirement.role != RoleId::record_store) {
                    return {ResolutionFailure::invalid_provision, requirement_index, {}};
                }
                context.record_store = endpoint;
                break;
            }
            }
        }

        if (!context.valid()) {
            return {ResolutionFailure::missing_binding, requirements.size(), {}};
        }
        return {ResolutionFailure::none, requirements.size(), context};
    }
}
