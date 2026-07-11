#pragma once

#include "Backends/contract/backend_evidence.hpp"

#include <array>
#include <cstdint>
#include <string_view>

namespace charm::backend::board {
    struct EarlyUartConsoleProviderEvidence {
        static constexpr std::string_view provider_instance{"board.h747.usart1.console"};
        static constexpr std::string_view provider_type{"board early UART console provider"};
        static constexpr std::string_view runtime_domain{"h747_cm7"};
        static constexpr std::string_view adapter{"stm32_hal_uart_tx_dma_adapter"};
        static constexpr std::string_view transport{"usart1"};

        std::uint32_t baud{115200};
        bool tx_dma_observed{true};
        bool rx_dma_observed{true};
        std::uint32_t dropped_bytes{0};
        std::uint32_t busy_count{0};
    };

    struct QspiBlockProviderEvidence {
        static constexpr std::string_view provider_instance{"board.h747.qspi.block"};
        static constexpr std::string_view provider_type{"board qspi block provider"};
        static constexpr std::string_view runtime_domain{"h747_cm7"};
        static constexpr std::string_view adapter{"stm32_hal_qspi_adapter"};
        static constexpr std::string_view transport{"qspi_nor"};

        bool jedec_observed{true};
        bool capacity_observed{true};
        bool erase_alignment_observed{true};
        bool write_alignment_observed{true};
    };

    struct EmmcRawSlotProviderEvidence {
        static constexpr std::string_view provider_instance{"board.h747.emmc.raw_slot_block"};
        static constexpr std::string_view provider_type{"board eMMC raw slot block provider"};
        static constexpr std::string_view runtime_domain{"h747_cm7"};
        static constexpr std::string_view adapter{"stm32_hal_sdmmc_adapter"};
        static constexpr std::string_view transport{"sdmmc_emmc"};

        std::uint32_t block_size{512};
        bool raw_slot_observed{true};
        bool fat_resource_path_observed{true};
    };

    struct ReferenceBackend {
        EarlyUartConsoleProviderEvidence console{};
        QspiBlockProviderEvidence qspi{};
        EmmcRawSlotProviderEvidence emmc{};

        std::array<contract::CapabilityExport, 4> capability_exports{
            contract::CapabilityExport{
                .capability_name = "TextSink",
                .requirement_role = "early_console",
                .provider_instance = EarlyUartConsoleProviderEvidence::provider_instance,
                .provider_type = EarlyUartConsoleProviderEvidence::provider_type,
                .runtime_domain = EarlyUartConsoleProviderEvidence::runtime_domain,
                .adapter = EarlyUartConsoleProviderEvidence::adapter,
            },
            contract::CapabilityExport{
                .capability_name = "LineSource",
                .requirement_role = "shell",
                .provider_instance = EarlyUartConsoleProviderEvidence::provider_instance,
                .provider_type = EarlyUartConsoleProviderEvidence::provider_type,
                .runtime_domain = EarlyUartConsoleProviderEvidence::runtime_domain,
                .adapter = EarlyUartConsoleProviderEvidence::adapter,
            },
            contract::CapabilityExport{
                .capability_name = "BlockDevice",
                .requirement_role = "app_store",
                .provider_instance = QspiBlockProviderEvidence::provider_instance,
                .provider_type = QspiBlockProviderEvidence::provider_type,
                .runtime_domain = QspiBlockProviderEvidence::runtime_domain,
                .adapter = QspiBlockProviderEvidence::adapter,
            },
            contract::CapabilityExport{
                .capability_name = "BlockDevice",
                .requirement_role = "resource_media",
                .provider_instance = EmmcRawSlotProviderEvidence::provider_instance,
                .provider_type = EmmcRawSlotProviderEvidence::provider_type,
                .runtime_domain = EmmcRawSlotProviderEvidence::runtime_domain,
                .adapter = EmmcRawSlotProviderEvidence::adapter,
            },
        };

