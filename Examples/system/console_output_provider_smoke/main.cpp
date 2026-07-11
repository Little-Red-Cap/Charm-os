#include "Backends/contract/capability_topology.hpp"
#include "Backends/contract/console_output.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <optional>
#include <span>
#include <string_view>
#include <tuple>

namespace cap {
    using Status = charm::backend::contract::console::Status;
    using StatusCode = charm::backend::contract::console::StatusCode;
    using Transfer = charm::backend::contract::console::Transfer;
    using ByteSink = charm::backend::contract::console::ByteSink;
    using TextSink = charm::backend::contract::console::TextSink;
    using LineSource = charm::backend::contract::console::LineSource;
}

namespace role {
    using log = charm::backend::contract::console::role::log;
    using debug_trace = charm::backend::contract::console::role::debug_trace;
    using shell = charm::backend::contract::console::role::shell;
}

namespace provider_instance {
    struct tag {
        using charm_provider_instance_tag = void;
    };

    struct host_buffered_console : tag {
        static constexpr std::string_view name{"host.buffered_console"};
    };

    struct host_trace_buffer : tag {
        static constexpr std::string_view name{"host.trace_buffer"};
    };
}

namespace provider_type {
    struct host_buffered_console_provider {
        static constexpr std::string_view name{"host buffered console provider"};
    };
}

namespace adapter {
    struct host_memory_console_adapter {
        static constexpr std::string_view name{"host_memory_console_adapter"};
    };
}

namespace backend {
    struct host {
        static constexpr std::string_view name{"host"};
    };
}

namespace runtime_domain {
    struct host_process {
        static constexpr std::string_view name{"host_process"};
    };
}

namespace transport {
    struct memory_buffer {
        static constexpr std::string_view name{"memory_buffer"};
    };
}

namespace hal {
    struct file_api {
        static constexpr std::string_view name{"file_api"};
    };
}

namespace console_evidence {
    using EvidenceStatus = charm::backend::contract::console::EvidenceStatus;
    using ConsoleEvidenceFrame = charm::backend::contract::console::EvidenceFrame;
    using ConsoleEvidenceView = charm::backend::contract::console::EvidenceView;
    using ConsoleEvidenceCollector = charm::backend::contract::console::EvidenceCollector<4>;
    using charm::backend::contract::console::project_view;
    using charm::backend::contract::console::status_from_degradation;

    struct H747ConsoleTxPresentation {
        std::size_t started{0};
        std::size_t done{0};
        std::size_t bytes{0};
        std::size_t fallback{0};
        std::size_t dropped{0};
        std::size_t busy{0};
        std::size_t ring_used{0};
        std::size_t ring_size{0};
    };

    [[nodiscard]] constexpr ConsoleEvidenceView project_h747_console_tx(
        const H747ConsoleTxPresentation& presentation) noexcept {
        (void)presentation.started;
        (void)presentation.done;
        (void)presentation.ring_used;
        (void)presentation.ring_size;
        return ConsoleEvidenceView{
            .capability_name = cap::TextSink::name,
            .requirement_role = role::log::name,
            .provider_instance = "h747.usart1.console",
            .runtime_domain = "h747_cm7",
            .transport = "usart1",
            .tx_mode = "dma_or_fallback",
            .rx_mode = "line_or_dma",
            .status = status_from_degradation(presentation.fallback,
                                               presentation.dropped,
                                               presentation.busy),
            .bytes_accepted = presentation.bytes,
            .fallback_count = presentation.fallback,
            .dropped_bytes = presentation.dropped,
            .busy_count = presentation.busy,
        };
    }
}

namespace topo = charm::backend::contract;

namespace {
    using LogReq = topo::Requirement<cap::TextSink, role::log>;
    using TraceReq = topo::Requirement<cap::TextSink, role::debug_trace>;
    using ShellReq = topo::Requirement<cap::LineSource, role::shell>;

    using LogProv = topo::Provided<cap::TextSink, role::log>;
    using TraceProv = topo::Provided<cap::TextSink, role::debug_trace>;
    using ShellProv = topo::Provided<cap::LineSource, role::shell>;

    using BufferedConsoleDesc = topo::ProviderDesc<provider_instance::host_buffered_console,
                                                   topo::ProviderSet<LogProv, ShellProv>>;
    using TraceBufferDesc = topo::ProviderDesc<provider_instance::host_trace_buffer,
                                               topo::ProviderSet<TraceProv>>;
    using Providers = std::tuple<BufferedConsoleDesc, TraceBufferDesc>;

    using BufferedConsoleMeta = topo::ProviderMeta<provider_instance::host_buffered_console,
                                                   provider_type::host_buffered_console_provider,
                                                   backend::host,
                                                   runtime_domain::host_process,
                                                   adapter::host_memory_console_adapter,
                                                   transport::memory_buffer>;
    using BufferedConsoleTransport = transport::memory_buffer;

