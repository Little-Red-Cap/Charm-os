#include "core/capability/relations.hpp"

enum class SharedKey : unsigned char {
    value,
};

using InvalidBinding = charm::capability::Binding<SharedKey, SharedKey>;

InvalidBinding invalid_binding{
    SharedKey::value,
    SharedKey::value,
};
