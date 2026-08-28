#include "core/capability/relations.hpp"

namespace relation = charm::capability;

namespace {
    enum class ContractKey : unsigned char {
        output,
    };

    enum class RequirementKey : unsigned char {
        report,
    };

    enum class ProvisionKey : unsigned char {
        console,
    };

    using Requirement = relation::Requirement<ContractKey, RequirementKey>;
    using Provision = relation::Provision<ContractKey, ProvisionKey>;
    using Binding = relation::Binding<RequirementKey, ProvisionKey>;

    constexpr Requirement requirement{RequirementKey::report, ContractKey::output};
    constexpr Provision provision{ProvisionKey::console, ContractKey::output};
    constexpr Binding binding{RequirementKey::report, ProvisionKey::console};

    static_assert(requirement.contract == provision.contract);
    static_assert(binding.requirement == requirement.key);
    static_assert(binding.provision == provision.key);
}
