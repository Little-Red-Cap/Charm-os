#include "Backends/contract/backend_evidence.hpp"
#include "Backends/contract/console_output.hpp"
#include "Backends/host/host_reference.hpp"
#include "Modules/core/capability/relations.hpp"

#include <array>
#include <cstddef>
#include <cstdio>
#include <optional>
#include <span>
#include <string_view>

namespace relation = charm::capability;
namespace be = charm::backend::contract;
namespace console = charm::backend::contract::console;
namespace host = charm::backend::host;

namespace cap {
    struct TextSink {
        static constexpr std::string_view name{"TextSink"};

        template <typename T>
        static constexpr bool satisfied_by = requires(T& sink, std::string_view text) {
            { sink.write(text) } noexcept -> std::same_as<host::Transfer>;
            { sink.flush() } noexcept -> std::same_as<host::Status>;
        };
    };

    struct LineSource {
        static constexpr std::string_view name{"LineSource"};

        template <typename T>
        static constexpr bool satisfied_by = requires(T& source) {
            { source.poll_line() } noexcept -> std::same_as<std::optional<std::string_view>>;
        };
    };

    struct BlockDevice {
        static constexpr std::string_view name{"BlockDevice"};

        template <typename T>
        static constexpr bool satisfied_by = requires(T& dev,
                                                      std::uint64_t lba,
                                                      std::span<std::byte> out,
                                                      std::span<const std::byte> in) {
            { dev.read(lba, out) } noexcept -> std::same_as<host::Status>;
            { dev.write(lba, in) } noexcept -> std::same_as<host::Status>;
            { dev.flush() } noexcept -> std::same_as<host::Status>;
        };
    };
}

namespace {
    enum class ContractKey : std::uint8_t {
        text_sink,
        line_source,
        block_device,
    };
    enum class RequirementKey : std::uint8_t {
        log,
        shell,
        app_store,
    };
    enum class ProvisionKey : std::uint8_t {
        console_text,
        console_line,
        memory_block,
    };

    constexpr std::array requirements{
        relation::Requirement<ContractKey, RequirementKey>{
            RequirementKey::log, ContractKey::text_sink},
        relation::Requirement<ContractKey, RequirementKey>{
            RequirementKey::shell, ContractKey::line_source},
        relation::Requirement<ContractKey, RequirementKey>{
            RequirementKey::app_store, ContractKey::block_device},
    };
    constexpr std::array provisions{
        relation::Provision<ContractKey, ProvisionKey>{
            ProvisionKey::console_text, ContractKey::text_sink},
        relation::Provision<ContractKey, ProvisionKey>{
            ProvisionKey::console_line, ContractKey::line_source},
        relation::Provision<ContractKey, ProvisionKey>{
            ProvisionKey::memory_block, ContractKey::block_device},
    };
    constexpr std::array bindings{
        relation::Binding<RequirementKey, ProvisionKey>{
            RequirementKey::log, ProvisionKey::console_text},
        relation::Binding<RequirementKey, ProvisionKey>{
            RequirementKey::shell, ProvisionKey::console_line},
        relation::Binding<RequirementKey, ProvisionKey>{
            RequirementKey::app_store, ProvisionKey::memory_block},
    };

    static_assert(cap::TextSink::satisfied_by<host::BufferedConsoleProvider>);
    static_assert(cap::LineSource::satisfied_by<host::BufferedConsoleProvider>);
    static_assert(cap::BlockDevice::satisfied_by<host::MemoryBlockProvider>);
    static_assert(requirements.size() == bindings.size());
    static_assert(provisions.size() == bindings.size());

    bool diagnostics_app(host::BufferedConsoleProvider& console_provider,
                         host::MemoryBlockProvider& app_store) {
        auto& log = console_provider;
        auto& shell = console_provider;

        log.scripted_lines[0] = "status";
        log.scripted_line_count = 1;

        const auto write = log.write("host");
        const auto line = shell.poll_line();
        const auto flushed = log.flush();

        std::array<std::byte, host::MemoryBlockProvider::block_size_value> block_in{};
        std::array<std::byte, host::MemoryBlockProvider::block_size_value> block_out{};
        block_in[0] = std::byte{0x42};

        const auto stored = app_store.write(0, block_in);
        const auto loaded = app_store.read(0, block_out);

        return write.is_ok() &&
               write.bytes == 4U &&
               line.has_value() &&
               *line == "status" &&
               flushed.is_ok() &&
               stored.is_ok() &&
               loaded.is_ok() &&
               block_out[0] == std::byte{0x42};
    }

    bool expect(const bool condition, const char* message) {
        if (!condition) {
            std::fprintf(stderr, "[ERR] %s\n", message);
            return false;
        }
        return true;
    }

    bool run_smoke() {
        host::ReferenceBackend backend{};

        bool ok = true;
        ok &= expect(diagnostics_app(backend.console, backend.app_store),
                     "app should consume explicitly materialized host provisions");
        ok &= expect(backend.console.bytes_accepted == 4U, "console provider should record accepted bytes");
        ok &= expect(backend.console.flush_count == 1U, "console provider should record flushes");
        ok &= expect(backend.console.lines_polled == 1U, "console provider should record line polling");
        const auto log_frame = backend.console.evidence_frame(console::TextSink::label, "log");
        const auto log_view = console::project_view(log_frame);
        ok &= expect(log_view.capability_name == "TextSink", "host console evidence should use contract console capability");
        ok &= expect(log_view.requirement_role == "log", "host console evidence should preserve role");
        ok &= expect(log_view.provider_instance == "host.buffered_console",
                     "host console evidence should preserve provider instance");
        ok &= expect(log_view.transport == "memory_buffer", "host console evidence should preserve transport");
        ok &= expect(log_view.bytes_accepted == 4U, "host console evidence should report accepted bytes");
        ok &= expect(backend.app_store.write_count == 1U, "block provider should record writes");
        ok &= expect(backend.app_store.read_count == 1U, "block provider should record reads");

        const auto view = backend.evidence_view();
        const auto summary = be::summarize_backend_evidence(view);

        ok &= expect(view.identity.kind == be::BackendKind::host, "host reference should export host identity");
        ok &= expect(view.identity.name == "reference", "host reference should export stable backend name");
        ok &= expect(summary.capability_export_count == 3U, "host reference should export three capabilities");
        ok &= expect(summary.selected_binding_count == 3U, "host reference should export three selected bindings");
        ok &= expect(summary.required_fact_count == 2U, "host reference should mark two required facts");
        ok &= expect(summary.provided_fact_count == 3U, "host reference should report provided facts");
        ok &= expect(summary.missing_required_fact_count == 0U, "host reference should have no missing required facts");
        ok &= expect(summary.unknown_required_fact_count == 0U, "host reference should have no unknown required facts");
        ok &= expect(summary.required_facts_are_ready, "host reference required facts should be ready");
        ok &= expect(view.capability_exports[0].provider_instance == "host.buffered_console",
                     "console provider instance should be evidence metadata");
        ok &= expect(view.capability_exports[2].provider_instance == "host.memory_block_app_store",
                     "block provider instance should be evidence metadata");
        return ok;
    }
}

int main() {
    if (!run_smoke()) {
        return 1;
    }
    std::puts("[backends-host-reference-smoke] ok");
    return 0;
}
