#include "Modules/core/capability/relations.hpp"

#include <array>
#include <cstddef>
#include <cstdio>
#include <span>
#include <string_view>

import init.graph;
import init.node;
import util.core;
import util.error;

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
    using ResolvedBinding = relation::ResolvedBinding<RequirementKey, ProvisionKey>;

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
    constexpr std::array resolved{
        ResolvedBinding{RequirementKey::display_power, ProvisionKey::power},
        ResolvedBinding{RequirementKey::app_display, ProvisionKey::display},
    };

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

    struct Trace {
        std::array<std::string_view, 3> entries{};
        std::size_t count{0};

        void append(const std::string_view value) noexcept {
            entries[count++] = value;
        }
    };

    util::Result<void> start_power(void* context) noexcept {
        static_cast<Trace*>(context)->append("power");
        return {};
    }

    util::Result<void> start_display(void* context) noexcept {
        static_cast<Trace*>(context)->append("display");
        return {};
    }

    util::Result<void> start_app(void* context) noexcept {
        static_cast<Trace*>(context)->append("app");
        return {};
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
        for (const auto& item : resolved) {
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
        std::array<EvidenceRow, resolved.size()> rows{};
        for (std::size_t index = 0; index < resolved.size(); ++index) {
            rows[index] = {
                requirement_label(resolved[index].requirement),
                provision_label(resolved[index].provision),
                resolved[index].provision == ProvisionKey::power
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
        static_assert(requirements[0].contract == provisions[0].contract);
        static_assert(requirements[1].contract == provisions[1].contract);
        static_assert(bindings[0].requirement == resolved[0].requirement);

        Trace trace{};
        constexpr std::array no_caps = std::array<init::CapId, 0>{};
        const std::array power_caps{init::cap_id(provision_label(ProvisionKey::power))};
        const std::array display_caps{init::cap_id(provision_label(ProvisionKey::display))};
        const std::array display_requires{init::cap_id(provision_label(resolved[0].provision))};
        const std::array app_requires{init::cap_id(provision_label(resolved[1].provision))};

        const init::Node power{
            "power", init::Phase::service, static_cast<util::u32>(init::Runlevel::all),
            power_caps, no_caps, start_power, nullptr, &trace};
        const init::Node display{
            "display", init::Phase::service, static_cast<util::u32>(init::Runlevel::all),
            display_caps, display_requires, start_display, nullptr, &trace};
        const init::Node app{
            "app", init::Phase::app, static_cast<util::u32>(init::Runlevel::all),
            no_caps, app_requires, start_app, nullptr, &trace};
        const std::array<const init::Node*, 3> nodes{&app, &display, &power};

        init::Graph<4, 4> graph{};
        const auto built = graph.build(nodes);
        if (!expect(built.has_value(), "resolved bindings should project to init graph")) {
            return false;
        }
        const auto started = graph.start();
        if (!expect(started.has_value(), "projected init graph should start")) {
            return false;
        }
        if (!expect(trace.entries == std::array<std::string_view, 3>{"power", "display", "app"},
                    "init projection should preserve dependency order")) {
            return false;
        }

        DisplayProvider provider{};
        auto context = materialize_context(provider);
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
