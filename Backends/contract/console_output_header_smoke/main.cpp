#include "Backends/contract/console_output.hpp"
#include "Modules/core/capability/relations.hpp"

#include <array>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <optional>
#include <span>
#include <string_view>

namespace relation = charm::capability;
namespace console = charm::backend::contract::console;

namespace provider_instance {
    struct buffered_console {
        using charm_provider_instance_tag = void;
        static constexpr std::string_view name{"host.buffered_console"};
    };
}

namespace {
    struct GoodConsole {
        std::array<std::byte, 16> tx{};
        std::size_t used{0};
        std::size_t lines_polled{0};

        [[nodiscard]] console::Transfer write(const std::span<const std::byte> bytes) noexcept {
            const auto accepted = bytes.size() < tx.size() - used ? bytes.size() : tx.size() - used;
            if (accepted != 0U) {
                std::memcpy(tx.data() + used, bytes.data(), accepted);
                used += accepted;
            }
            return console::Transfer{
                .status = console::Status{accepted == bytes.size() ? console::StatusCode::ok
                                                                    : console::StatusCode::busy},
                .bytes = accepted,
            };
        }

        [[nodiscard]] console::Transfer write(const std::string_view text) noexcept {
            const auto accepted = text.size() < tx.size() - used ? text.size() : tx.size() - used;
            for (std::size_t i = 0; i < accepted; ++i) {
                tx[used + i] = static_cast<std::byte>(text[i]);
            }
            used += accepted;
            return console::Transfer{
                .status = console::Status{accepted == text.size() ? console::StatusCode::ok
                                                                   : console::StatusCode::busy},
                .bytes = accepted,
            };
        }

        [[nodiscard]] console::Status flush() noexcept {
            return {};
        }

        [[nodiscard]] std::optional<std::string_view> poll_line() noexcept {
            ++lines_polled;
            return "status";
        }
    };

    struct MissingFlush {
        [[nodiscard]] console::Transfer write(std::string_view) noexcept {
            return {};
        }
    };

    enum class ContractKey : std::uint8_t {
        text_sink,
        line_source,
    };
    enum class RequirementKey : std::uint8_t {
        log,
        shell,
    };
    enum class ProvisionKey : std::uint8_t {
        console_text,
        console_line,
    };

    constexpr std::array requirements{
        relation::Requirement<ContractKey, RequirementKey>{
            RequirementKey::log, ContractKey::text_sink},
        relation::Requirement<ContractKey, RequirementKey>{
            RequirementKey::shell, ContractKey::line_source},
    };
    constexpr std::array provisions{
        relation::Provision<ContractKey, ProvisionKey>{
            ProvisionKey::console_text, ContractKey::text_sink},
        relation::Provision<ContractKey, ProvisionKey>{
            ProvisionKey::console_line, ContractKey::line_source},
    };
    constexpr std::array bindings{
        relation::Binding<RequirementKey, ProvisionKey>{
            RequirementKey::log, ProvisionKey::console_text},
        relation::Binding<RequirementKey, ProvisionKey>{
            RequirementKey::shell, ProvisionKey::console_line},
    };

    static_assert(console::ByteSink::satisfied_by<GoodConsole>);
    static_assert(console::TextSink::satisfied_by<GoodConsole>);
    static_assert(console::LineSource::satisfied_by<GoodConsole>);
    static_assert(!console::ByteSink::satisfied_by<MissingFlush>);
    static_assert(!console::TextSink::satisfied_by<MissingFlush>);
    static_assert(requirements.size() == bindings.size());
    static_assert(provisions[0].contract == requirements[0].contract);
    static_assert(provisions[1].contract == requirements[1].contract);

    bool expect(const bool condition, const char* message) {
        if (!condition) {
            std::fprintf(stderr, "[ERR] %s\n", message);
            return false;
        }
        return true;
    }

    bool run_smoke() {
        GoodConsole provider{};
        const auto first = provider.write("hello");
        const auto overflow = provider.write("abcdefghijklmnopqrstuvwxyz");
        const auto line = provider.poll_line();

        console::EvidenceCollector<1> collector{};
        const console::EvidenceFrame frame{
            .capability_name = console::TextSink::label,
            .requirement_role = "log",
            .provider_instance = provider_instance::buffered_console::name,
            .provider_type = "host buffered console provider",
            .backend = "host.reference",
            .runtime_domain = "host_process",
            .adapter = "host_memory_console_adapter",
            .transport = "memory_buffer",
            .tx_mode = "buffered",
            .rx_mode = "scripted_line",
            .status = console::status_from_degradation(0U, 15U, 1U),
            .bytes_accepted = first.bytes + overflow.bytes,
            .dropped_bytes = 15U,
            .busy_count = 1U,
            .lines_polled = provider.lines_polled,
        };

        bool ok = true;
        ok &= expect(first.is_ok() && first.bytes == 5U, "console text transfer should report accepted bytes");
        ok &= expect(!overflow.is_ok() && overflow.bytes == 11U, "console overflow should report busy transfer");
        ok &= expect(line.has_value() && *line == "status", "line source should return scripted line");
        ok &= expect(collector.append(frame), "collector should append first frame");
        ok &= expect(!collector.append(frame), "collector capacity should be explicit");

        const auto view = console::project_view(collector.frames[0]);
        ok &= expect(view.capability_name == "TextSink", "projected view should preserve capability");
        ok &= expect(view.requirement_role == "log", "projected view should preserve role");
        ok &= expect(view.provider_instance == "host.buffered_console", "projected view should preserve provider instance");
        ok &= expect(view.runtime_domain == "host_process", "projected view should preserve runtime domain");
        ok &= expect(view.transport == "memory_buffer", "projected view should preserve transport");
        ok &= expect(view.status == console::EvidenceStatus::degraded, "projected view should preserve degraded status");
        ok &= expect(view.bytes_accepted == 16U, "projected view should preserve accepted bytes");
        ok &= expect(view.dropped_bytes == 15U, "projected view should preserve dropped bytes");
        ok &= expect(view.busy_count == 1U, "projected view should preserve busy count");
        return ok;
    }
}

int main() {
    if (!run_smoke()) {
        return 1;
    }
    std::puts("[backends-contract-console-output-header-smoke] ok");
    return 0;
}
