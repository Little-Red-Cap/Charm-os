#pragma once

#include <cstdint>
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

    template <typename RequirementKey, typename ProvisionKey>
    struct ResolvedBinding {
        static_assert(std::is_scoped_enum_v<RequirementKey>);
        static_assert(std::is_scoped_enum_v<ProvisionKey>);
        static_assert(!std::is_same_v<RequirementKey, ProvisionKey>);

        RequirementKey requirement{};
        ProvisionKey provision{};

        [[nodiscard]] constexpr bool operator==(const ResolvedBinding&) const noexcept = default;
    };

    enum class ResolutionFailure : std::uint8_t {
        none = 0,
        duplicate_requirement,
        duplicate_provision,
        missing_binding,
        duplicate_binding,
        unknown_requirement,
        unknown_provision,
        contract_mismatch,
        invalid_provision,
    };
}
