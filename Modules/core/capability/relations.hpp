#pragma once

#include <type_traits>

namespace charm::capability {
    template <typename ContractKey, typename RequirementKey>
    struct Requirement {
        static_assert(std::is_scoped_enum_v<ContractKey>);
        static_assert(std::is_scoped_enum_v<RequirementKey>);
        static_assert(
            !std::is_same_v<ContractKey, RequirementKey>,
            "CHARM_CAPABILITY_REQUIREMENT_KEY_DOMAINS_MUST_DIFFER");

        constexpr Requirement(RequirementKey key_value,
                              ContractKey contract_value) noexcept
            : key(key_value), contract(contract_value) {}

        RequirementKey key;
        ContractKey contract;

        [[nodiscard]] constexpr bool operator==(const Requirement&) const noexcept = default;
    };

    template <typename ContractKey, typename ProvisionKey>
    struct Provision {
        static_assert(std::is_scoped_enum_v<ContractKey>);
        static_assert(std::is_scoped_enum_v<ProvisionKey>);
        static_assert(
            !std::is_same_v<ContractKey, ProvisionKey>,
            "CHARM_CAPABILITY_PROVISION_KEY_DOMAINS_MUST_DIFFER");

        constexpr Provision(ProvisionKey key_value,
                            ContractKey contract_value) noexcept
            : key(key_value), contract(contract_value) {}

        ProvisionKey key;
        ContractKey contract;

        [[nodiscard]] constexpr bool operator==(const Provision&) const noexcept = default;
    };

    template <typename RequirementKey, typename ProvisionKey>
    struct Binding {
        static_assert(std::is_scoped_enum_v<RequirementKey>);
        static_assert(std::is_scoped_enum_v<ProvisionKey>);
        static_assert(
            !std::is_same_v<RequirementKey, ProvisionKey>,
            "CHARM_CAPABILITY_BINDING_KEY_DOMAINS_MUST_DIFFER");

        constexpr Binding(RequirementKey requirement_value,
                          ProvisionKey provision_value) noexcept
            : requirement(requirement_value), provision(provision_value) {}

        RequirementKey requirement;
        ProvisionKey provision;

        [[nodiscard]] constexpr bool operator==(const Binding&) const noexcept = default;
    };

}
