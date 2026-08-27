#include "Backends/contract/backend_evidence.hpp"
#include "Backends/qemu/qemu_reference.hpp"
#include "Modules/core/capability/relations.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <span>
#include <string_view>

namespace relation = charm::capability;
namespace be = charm::backend::contract;
namespace qemu = charm::backend::qemu;

namespace cap {
    struct TextSink {
        static constexpr std::string_view name{"TextSink"};

        template <typename T>
        static constexpr bool satisfied_by = requires(T& sink, std::string_view text) {
            { sink.write(text) } noexcept -> std::same_as<qemu::Transfer>;
            { sink.flush() } noexcept -> std::same_as<qemu::Status>;
        };
    };
}

namespace {
    enum class ContractKey : std::uint8_t {
        text_sink,
    };
    enum class RequirementKey : std::uint8_t {
        early_console,
    };
    enum class ProvisionKey : std::uint8_t {
        semihost_console,
    };

    constexpr relation::Requirement<ContractKey, RequirementKey> requirement{
        RequirementKey::early_console, ContractKey::text_sink};
    constexpr relation::Provision<ContractKey, ProvisionKey> provision{
        ProvisionKey::semihost_console, ContractKey::text_sink};
    constexpr relation::Binding<RequirementKey, ProvisionKey> binding{
        RequirementKey::early_console, ProvisionKey::semihost_console};

    static_assert(cap::TextSink::satisfied_by<qemu::EarlyConsoleProvider>);
    static_assert(requirement.contract == provision.contract);
    static_assert(binding.requirement == requirement.key);
    static_assert(binding.provision == provision.key);

    bool expect(const bool condition, const char* message) {
        if (!condition) {
            std::fprintf(stderr, "[ERR] %s\n", message);
            return false;
        }
        return true;
    }

    bool run_smoke() {
        qemu::ReferenceBackend backend{};
        auto write = backend.early_console.write("qemu");
        auto flush = backend.early_console.flush();

        const auto view = backend.evidence_view();
        const auto summary = be::summarize_backend_evidence(view);

        bool ok = true;
        ok &= expect(write.is_ok() && write.bytes == 4U, "early console should accept text");
        ok &= expect(flush.is_ok(), "early console flush should succeed");
        ok &= expect(backend.early_console.bytes_accepted == 4U, "early console should record accepted bytes");
        ok &= expect(backend.memory_map[1].base == 0x20080000ULL, "QEMU runtime region should match resident ELF smoke");
        ok &= expect(backend.memory_map[1].size == 0x00010000ULL, "QEMU runtime region size should match resident ELF smoke");

        ok &= expect(view.identity.kind == be::BackendKind::qemu, "QEMU reference should export qemu identity");
        ok &= expect(view.identity.name == "mps2-an500", "QEMU reference should name machine model");
        ok &= expect(summary.capability_export_count == 1U, "QEMU reference should export early console capability");
        ok &= expect(summary.selected_binding_count == 1U, "QEMU reference should export early console binding");
        ok &= expect(summary.required_fact_count == 5U, "QEMU reference should track five required facts");
        ok &= expect(summary.provided_fact_count == 5U, "QEMU reference should provide five required facts");
        ok &= expect(summary.missing_required_fact_count == 0U, "QEMU reference should not miss required facts");
        ok &= expect(summary.unknown_required_fact_count == 0U, "QEMU reference should not have unknown required facts");
        ok &= expect(summary.required_facts_are_ready, "QEMU required facts should be ready");
        ok &= expect(view.facts[5].requirement == be::FactRequirement::optional,
                     "H747 external peripherals should remain optional in QEMU reference");
        ok &= expect(view.facts[5].state == be::FactState::missing,
                     "QEMU reference should explicitly not model H747 external peripherals");
        return ok;
    }
}

int main() {
    if (!run_smoke()) {
        return 1;
    }
    std::puts("[backends-qemu-reference-smoke] ok");
    return 0;
}
