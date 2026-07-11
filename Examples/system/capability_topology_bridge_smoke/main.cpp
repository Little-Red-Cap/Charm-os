#include "Backends/contract/capability_topology.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <span>
#include <string_view>
#include <tuple>

namespace cap {
    struct TextSink {
        static constexpr std::string_view name{"TextSink"};

        template <typename T>
        static constexpr bool satisfied_by = requires(T& sink, std::string_view text) {
            { sink.write(text) } noexcept -> std::same_as<void>;
        };
    };

    struct BlockDevice {
        static constexpr std::string_view name{"BlockDevice"};

        template <typename T>
        static constexpr bool satisfied_by = requires(T& dev, std::uint64_t lba, std::span<std::byte> out) {
            { dev.read(lba, out) } noexcept -> std::same_as<bool>;
        };
    };
}

namespace role {
    struct log {
        static constexpr std::string_view name{"log"};
    };

    struct debug_trace {
        static constexpr std::string_view name{"debug_trace"};
    };

    struct app_store {
        static constexpr std::string_view name{"app_store"};
    };
}

namespace provider_instance {
    struct tag {
        using charm_provider_instance_tag = void;
    };

    struct host_memory_log : tag {
        static constexpr std::string_view name{"host.memory_log"};
    };

    struct host_shared_console : tag {
        static constexpr std::string_view name{"host.shared_console"};
    };

    struct host_memory_block_app_store : tag {
        static constexpr std::string_view name{"host.memory_block_app_store"};
    };
}

namespace provider_type {
    struct host_console_provider {
        static constexpr std::string_view name{"host console provider"};
    };

    struct host_block_provider {
        static constexpr std::string_view name{"host block provider"};
    };
}

namespace adapter {
    struct host_memory_log_adapter {
        static constexpr std::string_view name{"host_memory_log_adapter"};
    };

    struct host_memory_block_adapter {
        static constexpr std::string_view name{"host_memory_block_adapter"};
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

namespace hal {
    struct file_api {
        static constexpr std::string_view name{"file_api"};
    };
}

namespace block {
    struct BlockEndpoint {
        std::string_view name{};
        std::uint64_t block_size{0};
        std::uint64_t block_count{0};
    };
}

namespace local_evidence {
    enum class EvidenceStatus : std::uint8_t {
        ok,
        error,
    };

    struct EvidenceField {
        std::string_view key{};
        std::string_view value{};
    };

    struct EvidenceFrame {
        std::string_view capability_name{};
        std::string_view requirement_role{};
        std::string_view provider_instance{};
        std::string_view provider_type{};
        std::string_view backend{};
        std::string_view runtime_domain{};
        std::string_view adapter{};
        EvidenceStatus status{EvidenceStatus::ok};
        std::array<EvidenceField, 4> fields{};
        std::size_t field_count{0};
    };

    template <typename Meta>
    [[nodiscard]] constexpr EvidenceFrame make_evidence(const std::string_view capability_name,
                                                        const std::string_view requirement_role,
                                                        const EvidenceStatus status = EvidenceStatus::ok) noexcept {
        return EvidenceFrame{
            .capability_name = capability_name,
            .requirement_role = requirement_role,
            .provider_instance = Meta::provider_instance::name,
            .provider_type = Meta::provider_type::name,
            .backend = Meta::backend::name,
            .runtime_domain = Meta::runtime_domain::name,
            .adapter = Meta::adapter::name,
            .status = status,
        };
    }
}

namespace topo = charm::backend::contract;

namespace {
    using LogReq = topo::Requirement<cap::TextSink, role::log>;
    using TraceReq = topo::Requirement<cap::TextSink, role::debug_trace>;
    using AppStoreReq = topo::Requirement<cap::BlockDevice, role::app_store>;

    using LogProv = topo::Provided<cap::TextSink, role::log>;
    using TraceProv = topo::Provided<cap::TextSink, role::debug_trace>;
    using AppStoreProv = topo::Provided<cap::BlockDevice, role::app_store>;

    using MemoryLogDesc = topo::ProviderDesc<provider_instance::host_memory_log,
                                             topo::ProviderSet<LogProv>>;
    using SharedConsoleDesc = topo::ProviderDesc<provider_instance::host_shared_console,
                                                 topo::ProviderSet<LogProv, TraceProv>>;
    using AppStoreBlockDesc = topo::ProviderDesc<provider_instance::host_memory_block_app_store,
                                                 topo::ProviderSet<AppStoreProv>>;
    using Providers = std::tuple<MemoryLogDesc, SharedConsoleDesc, AppStoreBlockDesc>;

    using MemoryLogMeta = topo::ProviderMeta<provider_instance::host_memory_log,
                                             provider_type::host_console_provider,
                                             backend::host,
                                             runtime_domain::host_process,
                                             adapter::host_memory_log_adapter>;
    using AppStoreBlockMeta = topo::ProviderMeta<provider_instance::host_memory_block_app_store,
                                                 provider_type::host_block_provider,
                                                 backend::host,
                                                 runtime_domain::host_process,
                                                 adapter::host_memory_block_adapter>;
    using Metas = std::tuple<MemoryLogMeta, AppStoreBlockMeta>;