        std::array<contract::BindingEvidence, 4> selected_bindings{
            contract::BindingEvidence{
                .capability_name = "TextSink",
                .requirement_role = "early_console",
                .provider_instance = EarlyUartConsoleProviderEvidence::provider_instance,
                .selection = "explicit_binding",
            },
            contract::BindingEvidence{
                .capability_name = "LineSource",
                .requirement_role = "shell",
                .provider_instance = EarlyUartConsoleProviderEvidence::provider_instance,
                .selection = "explicit_binding",
            },
            contract::BindingEvidence{
                .capability_name = "BlockDevice",
                .requirement_role = "app_store",
                .provider_instance = QspiBlockProviderEvidence::provider_instance,
                .selection = "explicit_binding",
            },
            contract::BindingEvidence{
                .capability_name = "BlockDevice",
                .requirement_role = "resource_media",
                .provider_instance = EmmcRawSlotProviderEvidence::provider_instance,
                .selection = "explicit_binding",
            },
        };

        std::array<contract::BackendFact, 10> facts{
            contract::BackendFact{
                .kind = "board",
                .name = "h747_player_board",
                .requirement = contract::FactRequirement::required,
                .state = contract::FactState::provided,
                .source = "H747 board README and h747-lab evidence",
            },
            contract::BackendFact{
                .kind = "runtime_domain",
                .name = "h747_cm7_system_owner",
                .requirement = contract::FactRequirement::required,
                .state = contract::FactState::provided,
                .source = "Draft/h747_board_execution_model_v0.md",
            },
            contract::BackendFact{
                .kind = "console",
                .name = "usart1_console_tx_rx_dma",
                .requirement = contract::FactRequirement::required,
                .state = contract::FactState::provided,
                .source = "h747-lab console evidence",
            },
            contract::BackendFact{
                .kind = "memory",
                .name = "external_sdram_regions",
                .requirement = contract::FactRequirement::required,
                .state = contract::FactState::provided,
                .source = "h747_lab_memory_evidence.md",
            },
            contract::BackendFact{
                .kind = "storage",
                .name = "qspi_nor_probe",
                .requirement = contract::FactRequirement::required,
                .state = contract::FactState::provided,
                .source = "h747-lab storage evidence",
            },
            contract::BackendFact{
                .kind = "storage",
                .name = "emmc_raw_slot_and_fat_resource_path",
                .requirement = contract::FactRequirement::required,
                .state = contract::FactState::provided,
                .source = "h747-lab dev_loader/player evidence",
            },
            contract::BackendFact{
                .kind = "clock",
                .name = "production_clock_tree_readback",
                .requirement = contract::FactRequirement::required,
                .state = contract::FactState::unknown,
                .source = "needs scoped board evidence capture",
            },
            contract::BackendFact{
                .kind = "irq",
                .name = "interrupt_line_inventory",
                .requirement = contract::FactRequirement::required,
                .state = contract::FactState::missing,
                .source = "not represented in Backends v0 reference yet",
            },
            contract::BackendFact{
                .kind = "display",
                .name = "ltdc_dsi_dma2d_present",
                .requirement = contract::FactRequirement::optional,
                .state = contract::FactState::provided,
                .source = "h747_lab_raster_evidence.md",
            },
            contract::BackendFact{
                .kind = "touch",
                .name = "gt9xx_i2c_event_path",
                .requirement = contract::FactRequirement::optional,
                .state = contract::FactState::provided,
                .source = "h747-lab player/touch evidence",
            },
        };

        [[nodiscard]] contract::BackendEvidenceView evidence_view() const noexcept {
            return contract::BackendEvidenceView{
                .identity = contract::BackendIdentity{
                    .kind = contract::BackendKind::board,
                    .name = "h747_reference",
                    .architecture = "cortex-m7",
                    .status = contract::BackendStatus::prototype,
                },
                .capability_exports = capability_exports,
                .selected_bindings = selected_bindings,
                .facts = facts,
            };
        }
    };
}