    using LogBinding = topo::ProfileBinding<LogReq, provider_instance::host_buffered_console>;
    using ShellBinding = topo::ProfileBinding<ShellReq, provider_instance::host_buffered_console>;
    using TraceBinding = topo::ProfileBinding<TraceReq, provider_instance::host_trace_buffer>;
    using BadShellBinding = topo::ProfileBinding<ShellReq, provider_instance::host_trace_buffer>;
    using DuplicateBindings = std::tuple<LogBinding, ShellBinding, LogBinding>;
    using ValidBindings = std::tuple<LogBinding, ShellBinding>;
    using Requirements = topo::RequirementSet<LogReq, ShellReq>;

    static_assert(topo::CanMakeProfileBinding<LogReq, provider_instance::host_buffered_console>);
    static_assert(!topo::CanMakeProfileBinding<LogReq, provider_type::host_buffered_console_provider>);
    static_assert(!topo::CanMakeProfileBinding<LogReq, adapter::host_memory_console_adapter>);
    static_assert(!topo::CanMakeProfileBinding<LogReq, backend::host>);
    static_assert(!topo::CanMakeProfileBinding<LogReq, transport::memory_buffer>);
    static_assert(!topo::CanMakeProfileBinding<LogReq, hal::file_api>);

    static_assert(topo::binding_valid_v<LogBinding, Providers>);
    static_assert(topo::binding_valid_v<ShellBinding, Providers>);
    static_assert(topo::binding_valid_v<TraceBinding, Providers>);
    static_assert(!topo::binding_valid_v<BadShellBinding, Providers>);
    static_assert(topo::requirements_bound_once_v<ValidBindings, Requirements>);
    static_assert(!topo::requirements_bound_once_v<DuplicateBindings, Requirements>);

    struct BufferedConsole {
        std::array<std::byte, 32> tx{};
        std::size_t tx_used{0};
        std::size_t bytes_accepted{0};
        std::size_t fallback_count{0};
        std::size_t dropped_bytes{0};
        std::size_t busy_count{0};
        std::size_t flush_count{0};
        std::array<std::string_view, 3> scripted_lines{"status", "help", ""};
        std::size_t line_index{0};
        std::size_t lines_polled{0};

        [[nodiscard]] cap::Transfer write(const std::span<const std::byte> bytes) noexcept {
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
                return cap::Transfer{cap::Status{cap::StatusCode::busy}, accepted};
            }
            return cap::Transfer{cap::Status{}, accepted};
        }

