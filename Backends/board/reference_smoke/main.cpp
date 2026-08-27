#include "Backends/board/board_reference.hpp"
#include "Backends/contract/backend_evidence.hpp"
#include "Modules/core/capability/relations.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <span>
#include <string_view>

namespace relation = charm::capability;
namespace be = charm::backend::contract;
namespace board = charm::backend::board;

namespace cap {
    struct TextSink {
        static constexpr std::string_view name{"TextSink"};
    };

    struct LineSource {
        static constexpr std::string_view name{"LineSource"};
    };

    struct BlockDevice {
        static constexpr std::string_view name{"BlockDevice"};
    };
}

namespace {
    enum class ContractKey : std::uint8_t {
        text_sink,
        line_source,
        block_device,
    };
    enum class RequirementKey : std::uint8_t {
        early_console,
        shell,
        app_store,
        resource_media,
    };
    enum class ProvisionKey : std::uint8_t {
        uart_text,
        uart_line,
        qspi_block,
        emmc_block,
    };

    constexpr std::array requirements{
        relation::Requirement<ContractKey, RequirementKey>{
            RequirementKey::early_console, ContractKey::text_sink},
        relation::Requirement<ContractKey, RequirementKey>{
            RequirementKey::shell, ContractKey::line_source},
        relation::Requirement<ContractKey, RequirementKey>{
            RequirementKey::app_store, ContractKey::block_device},
        relation::Requirement<ContractKey, RequirementKey>{
            RequirementKey::resource_media, ContractKey::block_device},
    };
    constexpr std::array provisions{
        relation::Provision<ContractKey, ProvisionKey>{
            ProvisionKey::uart_text, ContractKey::text_sink},
        relation::Provision<ContractKey, ProvisionKey>{
            ProvisionKey::uart_line, ContractKey::line_source},
        relation::Provision<ContractKey, ProvisionKey>{
            ProvisionKey::qspi_block, ContractKey::block_device},
        relation::Provision<ContractKey, ProvisionKey>{
            ProvisionKey::emmc_block, ContractKey::block_device},
    };
    constexpr std::array bindings{
        relation::Binding<RequirementKey, ProvisionKey>{
            RequirementKey::early_console, ProvisionKey::uart_text},
        relation::Binding<RequirementKey, ProvisionKey>{
            RequirementKey::shell, ProvisionKey::uart_line},
        relation::Binding<RequirementKey, ProvisionKey>{
            RequirementKey::app_store, ProvisionKey::qspi_block},
        relation::Binding<RequirementKey, ProvisionKey>{
            RequirementKey::resource_media, ProvisionKey::emmc_block},
    };

    static_assert(requirements.size() == provisions.size());
    static_assert(requirements.size() == bindings.size());

    bool expect(const bool condition, const char* message) {
        if (!condition) {
            std::fprintf(stderr, "[ERR] %s\n", message);
            return false;
        }
        return true;
    }

    bool run_smoke() {
        board::ReferenceBackend backend{};
        const auto view = backend.evidence_view();
        const auto summary = be::summarize_backend_evidence(view);

        bool ok = true;
        ok &= expect(view.identity.kind == be::BackendKind::board, "board reference should export board identity");
        ok &= expect(view.identity.name == "h747_reference", "board reference should name H747 reference backend");
        ok &= expect(view.identity.architecture == "cortex-m7", "board reference should expose CM7 architecture");
        ok &= expect(summary.capability_export_count == 4U, "board reference should export four capability rows");
        ok &= expect(summary.selected_binding_count == 4U, "board reference should export four selected bindings");
        ok &= expect(summary.required_fact_count == 8U, "board reference should track eight required facts");
        ok &= expect(summary.provided_fact_count == 8U, "board reference should expose provided facts");
        ok &= expect(summary.missing_required_fact_count == 1U, "board reference should keep missing IRQ fact visible");
        ok &= expect(summary.unknown_required_fact_count == 1U, "board reference should keep unknown clock fact visible");
        ok &= expect(!summary.required_facts_are_ready, "board reference should not claim board readiness");
        ok &= expect(view.capability_exports[0].provider_instance == "board.h747.usart1.console",
                     "console provider identity should stay in evidence");
        ok &= expect(view.capability_exports[2].provider_instance == "board.h747.qspi.block",
                     "QSPI provider identity should stay in evidence");
        ok &= expect(view.capability_exports[3].provider_instance == "board.h747.emmc.raw_slot_block",
                     "eMMC provider identity should stay in evidence");
        ok &= expect(view.selected_bindings[0].selection == "explicit_binding",
                     "board selected bindings should remain explicit");
        ok &= expect(backend.console.baud == 115200U, "board reference should carry USART baud evidence");
        ok &= expect(backend.console.tx_dma_observed, "board reference should carry TX DMA evidence");
        ok &= expect(backend.console.rx_dma_observed, "board reference should carry RX DMA evidence");
        ok &= expect(backend.qspi.jedec_observed, "board reference should carry QSPI JEDEC evidence");
        ok &= expect(backend.emmc.block_size == 512U, "board reference should carry eMMC block size evidence");
        return ok;
    }
}

int main() {
    if (!run_smoke()) {
        return 1;
    }
    std::puts("[backends-board-reference-smoke] ok");
    return 0;
}
