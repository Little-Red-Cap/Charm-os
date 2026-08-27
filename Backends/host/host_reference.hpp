#pragma once

#include "Backends/contract/backend_evidence.hpp"
#include "Backends/contract/console_output.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>
#include <span>
#include <string_view>

namespace charm::backend::host {
    namespace console = contract::console;

    enum class StatusCode : std::uint8_t {
        ok,
        busy,
        no_entry,
        invalid_argument,
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

    struct BufferedConsoleProvider {
        static constexpr std::string_view provider_instance{"host.buffered_console"};
        static constexpr std::string_view provider_type{"host buffered console provider"};
        static constexpr std::string_view runtime_domain{"host_process"};
        static constexpr std::string_view adapter{"host_memory_console_adapter"};
        static constexpr std::string_view transport{"memory_buffer"};

        std::array<std::byte, 128> tx{};
        std::size_t tx_used{0};
        std::size_t bytes_accepted{0};
        std::size_t dropped_bytes{0};
        std::size_t busy_count{0};
        std::size_t flush_count{0};
        std::array<std::string_view, 4> scripted_lines{};
        std::size_t scripted_line_count{0};
        std::size_t line_index{0};
        std::size_t lines_polled{0};

        [[nodiscard]] Transfer write(const std::span<const std::byte> bytes) noexcept {
            const auto remaining = tx.size() - tx_used;
            const auto accepted = bytes.size() < remaining ? bytes.size() : remaining;
            if (accepted != 0U) {
                std::memcpy(tx.data() + tx_used, bytes.data(), accepted);
                tx_used += accepted;
                bytes_accepted += accepted;
            }
            if (accepted < bytes.size()) {
                dropped_bytes += bytes.size() - accepted;
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
                dropped_bytes += text.size() - accepted;
                ++busy_count;
                return Transfer{Status{StatusCode::busy}, accepted};
            }
            return Transfer{Status{}, accepted};
        }

        [[nodiscard]] Status flush() noexcept {
            ++flush_count;
            return {};
        }

        [[nodiscard]] std::optional<std::string_view> poll_line() noexcept {
            ++lines_polled;
            if (line_index >= scripted_line_count) {
                return std::nullopt;
            }
            const auto line = scripted_lines[line_index++];
            if (line.empty()) {
                return std::nullopt;
            }
            return line;
        }

        [[nodiscard]] bool degraded() const noexcept {
            return dropped_bytes != 0U || busy_count != 0U;
        }

        [[nodiscard]] console::EvidenceFrame evidence_frame(const std::string_view capability_name,
                                                            const std::string_view requirement_role) const noexcept {
            return console::EvidenceFrame{
                .capability_name = capability_name,
                .requirement_role = requirement_role,
                .provider_instance = provider_instance,
                .provider_type = provider_type,
                .backend = "host.reference",
                .runtime_domain = runtime_domain,
                .adapter = adapter,
                .transport = transport,
                .tx_mode = "buffered",
                .rx_mode = "scripted_line",
                .status = console::status_from_degradation(0U, dropped_bytes, busy_count),
                .bytes_accepted = bytes_accepted,
                .dropped_bytes = dropped_bytes,
                .busy_count = busy_count,
                .lines_polled = lines_polled,
            };
        }
    };

    struct MemoryBlockProvider {
        static constexpr std::string_view provider_instance{"host.memory_block_app_store"};
        static constexpr std::string_view provider_type{"host memory block provider"};
        static constexpr std::string_view runtime_domain{"host_process"};
        static constexpr std::string_view adapter{"host_memory_block_adapter"};
        static constexpr std::string_view media_kind{"host_memory"};
        static constexpr std::uint64_t block_size_value{16};
        static constexpr std::uint64_t block_count_value{4};

        std::array<std::byte, block_size_value * block_count_value> bytes{};
        std::size_t read_count{0};
        std::size_t write_count{0};
        StatusCode last_error{StatusCode::ok};

        [[nodiscard]] constexpr std::uint64_t block_size() const noexcept {
            return block_size_value;
        }

        [[nodiscard]] constexpr std::uint64_t block_count() const noexcept {
            return block_count_value;
        }

        [[nodiscard]] Status read(const std::uint64_t lba, const std::span<std::byte> out) noexcept {
            ++read_count;
            if (out.size() != block_size_value || lba >= block_count_value) {
                last_error = StatusCode::invalid_argument;
                return {last_error};
            }
            const auto offset = static_cast<std::size_t>(lba * block_size_value);
            std::memcpy(out.data(), bytes.data() + offset, out.size());
            last_error = StatusCode::ok;
            return {};
        }

