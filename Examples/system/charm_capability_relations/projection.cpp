#include "core/capability/relations.hpp"

#include <array>
#include <cstddef>
#include <cstdio>
#include <string_view>

namespace relation = charm::capability;

namespace {
    enum class ContractKey : unsigned char {
        power,
        display,
    };

    enum class RequirementKey : unsigned char {
        display_power,
        app_display,
    };

    enum class ProvisionKey : unsigned char {
        power,
        display,
    };

    using Requirement = relation::Requirement<ContractKey, RequirementKey>;
    using Provision = relation::Provision<ContractKey, ProvisionKey>;
    using Binding = relation::Binding<RequirementKey, ProvisionKey>;

    constexpr std::array requirements{
        Requirement{RequirementKey::display_power, ContractKey::power},
        Requirement{RequirementKey::app_display, ContractKey::display},
    };
    constexpr std::array provisions{
        Provision{ProvisionKey::power, ContractKey::power},
        Provision{ProvisionKey::display, ContractKey::display},
    };
    constexpr std::array bindings{
        Binding{RequirementKey::display_power, ProvisionKey::power},
        Binding{RequirementKey::app_display, ProvisionKey::display},
    };

    constexpr bool relation_contracts_match() noexcept {
        for (const auto& binding : bindings) {
            bool matched = false;
            for (const auto& requirement : requirements) {
                if (requirement.key != binding.requirement) {
                    continue;
                }
                for (const auto& provision : provisions) {
                    if (provision.key == binding.provision) {
                        matched = true;
                        if (requirement.contract != provision.contract) {
                            return false;
                        }
                        break;
                    }
                }
                break;
            }
            if (!matched) {
                return false;
            }
        }
        return true;
    }

    constexpr std::string_view requirement_label(const RequirementKey key) noexcept {
        switch (key) {
        case RequirementKey::display_power:
            return "display.power";
        case RequirementKey::app_display:
            return "app.display";
        }
        return "unknown";
    }

    constexpr std::string_view provision_label(const ProvisionKey key) noexcept {
        switch (key) {
        case ProvisionKey::power:
            return "power";
        case ProvisionKey::display:
            return "display";
        }
        return "unknown";
    }

    struct DisplayProvider {
        std::size_t presents{0};

        void present() noexcept {
            ++presents;
        }
    };

    struct AppContext {
        DisplayProvider* display{nullptr};

        [[nodiscard]] bool valid() const noexcept {
            return display != nullptr;
        }
    };

    [[nodiscard]] AppContext materialize_context(DisplayProvider& display) noexcept {
        for (const auto& item : bindings) {
            if (item.requirement == RequirementKey::app_display &&
                item.provision == ProvisionKey::display) {
                return {&display};
            }
        }
        return {};
    }

    struct EvidenceRow {
        std::string_view requirement{};
        std::string_view provision{};
        std::string_view provider_instance{};
    };

    [[nodiscard]] constexpr auto project_evidence() noexcept {
        std::array<EvidenceRow, bindings.size()> rows{};
        for (std::size_t index = 0; index < bindings.size(); ++index) {
            rows[index] = {
                requirement_label(bindings[index].requirement),
                provision_label(bindings[index].provision),
                bindings[index].provision == ProvisionKey::power
                    ? "host.power_provider"
                    : "host.display_provider",
            };
        }
        return rows;
    }

    bool expect(const bool condition, const char* message) noexcept {
        if (!condition) {
            std::fprintf(stderr, "[ERR] %s\n", message);
            return false;
        }
        return true;
    }

    bool run_projection() noexcept {
        static_assert(relation_contracts_match());

        DisplayProvider provider{};
        const auto context = materialize_context(provider);
        if (!expect(context.valid(), "resolved binding should materialize explicit app context")) {
            return false;
        }
        context.display->present();
        if (!expect(provider.presents == 1U, "app context should expose only capability behavior")) {
            return false;
        }

        constexpr auto evidence = project_evidence();
        if (!expect(evidence[1].requirement == "app.display",
                    "evidence should explain requirement label")) {
            return false;
        }
        return expect(evidence[1].provider_instance == "host.display_provider",
                      "provider identity should remain outside app context");
    }
}

int main() {
    if (!run_projection()) {
        return 1;
    }
    std::puts("[charm-capability-relations-projection] ok");
    return 0;
}
