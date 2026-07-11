#include "Backends/board/board_reference.hpp"
#include "Backends/contract/backend_evidence.hpp"
#include "Backends/contract/capability_topology.hpp"

#include <cstddef>
#include <cstdio>
#include <span>
#include <string_view>
#include <tuple>

namespace topo = charm::backend::contract;
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

namespace role {
    struct early_console {};
    struct shell {};
    struct app_store {};
    struct resource_media {};
}

namespace provider_instance {
    struct h747_usart1_console {
        using charm_provider_instance_tag = void;
        static constexpr std::string_view name{board::EarlyUartConsoleProviderEvidence::provider_instance};
    };

    struct h747_qspi_block {
        using charm_provider_instance_tag = void;
        static constexpr std::string_view name{board::QspiBlockProviderEvidence::provider_instance};
    };

    struct h747_emmc_raw_slot_block {
        using charm_provider_instance_tag = void;
        static constexpr std::string_view name{board::EmmcRawSlotProviderEvidence::provider_instance};
    };
}

namespace provider_type {
    struct h747_uart_console_provider {};
    struct h747_qspi_block_provider {};
    struct h747_emmc_raw_slot_block_provider {};
}

namespace backend {
    struct board_h747 {};
}

namespace runtime_domain {
    struct h747_cm7 {};
}

namespace adapter {
    struct stm32_hal_uart_tx_dma_adapter {};
    struct stm32_hal_qspi_adapter {};
    struct stm32_hal_sdmmc_adapter {};
}

namespace transport {
    struct usart1 {};
}

namespace {
    using ConsoleReq = topo::Requirement<cap::TextSink, role::early_console>;
    using ShellReq = topo::Requirement<cap::LineSource, role::shell>;
    using AppStoreReq = topo::Requirement<cap::BlockDevice, role::app_store>;
    using ResourceMediaReq = topo::Requirement<cap::BlockDevice, role::resource_media>;

    using ConsoleProv = topo::Provided<cap::TextSink, role::early_console>;
    using ShellProv = topo::Provided<cap::LineSource, role::shell>;
    using AppStoreProv = topo::Provided<cap::BlockDevice, role::app_store>;
    using ResourceMediaProv = topo::Provided<cap::BlockDevice, role::resource_media>;

    using ConsoleDesc = topo::ProviderDesc<provider_instance::h747_usart1_console,
                                           topo::ProviderSet<ConsoleProv, ShellProv>>;
    using QspiDesc = topo::ProviderDesc<provider_instance::h747_qspi_block,
                                        topo::ProviderSet<AppStoreProv>>;
    using EmmcDesc = topo::ProviderDesc<provider_instance::h747_emmc_raw_slot_block,
                                        topo::ProviderSet<ResourceMediaProv>>;
    using Providers = std::tuple<ConsoleDesc, QspiDesc, EmmcDesc>;

    using ConsoleMeta = topo::ProviderMeta<provider_instance::h747_usart1_console,
                                           provider_type::h747_uart_console_provider,
                                           backend::board_h747,
                                           runtime_domain::h747_cm7,
                                           adapter::stm32_hal_uart_tx_dma_adapter>;
    using QspiMeta = topo::ProviderMeta<provider_instance::h747_qspi_block,
                                        provider_type::h747_qspi_block_provider,
                                        backend::board_h747,
                                        runtime_domain::h747_cm7,
                                        adapter::stm32_hal_qspi_adapter>;
    using EmmcMeta = topo::ProviderMeta<provider_instance::h747_emmc_raw_slot_block,
                                        provider_type::h747_emmc_raw_slot_block_provider,
                                        backend::board_h747,
                                        runtime_domain::h747_cm7,
                                        adapter::stm32_hal_sdmmc_adapter>;
    using Metas = std::tuple<ConsoleMeta, QspiMeta, EmmcMeta>;

    using ConsoleBinding = topo::ProfileBinding<ConsoleReq, provider_instance::h747_usart1_console>;
    using ShellBinding = topo::ProfileBinding<ShellReq, provider_instance::h747_usart1_console>;
    using AppStoreBinding = topo::ProfileBinding<AppStoreReq, provider_instance::h747_qspi_block>;
    using ResourceMediaBinding = topo::ProfileBinding<ResourceMediaReq, provider_instance::h747_emmc_raw_slot_block>;
    using BadStorageBinding = topo::ProfileBinding<AppStoreReq, provider_instance::h747_usart1_console>;
    using Requirements = topo::RequirementSet<ConsoleReq, ShellReq, AppStoreReq, ResourceMediaReq>;
    using Bindings = std::tuple<ConsoleBinding, ShellBinding, AppStoreBinding, ResourceMediaBinding>;

    static_assert(topo::binding_valid_v<ConsoleBinding, Providers>);
    static_assert(topo::binding_valid_v<ShellBinding, Providers>);
    static_assert(topo::binding_valid_v<AppStoreBinding, Providers>);
    static_assert(topo::binding_valid_v<ResourceMediaBinding, Providers>);
    static_assert(!topo::binding_valid_v<BadStorageBinding, Providers>);
    static_assert(topo::requirements_bound_once_v<Bindings, Requirements>);
    static_assert(topo::binding_has_meta_v<ConsoleBinding, Metas>);
    static_assert(topo::binding_has_meta_v<AppStoreBinding, Metas>);
    static_assert(topo::binding_has_meta_v<ResourceMediaBinding, Metas>);
    static_assert(!topo::CanMakeProfileBinding<ConsoleReq, backend::board_h747>);
    static_assert(!topo::CanMakeProfileBinding<ConsoleReq, adapter::stm32_hal_uart_tx_dma_adapter>);
    static_assert(!topo::CanMakeProfileBinding<ConsoleReq, transport::usart1>);

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
