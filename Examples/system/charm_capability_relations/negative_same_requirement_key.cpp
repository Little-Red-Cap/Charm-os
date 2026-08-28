#include "core/capability/relations.hpp"

enum class SharedKey : unsigned char {
    value,
};

using InvalidRequirement = charm::capability::Requirement<SharedKey, SharedKey>;

InvalidRequirement invalid_requirement{};
