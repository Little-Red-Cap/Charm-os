#pragma once

#include "Backends/contract/backend_evidence.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <string_view>

namespace charm::backend::qemu {
    enum class StatusCode : std::uint8_t {
        ok,
        busy,
    };

    struct Status {
        StatusCode code{StatusCode::ok};

        [[nodiscard]] constexpr bool is_ok() const noexcept {
            return code == StatusCode::ok;
        }
    };

    struct Transfer {
        Status status{};
        std::size_t bytes{0};

        [[nodiscard]] constexpr bool is_ok() const noexcept {
            return status.is_ok();
        }
    };

    struct EarlyConsoleProvider {
        static constexpr std::string_view provider_instance{"qemu.early_console"};
        static constexpr std::string_view provider_type{"qemu early console provider"};
        static constexpr std::string_view runtime_domain{"qemu_m7"};
        static constexpr std::string_view adapter{"qemu_semihost_or_uart_adapter"};
        static constexpr std::string_view transport{"qemu_console"};

        std::array<std::byte, 256> tx{};
        std::size_t tx_used{0};
        std::size_t bytes_accepted{0};
        std::size_t busy_count{0};

        [[nodiscard]] Transfer write(const std::span<const std::byte> bytes) noexcept {
            const auto remaining = tx.size() - tx_used;
            const auto accepted = bytes.size() < remaining ? bytes.size() : remaining;
            if (accepted != 0U) {
                std::memcpy(tx.data() + tx_used, bytes.data(), accepted);
                tx_used += accepted;
                bytes_accepted += accepted;
            }
            if (accepted < bytes.size()) {
                ++busy_count;
                return Transfer{Status{StatusCode::busy}, accepted};
            }
            return Transfer{Status{}, accepted};
        }

        [[nodiscard]] Transfer write(const std::string_view text) noexcept {
            const auto remaining = tx.size() - tx_used;
            const auto accepted = text.size() < remaining ? text.size() : remaining;
            for (std::size_t i = 0; i < accepted; ++i) {
                tx[tx_used + i] = static_cast<std::byte>(text[i]);
            }
            tx_used += accepted;
            bytes_accepted += accepted;
            if (accepted < text.size()) {
                ++busy_count;
                return Transfer{Status{StatusCode::busy}, accepted};
            }
            return Transfer{Status{}, accepted};
        }

        [[nodiscard]] Status flush() noexcept {
            return {};
        }
    };

    struct MemoryRegion {
        std::string_view name{};
        std::uint64_t base{0};
        std::uint64_t size{0};
    };

    struct ReferenceBackend {
        EarlyConsoleProvider early_console{};

        std::array<MemoryRegion, 2> memory_map{
            MemoryRegion{
                .name = "qemu_firmware_sram",
                .base = 0x20000000ULL,
                .size = 0x00080000ULL,
            },
            MemoryRegion{
                .name = "qemu_app_runtime_region",
                .base = 0x20080000ULL,
                .size = 0x00010000ULL,
            },
        };

        std::array<contract::CapabilityExport, 1> capability_exports{
            contract::CapabilityExport{
                .capability_name = "TextSink",
                .requirement_role = "early_console",
                .provider_instance = EarlyConsoleProvider::provider_instance,
                .provider_type = EarlyConsoleProvider::provider_type,
                .runtime_domain = EarlyConsoleProvider::runtime_domain,
                .adapter = EarlyConsoleProvider::adapter,
            },
        };

        std::array<contract::BindingEvidence, 1> selected_bindings{
            contract::BindingEvidence{
                .capability_name = "TextSink",
                .requirement_role = "early_console",
                .provider_instance = EarlyConsoleProvider::provider_instance,
                .selection = "explicit_binding",
            },
        };

        std::array<contract::BackendFact, 6> facts{
            contract::BackendFact{
                .kind = "console",
                .name = "early_console",
                .requirement = contract::FactRequirement::required,
                .state = contract::FactState::provided,
                .source = "Backends/qemu reference",
            },
            contract::BackendFact{
                .kind = "timer",
                .name = "timer_observation",
                .requirement = contract::FactRequirement::required,
                .state = contract::FactState::provided,
                .source = "Backends/qemu reference",
            },
            contract::BackendFact{
                .kind = "trap",
                .name = "exception_path",
                .requirement = contract::FactRequirement::required,
                .state = contract::FactState::provided,
                .source = "Backends/qemu reference",
            },
            contract::BackendFact{
                .kind = "irq",
                .name = "interrupt_controller",
                .requirement = contract::FactRequirement::required,
                .state = contract::FactState::provided,
                .source = "Backends/qemu reference",
            },
            contract::BackendFact{
                .kind = "memory",
                .name = "runtime_region",
                .requirement = contract::FactRequirement::required,
                .state = contract::FactState::provided,
                .source = "Backends/qemu reference",
            },
            contract::BackendFact{
                .kind = "peripheral",
                .name = "h747_external_devices",
                .requirement = contract::FactRequirement::optional,
                .state = contract::FactState::missing,
                .source = "QEMU reference does not model H747 DSI/LTDC/eMMC/GT9xx",
            },
        };

        [[nodiscard]] contract::BackendEvidenceView evidence_view() const noexcept {
            return contract::BackendEvidenceView{
                .identity = contract::BackendIdentity{
                    .kind = contract::BackendKind::qemu,
                    .name = "mps2-an500",
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