        [[nodiscard]] cap::Transfer write(const std::string_view text) noexcept {
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
                return cap::Transfer{cap::Status{cap::StatusCode::busy}, accepted};
            }
            return cap::Transfer{cap::Status{}, accepted};
        }

        [[nodiscard]] cap::Status flush() noexcept {
            ++flush_count;
            return {};
        }

        [[nodiscard]] std::optional<std::string_view> poll_line() noexcept {
            ++lines_polled;
            if (line_index >= scripted_lines.size()) {
                return std::nullopt;
            }
            const auto line = scripted_lines[line_index++];
            if (line.empty()) {
                return std::nullopt;
            }
            return line;
        }
    };

    static_assert(cap::ByteSink::satisfied_by<BufferedConsole>);
    static_assert(cap::TextSink::satisfied_by<BufferedConsole>);
    static_assert(cap::LineSource::satisfied_by<BufferedConsole>);

    [[nodiscard]] console_evidence::ConsoleEvidenceFrame make_log_evidence(const BufferedConsole& provider) noexcept {
        return console_evidence::ConsoleEvidenceFrame{
            .capability_name = cap::TextSink::name,
            .requirement_role = role::log::name,
            .provider_instance = BufferedConsoleMeta::provider_instance::name,
            .provider_type = BufferedConsoleMeta::provider_type::name,
            .backend = BufferedConsoleMeta::backend::name,
            .runtime_domain = BufferedConsoleMeta::runtime_domain::name,
            .adapter = BufferedConsoleMeta::adapter::name,
            .transport = BufferedConsoleTransport::name,
            .tx_mode = "buffered",
            .rx_mode = "scripted_line",
            .status = console_evidence::status_from_degradation(provider.fallback_count,
                                                                 provider.dropped_bytes,
                                                                 provider.busy_count),
            .bytes_accepted = provider.bytes_accepted,
            .fallback_count = provider.fallback_count,
            .dropped_bytes = provider.dropped_bytes,
            .busy_count = provider.busy_count,
            .lines_polled = provider.lines_polled,
        };
    }

    [[nodiscard]] console_evidence::ConsoleEvidenceFrame make_shell_evidence(const BufferedConsole& provider) noexcept {
        auto frame = make_log_evidence(provider);
        frame.capability_name = cap::LineSource::name;
        frame.requirement_role = role::shell::name;
        return frame;
    }

    bool diagnostics_app(auto& context) {
        auto& log = context.template get<LogReq>();
        auto& shell = context.template get<ShellReq>();

        const auto hello = log.write("hello");
        const auto line = shell.poll_line();
        const auto flushed = log.flush();

        return hello.is_ok() &&
               hello.bytes == 5U &&
               line.has_value() &&
               *line == "status" &&
               flushed.is_ok();
    }

    bool expect(const bool condition, const char* message) {
        if (!condition) {
            std::fprintf(stderr, "[ERR] %s\n", message);
            return false;
        }
        return true;
    }

    bool run_smoke() {
        BufferedConsole provider{};
        console_evidence::ConsoleEvidenceCollector collector{};

        using LogRef = topo::ProviderRef<cap::TextSink, provider_instance::host_buffered_console, BufferedConsole>;
        using ShellRef = topo::ProviderRef<cap::LineSource, provider_instance::host_buffered_console, BufferedConsole>;
        topo::ContextView context{
            topo::RuntimeBinding<LogReq, LogRef>{LogRef{provider}},
            topo::RuntimeBinding<ShellReq, ShellRef>{ShellRef{provider}},
        };

        bool ok = true;
        ok &= expect(collector.count == 0U, "evidence collector should not be part of app execution");
        ok &= expect(diagnostics_app(context), "app should consume TextSink and LineSource requirements");
        ok &= expect(provider.tx_used == 5U && provider.flush_count == 1U, "provider should record app output");
        ok &= expect(provider.lines_polled == 1U, "provider should record line polling");
        ok &= expect(collector.count == 0U, "app should not collect provider evidence directly");

        const auto overflow = provider.write("abcdefghijklmnopqrstuvwxyz0123456789");
        ok &= expect(!overflow.is_ok(), "overflow should degrade provider transfer");
        ok &= expect(overflow.bytes == 27U, "Transfer.bytes should report accepted bytes in this smoke");
        ok &= expect(provider.dropped_bytes == 9U && provider.busy_count == 1U, "drop/busy should remain provider evidence");

        ok &= expect(collector.append(make_log_evidence(provider)), "log evidence should be collected outside app context");
        ok &= expect(collector.append(make_shell_evidence(provider)), "shell evidence should be collected outside app context");
        ok &= expect(collector.count == 2U, "collector should hold two evidence frames");

        const auto& log_frame = collector.frames[0];
        const auto& shell_frame = collector.frames[1];
        ok &= expect(log_frame.capability_name == "TextSink", "log evidence should keep capability name separate");
        ok &= expect(log_frame.requirement_role == "log", "log evidence should keep role separate");
        ok &= expect(shell_frame.capability_name == "LineSource", "shell evidence should keep capability name separate");
        ok &= expect(shell_frame.requirement_role == "shell", "shell evidence should keep role separate");
        ok &= expect(log_frame.provider_instance == "host.buffered_console", "evidence should expose provider instance");
        ok &= expect(log_frame.provider_type == "host buffered console provider", "provider type should remain metadata");
        ok &= expect(log_frame.adapter == "host_memory_console_adapter", "adapter should remain metadata");
        ok &= expect(log_frame.transport == "memory_buffer", "transport should remain metadata");
        ok &= expect(log_frame.status == console_evidence::EvidenceStatus::degraded, "busy/drop should degrade evidence status");
        ok &= expect(log_frame.bytes_accepted == 32U, "evidence should report accepted bytes");
        ok &= expect(log_frame.dropped_bytes == 9U, "evidence should report dropped bytes");
        ok &= expect(log_frame.busy_count == 1U, "evidence should report busy count");

        const auto host_view = console_evidence::project_view(log_frame);
        const auto h747_view = console_evidence::project_h747_console_tx(console_evidence::H747ConsoleTxPresentation{
            .started = 3U,
            .done = 3U,
            .bytes = 32U,
            .fallback = 0U,
            .dropped = 9U,
            .busy = 1U,
            .ring_used = 0U,
            .ring_size = 4096U,
        });

        ok &= expect(host_view.capability_name == "TextSink", "host view should preserve capability name");
        ok &= expect(host_view.requirement_role == "log", "host view should preserve requirement role");
        ok &= expect(h747_view.capability_name == "TextSink", "H747 view should preserve capability name");
        ok &= expect(h747_view.requirement_role == "log", "H747 view should preserve requirement role");
        ok &= expect(h747_view.provider_instance == "h747.usart1.console", "H747 view should use provider instance metadata");
        ok &= expect(h747_view.runtime_domain == "h747_cm7", "H747 view should carry runtime domain metadata");
        ok &= expect(h747_view.transport == "usart1", "H747 view should carry transport metadata");
        ok &= expect(h747_view.bytes_accepted == host_view.bytes_accepted, "host/H747 views should compare accepted bytes");
        ok &= expect(h747_view.dropped_bytes == host_view.dropped_bytes, "host/H747 views should compare dropped bytes");
        ok &= expect(h747_view.busy_count == host_view.busy_count, "host/H747 views should compare busy count");
        ok &= expect(h747_view.status == console_evidence::EvidenceStatus::degraded, "H747 presentation degradation should remain evidence");
        return ok;
    }
}

int main() {
    if (!run_smoke()) {
        return 1;
    }
    std::puts("[console-output-provider-smoke] ok");
    return 0;
}
