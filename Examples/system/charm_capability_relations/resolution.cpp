#include "Modules/core/capability/relations.hpp"

#include <array>
#include <cstddef>
#include <cstdio>
#include <span>

namespace relation = charm::capability;

namespace {
    enum class ContractKey : unsigned char {
        text_sink,
        clock,
        block_device,
    };

    enum class RequirementKey : unsigned char {
        report,
        monotonic_time,
        record_store,
        unknown,
    };

    enum class ProvisionKey : unsigned char {
        console,
        clock,
        block,
        unknown,
    };

    using Requirement = relation::Requirement<ContractKey, RequirementKey>;
    using Provision = relation::Provision<ContractKey, ProvisionKey>;
    using Binding = relation::Binding<RequirementKey, ProvisionKey>;

    struct Result {
        relation::ResolutionFailure failure{relation::ResolutionFailure::none};
        std::size_t index{0};

        [[nodiscard]] constexpr bool is_ok() const noexcept {
            return failure == relation::ResolutionFailure::none;
        }
    };

    [[nodiscard]] constexpr Result validate(
        const std::span<const Requirement> requirements,
        const std::span<const Provision> provisions,
        const std::span<const Binding> bindings) noexcept {
        for (std::size_t left = 0; left < requirements.size(); ++left) {
            for (std::size_t right = left + 1U; right < requirements.size(); ++right) {
                if (requirements[left].key == requirements[right].key) {
                    return {relation::ResolutionFailure::duplicate_requirement, right};
                }
            }
        }
        for (std::size_t left = 0; left < provisions.size(); ++left) {
            for (std::size_t right = left + 1U; right < provisions.size(); ++right) {
                if (provisions[left].key == provisions[right].key) {
                    return {relation::ResolutionFailure::duplicate_provision, right};
                }
            }
        }

        for (std::size_t binding_index = 0; binding_index < bindings.size(); ++binding_index) {
            const auto& binding = bindings[binding_index];
            const Requirement* requirement = nullptr;
            const Provision* provision = nullptr;
            for (const auto& candidate : requirements) {
                if (candidate.key == binding.requirement) {
                    requirement = &candidate;
                    break;
                }
            }
            for (const auto& candidate : provisions) {
                if (candidate.key == binding.provision) {
                    provision = &candidate;
                    break;
                }
            }
            if (requirement == nullptr) {
                return {relation::ResolutionFailure::unknown_requirement, binding_index};
            }
            if (provision == nullptr) {
                return {relation::ResolutionFailure::unknown_provision, binding_index};
            }
            if (requirement->contract != provision->contract) {
                return {relation::ResolutionFailure::contract_mismatch, binding_index};
            }
        }

        for (std::size_t requirement_index = 0;
             requirement_index < requirements.size();
             ++requirement_index) {
            std::size_t count = 0;
            for (const auto& binding : bindings) {
                if (binding.requirement == requirements[requirement_index].key) {
                    ++count;
                }
            }
            if (count == 0U) {
                return {relation::ResolutionFailure::missing_binding, requirement_index};
            }
            if (count > 1U) {
                return {relation::ResolutionFailure::duplicate_binding, requirement_index};
            }
        }
        return {relation::ResolutionFailure::none, requirements.size()};
    }

    constexpr std::array requirements{
        Requirement{RequirementKey::report, ContractKey::text_sink},
        Requirement{RequirementKey::monotonic_time, ContractKey::clock},
        Requirement{RequirementKey::record_store, ContractKey::block_device},
    };
    constexpr std::array provisions{
        Provision{ProvisionKey::console, ContractKey::text_sink},
        Provision{ProvisionKey::clock, ContractKey::clock},
        Provision{ProvisionKey::block, ContractKey::block_device},
    };
    constexpr std::array bindings{
        Binding{RequirementKey::report, ProvisionKey::console},
        Binding{RequirementKey::monotonic_time, ProvisionKey::clock},
        Binding{RequirementKey::record_store, ProvisionKey::block},
    };

    static_assert(validate(requirements, provisions, bindings).is_ok());

    constexpr auto duplicate_requirements = [] {
        auto value = requirements;
        value[2].key = RequirementKey::report;
        return value;
    }();
    static_assert(validate(duplicate_requirements, provisions, bindings).failure ==
                  relation::ResolutionFailure::duplicate_requirement);

    constexpr auto duplicate_provisions = [] {
        auto value = provisions;
        value[2].key = ProvisionKey::console;
        return value;
    }();
    static_assert(validate(requirements, duplicate_provisions, bindings).failure ==
                  relation::ResolutionFailure::duplicate_provision);

    constexpr std::array missing_binding{bindings[0], bindings[1]};
    static_assert(validate(requirements, provisions, missing_binding).failure ==
                  relation::ResolutionFailure::missing_binding);

    constexpr std::array duplicate_binding{bindings[0], bindings[1], bindings[2], bindings[0]};
    static_assert(validate(requirements, provisions, duplicate_binding).failure ==
                  relation::ResolutionFailure::duplicate_binding);

    constexpr auto unknown_requirement = [] {
        auto value = bindings;
        value[0].requirement = RequirementKey::unknown;
        return value;
    }();
    static_assert(validate(requirements, provisions, unknown_requirement).failure ==
                  relation::ResolutionFailure::unknown_requirement);

    constexpr auto unknown_provision = [] {
        auto value = bindings;
        value[0].provision = ProvisionKey::unknown;
        return value;
    }();
    static_assert(validate(requirements, provisions, unknown_provision).failure ==
                  relation::ResolutionFailure::unknown_provision);

    constexpr auto mismatch = [] {
        auto value = bindings;
        value[0].provision = ProvisionKey::clock;
        return value;
    }();
    static_assert(validate(requirements, provisions, mismatch).failure ==
                  relation::ResolutionFailure::contract_mismatch);
}

int main() {
    if (!validate(requirements, provisions, bindings).is_ok()) {
        return 1;
    }
    std::puts("[charm-capability-relations-resolution] ok");
    return 0;
}
