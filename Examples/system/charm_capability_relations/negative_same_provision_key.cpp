#include "core/capability/relations.hpp"

enum class SharedKey : unsigned char {
    value,
};

using InvalidProvision = charm::capability::Provision<SharedKey, SharedKey>;

InvalidProvision invalid_provision{};