    using LogBinding = topo::ProfileBinding<LogReq, provider_instance::host_memory_log>;
    using TraceBinding = topo::ProfileBinding<TraceReq, provider_instance::host_shared_console>;
    using SharedLogBinding = topo::ProfileBinding<LogReq, provider_instance::host_shared_console>;
    using AppStoreBinding = topo::ProfileBinding<AppStoreReq, provider_instance::host_memory_block_app_store>;
    using BadTraceBinding = topo::ProfileBinding<TraceReq, provider_instance::host_memory_log>;

    using Requirements = topo::RequirementSet<LogReq, TraceReq, AppStoreReq>;
    using Bindings = std::tuple<LogBinding, TraceBinding, AppStoreBinding>;
    using DuplicateLogBindings = std::tuple<LogBinding, SharedLogBinding, TraceBinding, AppStoreBinding>;

    static_assert(topo::CanMakeProfileBinding<LogReq, provider_instance::host_memory_log>);
    static_assert(!topo::CanMakeProfileBinding<LogReq, provider_type::host_console_provider>);
    static_assert(!topo::CanMakeProfileBinding<LogReq, adapter::host_memory_log_adapter>);
    static_assert(!topo::CanMakeProfileBinding<LogReq, backend::host>);
    static_assert(!topo::CanMakeProfileBinding<LogReq, hal::file_api>);
    static_assert(!topo::CanMakeProfileBinding<AppStoreReq, block::BlockEndpoint>);

    static_assert(topo::binding_valid_v<LogBinding, Providers>);
    static_assert(topo::binding_valid_v<TraceBinding, Providers>);
    static_assert(topo::binding_valid_v<AppStoreBinding, Providers>);
    static_assert(!topo::binding_valid_v<BadTraceBinding, Providers>);
    static_assert(topo::requirements_bound_once_v<Bindings, Requirements>);
    static_assert(!topo::requirements_bound_once_v<DuplicateLogBindings, Requirements>);
    static_assert(topo::binding_has_meta_v<LogBinding, Metas>);
    static_assert(topo::binding_has_meta_v<AppStoreBinding, Metas>);

    struct MemoryLog {
        std::array<char, 128> bytes{};
        std::size_t used{0};
        std::uint32_t writes{0};

        void write(const std::string_view text) noexcept {
            const auto remaining = bytes.size() - used;
            const auto count = text.size() < remaining ? text.size() : remaining;
            if (count != 0U) {
                std::memcpy(bytes.data() + used, text.data(), count);
                used += count;
            }
            ++writes;
        }
    };

    struct MemoryBlock {
        static constexpr std::uint64_t block_size = 16;
        static constexpr std::uint64_t block_count = 2;

        std::array<std::byte, block_size * block_count> bytes{};
        block::BlockEndpoint endpoint{"block.host_app_store", block_size, block_count};

        bool read(const std::uint64_t lba, const std::span<std::byte> out) noexcept {
            if (out.size() != block_size || lba >= block_count) {
                return false;
            }
            const auto offset = static_cast<std::size_t>(lba * block_size);
            std::memcpy(out.data(), bytes.data() + offset, out.size());
            return true;
        }
    };

    bool expect(const bool condition, const char* message) {
        if (!condition) {
            std::fprintf(stderr, "[ERR] %s\n", message);
            return false;
        }
        return true;
    }

    bool run_smoke() {
        static_assert(cap::TextSink::satisfied_by<MemoryLog>);
        static_assert(cap::BlockDevice::satisfied_by<MemoryBlock>);

        MemoryLog log{};
        log.write("topology");

        MemoryBlock block_dev{};
        block_dev.bytes[0] = std::byte{0x43};
        std::array<std::byte, MemoryBlock::block_size> readback{};
        const auto read_ok = block_dev.read(0, readback);

        const auto log_evidence = local_evidence::make_evidence<MemoryLogMeta>(cap::TextSink::name, role::log::name);
        auto block_evidence = local_evidence::make_evidence<AppStoreBlockMeta>(cap::BlockDevice::name, role::app_store::name);
        block_evidence.fields[0] = local_evidence::EvidenceField{"block_endpoint", block_dev.endpoint.name};
        block_evidence.fields[1] = local_evidence::EvidenceField{"block_size", "16"};
        block_evidence.field_count = 2;

        bool ok = true;
        ok &= expect(log.used == 8U && log.writes == 1U, "TextSink provider should accept text");
        ok &= expect(read_ok && readback[0] == std::byte{0x43}, "BlockDevice provider should read block data");
        ok &= expect(log_evidence.capability_name == "TextSink", "evidence should keep capability name separate");
        ok &= expect(log_evidence.requirement_role == "log", "evidence should keep requirement role separate");
        ok &= expect(log_evidence.provider_instance == "host.memory_log", "evidence should report provider instance");
        ok &= expect(log_evidence.provider_type == "host console provider", "evidence should report provider type only as metadata");
        ok &= expect(block_evidence.capability_name == "BlockDevice", "block evidence should not encode role in capability name");
        ok &= expect(block_evidence.requirement_role == "app_store", "block evidence should expose app_store as role");
        ok &= expect(block_evidence.fields[0].value == "block.host_app_store", "block endpoint should be provider-published evidence");
        ok &= expect(block_evidence.provider_instance != block_evidence.fields[0].value,
                     "block endpoint must not replace provider instance identity");
        return ok;
    }
}

int main() {
    if (!run_smoke()) {
        return 1;
    }
    std::puts("[capability-topology-bridge-smoke] ok");
    return 0;
}
