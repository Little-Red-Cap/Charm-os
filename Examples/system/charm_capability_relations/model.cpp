#include "core/capability/relations.hpp"

#include <array>
#include <cstdio>
#include <string_view>
#include <type_traits>

namespace relation = charm::capability;

namespace {
    enum class ContractKey : unsigned char {
        text_sink,
        clock,
    };

    enum class RequirementKey : unsigned char {
        report,
        trace,
        monotonic_time,
    };

    enum class ProvisionKey : unsigned char {
        shared_console,
        clock,
    };

    using Requirement = relation::Requirement<ContractKey, RequirementKey>;
    using Provision = relation::Provision<ContractKey, ProvisionKey>;
    using Binding = relation::Binding<RequirementKey, ProvisionKey>;
    using ResolvedBinding = relation::ResolvedBinding<RequirementKey, ProvisionKey>;

    constexpr std::array requirements{
        Requirement{RequirementKey::report, ContractKey::text_sink},
        Requirement{RequirementKey::trace, ContractKey::text_sink},
        Requirement{RequirementKey::monotonic_time, ContractKey::clock},
    };

    constexpr std::array provisions{
        Provision{ProvisionKey::shared_console, ContractKey::text_sink},
        Provision{ProvisionKey::clock, ContractKey::clock},
    };

    constexpr std::array bindings{
        Binding{RequirementKey::report, ProvisionKey::shared_console},
        Binding{RequirementKey::trace, ProvisionKey::shared_console},
        Binding{RequirementKey::monotonic_time, ProvisionKey::clock},
    };

    static_assert(std::is_trivially_copyable_v<Requirement>);
    static_assert(std::is_trivially_copyable_v<Provision>);
    static_assert(std::is_trivially_copyable_v<Binding>);
    static_assert(std::is_trivially_copyable_v<ResolvedBinding>);
    static_assert(bindings[0].provision == bindings[1].provision);
    static_assert(requirements[0].key != requirements[1].key);

    constexpr std::string_view requirement_label(const RequirementKey key) noexcept {
        switch (key) {
        case RequirementKey::report:
        case RequirementKey::trace:
            return "output";
        case RequirementKey::monotonic_time:
            return "monotonic_time";
        }
        return "unknown";
    }

    static_assert(requirement_label(RequirementKey::report) ==
                  requirement_label(RequirementKey::trace));
    static_assert(bindings[0].requirement != bindings[1].requirement);
}

int main() {
    if (requirements.size() != 3U || provisions.size() != 2U || bindings.size() != 3U) {
        return 1;
    }
    std::puts("[charm-capability-relations-model] ok");
    return 0;
}
