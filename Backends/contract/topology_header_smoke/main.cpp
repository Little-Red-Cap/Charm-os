#include "Backends/contract/capability_topology.hpp"

#include <cstdio>
#include <string_view>
#include <tuple>

namespace topo = charm::backend::contract;

namespace capability {
    struct TextSink {
        static constexpr std::string_view name{"TextSink"};

        template <typename T>
        static constexpr bool satisfied_by = requires(T& sink, std::string_view text) {
            { sink.write(text) } noexcept -> std::same_as<void>;
        };
    };
}

namespace role {
    struct log {
        static constexpr std::string_view name{"log"};
    };

    struct trace {
        static constexpr std::string_view name{"trace"};
    };
}

namespace provider_instance {
    struct memory_log {
        using charm_provider_instance_tag = void;
        static constexpr std::string_view name{"host.memory_log"};
    };

    struct trace_log {
        using charm_provider_instance_tag = void;
        static constexpr std::string_view name{"host.trace_log"};
    };
}

namespace provider_type {
    struct memory_log_provider {
        static constexpr std::string_view name{"memory log provider"};
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

namespace adapter {
    struct memory_adapter {
        static constexpr std::string_view name{"memory_adapter"};
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

namespace endpoint {
    struct text_endpoint {
        static constexpr std::string_view name{"text.endpoint"};
    };
}

namespace {
    using LogReq = topo::Requirement<capability::TextSink, role::log>;
    using TraceReq = topo::Requirement<capability::TextSink, role::trace>;
    using LogProv = topo::Provided<capability::TextSink, role::log>;
    using TraceProv = topo::Provided<capability::TextSink, role::trace>;

    using MemoryLogProvider = topo::ProviderDesc<provider_instance::memory_log,
                                                 topo::ProviderSet<LogProv>>;
    using TraceLogProvider = topo::ProviderDesc<provider_instance::trace_log,
                                                topo::ProviderSet<TraceProv>>;
    using Providers = std::tuple<MemoryLogProvider, TraceLogProvider>;

    using MemoryLogMeta = topo::ProviderMeta<provider_instance::memory_log,
                                             provider_type::memory_log_provider,
                                             backend::host,
                                             runtime_domain::host_process,
                                             adapter::memory_adapter,
                                             transport::memory_buffer>;
    using Metas = std::tuple<MemoryLogMeta>;

    using LogBinding = topo::ProfileBinding<LogReq, provider_instance::memory_log>;
    using TraceBinding = topo::ProfileBinding<TraceReq, provider_instance::trace_log>;
    using BadTraceBinding = topo::ProfileBinding<TraceReq, provider_instance::memory_log>;
    using DuplicateBindings = std::tuple<LogBinding, LogBinding>;
    using ValidBindings = std::tuple<LogBinding, TraceBinding>;
    using Requirements = topo::RequirementSet<LogReq, TraceReq>;

    static_assert(topo::ProviderInstanceToken<provider_instance::memory_log>);
    static_assert(!topo::ProviderInstanceToken<provider_type::memory_log_provider>);
    static_assert(!topo::CanMakeProfileBinding<LogReq, provider_type::memory_log_provider>);
    static_assert(!topo::CanMakeProfileBinding<LogReq, backend::host>);
    static_assert(!topo::CanMakeProfileBinding<LogReq, runtime_domain::host_process>);
    static_assert(!topo::CanMakeProfileBinding<LogReq, adapter::memory_adapter>);
    static_assert(!topo::CanMakeProfileBinding<LogReq, transport::memory_buffer>);
    static_assert(!topo::CanMakeProfileBinding<LogReq, hal::file_api>);
    static_assert(!topo::CanMakeProfileBinding<LogReq, endpoint::text_endpoint>);

    static_assert(topo::provider_declares_requirement_v<LogReq, MemoryLogProvider>);
    static_assert(!topo::provider_declares_requirement_v<TraceReq, MemoryLogProvider>);
    static_assert(topo::binding_valid_v<LogBinding, Providers>);
    static_assert(topo::binding_valid_v<TraceBinding, Providers>);
    static_assert(!topo::binding_valid_v<BadTraceBinding, Providers>);
    static_assert(topo::requirements_bound_once_v<ValidBindings, Requirements>);
    static_assert(!topo::requirements_bound_once_v<DuplicateBindings, topo::RequirementSet<LogReq>>);
    static_assert(topo::binding_has_meta_v<LogBinding, Metas>);
    static_assert(!topo::binding_has_meta_v<TraceBinding, Metas>);
    static_assert(std::same_as<MemoryLogMeta::extra_metadata, std::tuple<transport::memory_buffer>>);

    struct MemoryLog {
        std::size_t writes{0};
        std::string_view last{};

        void write(const std::string_view text) noexcept {
            ++writes;
            last = text;
        }
    };

    static_assert(capability::TextSink::satisfied_by<MemoryLog>);

    bool expect(const bool condition, const char* message) {
        if (!condition) {
            std::fprintf(stderr, "[ERR] %s\n", message);
            return false;
        }
        return true;
    }

    bool run_smoke() {
        MemoryLog memory_log{};
        using LogRef = topo::ProviderRef<capability::TextSink, provider_instance::memory_log, MemoryLog>;
        topo::ContextView context{
            topo::RuntimeBinding<LogReq, LogRef>{LogRef{memory_log}},
        };

        auto& log = context.get<LogReq>();
        log.write("contract");

        bool ok = true;
        ok &= expect(memory_log.writes == 1U, "ContextView should expose provider implementation through requirement");
        ok &= expect(memory_log.last == "contract", "provider implementation should receive capability call");
        return ok;
    }
}

int main() {
    if (!run_smoke()) {
        return 1;
    }
    std::puts("[backends-contract-topology-header-smoke] ok");
    return 0;
}
