#pragma once

#include <type_traits>

namespace charm::capability {
    template <typename ContractKey, typename RequirementKey>
    struct Requirement {
        static_assert(std::is_scoped_enum_v<ContractKey>);
        static_assert(std::is_scoped_enum_v<RequirementKey>);
        static_assert(!std::is_same_v<ContractKey, RequirementKey>);

        RequirementKey key{};
        ContractKey contract{};

        [[nodiscard]] constexpr bool operator==(const Requirement&) const noexcept = default;
    };

    template <typename ContractKey, typename ProvisionKey>
    struct Provision {
        static_assert(std::is_scoped_enum_v<ContractKey>);
        static_assert(std::is_scoped_enum_v<ProvisionKey>);
        static_assert(!std::is_same_v<ContractKey, ProvisionKey>);

        ProvisionKey key{};
        ContractKey contract{};

        [[nodiscard]] constexpr bool operator==(const Provision&) const noexcept = default;
    };

    template <typename RequirementKey, typename ProvisionKey>
    struct Binding {
        static_assert(std::is_scoped_enum_v<RequirementKey>);
        static_assert(std::is_scoped_enum_v<ProvisionKey>);
        static_assert(!std::is_same_v<RequirementKey, ProvisionKey>);

        RequirementKey requirement{};
        ProvisionKey provision{};

        [[nodiscard]] constexpr bool operator==(const Binding&) const noexcept = default;
    };

}