        [[nodiscard]] Status write(const std::uint64_t lba, const std::span<const std::byte> in) noexcept {
            ++write_count;
            if (in.size() != block_size_value || lba >= block_count_value) {
                last_error = StatusCode::invalid_argument;
                return {last_error};
            }
            const auto offset = static_cast<std::size_t>(lba * block_size_value);
            std::memcpy(bytes.data() + offset, in.data(), in.size());
            last_error = StatusCode::ok;
            return {};
        }

        [[nodiscard]] Status flush() noexcept {
            last_error = StatusCode::ok;
            return {};
        }
    };

    struct ReferenceBackend {
        BufferedConsoleProvider console{};
        MemoryBlockProvider app_store{};

        std::array<contract::CapabilityExport, 3> capability_exports{
            contract::CapabilityExport{
                .capability_name = "TextSink",
                .requirement_role = "log",
                .provision_label = "console.text",
                .provider_instance = BufferedConsoleProvider::provider_instance,
                .provider_type = BufferedConsoleProvider::provider_type,
                .runtime_domain = BufferedConsoleProvider::runtime_domain,
                .adapter = BufferedConsoleProvider::adapter,
            },
            contract::CapabilityExport{
                .capability_name = "LineSource",
                .requirement_role = "shell",
                .provision_label = "console.line",
                .provider_instance = BufferedConsoleProvider::provider_instance,
                .provider_type = BufferedConsoleProvider::provider_type,
                .runtime_domain = BufferedConsoleProvider::runtime_domain,
                .adapter = BufferedConsoleProvider::adapter,
            },
            contract::CapabilityExport{
                .capability_name = "BlockDevice",
                .requirement_role = "app_store",
                .provision_label = "block.memory.app_store",
                .provider_instance = MemoryBlockProvider::provider_instance,
                .provider_type = MemoryBlockProvider::provider_type,
                .runtime_domain = MemoryBlockProvider::runtime_domain,
                .adapter = MemoryBlockProvider::adapter,
            },
        };

        std::array<contract::BindingEvidence, 3> selected_bindings{
            contract::BindingEvidence{
                .capability_name = "TextSink",
                .requirement_role = "log",
                .provision_label = "console.text",
                .provider_instance = BufferedConsoleProvider::provider_instance,
                .selection = "explicit_binding",
            },
            contract::BindingEvidence{
                .capability_name = "LineSource",
                .requirement_role = "shell",
                .provision_label = "console.line",
                .provider_instance = BufferedConsoleProvider::provider_instance,
                .selection = "explicit_binding",
            },
            contract::BindingEvidence{
                .capability_name = "BlockDevice",
                .requirement_role = "app_store",
                .provision_label = "block.memory.app_store",
                .provider_instance = MemoryBlockProvider::provider_instance,
                .selection = "explicit_binding",
            },
        };

        std::array<contract::BackendFact, 4> facts{
            contract::BackendFact{
                .kind = "console",
                .name = "buffered_console",
                .requirement = contract::FactRequirement::required,
                .state = contract::FactState::provided,
                .source = "Backends/host reference",
            },
            contract::BackendFact{
                .kind = "line_source",
                .name = "scripted_lines",
                .requirement = contract::FactRequirement::optional,
                .state = contract::FactState::provided,
                .source = "Backends/host reference",
            },
            contract::BackendFact{
                .kind = "block",
                .name = "memory_block_app_store",
                .requirement = contract::FactRequirement::required,
                .state = contract::FactState::provided,
                .source = "Backends/host reference",
            },
            contract::BackendFact{
                .kind = "clock",
                .name = "monotonic_clock",
                .requirement = contract::FactRequirement::optional,
                .state = contract::FactState::unknown,
                .source = "not implemented in host reference v0",
            },
        };

        [[nodiscard]] contract::BackendEvidenceView evidence_view() const noexcept {
            return contract::BackendEvidenceView{
                .identity = contract::BackendIdentity{
                    .kind = contract::BackendKind::host,
                    .name = "reference",
                    .architecture = "portable",
                    .status = contract::BackendStatus::prototype,
                },
                .capability_exports = capability_exports,
                .selected_bindings = selected_bindings,
                .facts = facts,
            };
        }
    };
}
