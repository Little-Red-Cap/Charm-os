#include "core/capability/relations.hpp"

enum class ContractKey : unsigned char {
    value,
};

enum class RequirementKey : unsigned char {
    value,
};

enum class ProvisionKey : unsigned char {
    value,
};

using InvalidRequirement = charm::capability::Requirement<ContractKey, RequirementKey>;
using InvalidProvision = charm::capability::Provision<ContractKey, ProvisionKey>;
using InvalidBinding = charm::capability::Binding<RequirementKey, ProvisionKey>;

InvalidRequirement invalid_requirement{};
InvalidProvision invalid_provision{};
InvalidBinding invalid_binding{};
