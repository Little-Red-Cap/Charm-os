#include "Backends/contract/block_storage.hpp"
#include "Backends/contract/capability_topology.hpp"

#include <array>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <span>
#include <string_view>
#include <tuple>

namespace topo = charm::backend::contract;
namespace block = charm::backend::contract::block;

namespace provider_instance {
    struct memory_block_app_store {
        using charm_provider_instance_tag = void;
        static constexpr std::string_view name{"host.memory_block_app_store"};
    };
}

namespace provider_type {
    struct memory_block_provider {};
}

namespace backend {
    struct host_reference {};
}

namespace runtime_domain {
    struct host_process {};
}

namespace adapter {
    struct memory_block_adapter {};
}

namespace media_kind {
    struct host_memory {};
}

namespace hal {
    struct file_api {};
}

namespace {
    struct MemoryBlock {
        static constexpr std::uint64_t kBlockSize = 8;
        static constexpr std::uint64_t kBlockCount = 2;

        std::array<std::byte, kBlockSize * kBlockCount> bytes{};
        block::StatusCode last_error{block::StatusCode::ok};

        [[nodiscard]] std::uint64_t block_size() const noexcept {
            return kBlockSize;
        }

        [[nodiscard]] std::uint64_t block_count() const noexcept {
            return kBlockCount;
        }

        [[nodiscard]] block::Status read(const std::uint64_t lba, const std::span<std::byte> out) noexcept {
            if (out.size() != kBlockSize || lba >= kBlockCount) {
                last_error = block::StatusCode::invalid_argument;
                return {last_error};
            }
            const auto offset = static_cast<std::size_t>(lba * kBlockSize);
            std::memcpy(out.data(), bytes.data() + offset, out.size());
            last_error = block::StatusCode::ok;
            return {};
        }

        [[nodiscard]] block::Status write(const std::uint64_t lba, const std::span<const std::byte> in) noexcept {
            if (in.size() != kBlockSize || lba >= kBlockCount) {
                last_error = block::StatusCode::invalid_argument;
                return {last_error};
            }
            const auto offset = static_cast<std::size_t>(lba * kBlockSize);
            std::memcpy(bytes.data() + offset, in.data(), in.size());
            last_error = block::StatusCode::ok;
            return {};
        }

        [[nodiscard]] block::Status flush() noexcept {
            last_error = block::StatusCode::ok;
            return {};
        }
    };

    struct MissingGeometry {
        [[nodiscard]] block::Status read(std::uint64_t, std::span<std::byte>) noexcept {
            return {};
        }

        [[nodiscard]] block::Status write(std::uint64_t, std::span<const std::byte>) noexcept {
            return {};
        }

        [[nodiscard]] block::Status flush() noexcept {
            return {};
        }
    };

    using AppStoreReq = topo::Requirement<block::BlockDevice, block::role::app_store>;
    using ResourceReq = topo::Requirement<block::BlockDevice, block::role::resource_media>;
    using AppStoreProv = topo::Provided<block::BlockDevice, block::role::app_store>;

    using ProviderDesc = topo::ProviderDesc<provider_instance::memory_block_app_store,
                                            topo::ProviderSet<AppStoreProv>>;
    using Providers = std::tuple<ProviderDesc>;
    using ProviderMeta = topo::ProviderMeta<provider_instance::memory_block_app_store,
                                            provider_type::memory_block_provider,
                                            backend::host_reference,
                                            runtime_domain::host_process,
                                            adapter::memory_block_adapter,
                                            media_kind::host_memory>;
    using Metas = std::tuple<ProviderMeta>;
    using AppStoreBinding = topo::ProfileBinding<AppStoreReq, provider_instance::memory_block_app_store>;
    using BadResourceBinding = topo::ProfileBinding<ResourceReq, provider_instance::memory_block_app_store>;
    using Requirements = topo::RequirementSet<AppStoreReq>;
    using Bindings = std::tuple<AppStoreBinding>;

    static_assert(block::BlockDevice::satisfied_by<MemoryBlock>);
    static_assert(!block::BlockDevice::satisfied_by<MissingGeometry>);
    static_assert(topo::binding_valid_v<AppStoreBinding, Providers>);
    static_assert(!topo::binding_valid_v<BadResourceBinding, Providers>);
    static_assert(topo::requirements_bound_once_v<Bindings, Requirements>);
    static_assert(topo::binding_has_meta_v<AppStoreBinding, Metas>);
    static_assert(!topo::CanMakeProfileBinding<AppStoreReq, provider_type::memory_block_provider>);
    static_assert(!topo::CanMakeProfileBinding<AppStoreReq, adapter::memory_block_adapter>);
    static_assert(!topo::CanMakeProfileBinding<AppStoreReq, block::BlockEndpoint>);
    static_assert(!topo::CanMakeProfileBinding<AppStoreReq, hal::file_api>);

    bool expect(const bool condition, const char* message) {
        if (!condition) {
            std::fprintf(stderr, "[ERR] %s\n", message);
            return false;
        }
        return true;
    }

    bool run_smoke() {
        MemoryBlock provider{};
        block::BlockEndpoint endpoint{
            .name = "block.host_app_store",
            .block_size = provider.block_size(),
            .block_count = provider.block_count(),
            .ctx = &provider,
            .read_fn = [](void* ctx, const std::uint64_t lba, const std::span<std::byte> out) noexcept {
                return static_cast<MemoryBlock*>(ctx)->read(lba, out);
            },
            .write_fn = [](void* ctx, const std::uint64_t lba, const std::span<const std::byte> in) noexcept {
                return static_cast<MemoryBlock*>(ctx)->write(lba, in);
            },
        };

        std::array<std::byte, MemoryBlock::kBlockSize> in{};
        std::array<std::byte, MemoryBlock::kBlockSize> out{};
        in[0] = std::byte{0x5A};

        bool ok = true;
        ok &= expect(block::write(endpoint, 0, in).is_ok(), "endpoint write should route to provider");
        ok &= expect(block::read(endpoint, 0, out).is_ok(), "endpoint read should route to provider");
        ok &= expect(out[0] == std::byte{0x5A}, "endpoint should round-trip data");

        std::array<std::byte, MemoryBlock::kBlockSize - 1> short_read{};
        const auto bad = block::read(endpoint, 0, short_read);
        ok &= expect(!bad.is_ok(), "invalid geometry should return error status");
        ok &= expect(provider.last_error == block::StatusCode::invalid_argument,
                     "provider should record last block error");

        const block::EvidenceFrame frame{
            .capability_name = block::BlockDevice::name,
            .requirement_role = block::role::app_store::name,
            .provider_instance = provider_instance::memory_block_app_store::name,
            .provider_type = "host memory block provider",
            .block_endpoint = endpoint.name,
            .runtime_domain = "host_process",
            .media_kind = "host_memory",
            .status = block::status_from_error(provider.last_error),
            .block_size = endpoint.block_size,
            .block_count = endpoint.block_count,
            .capacity = endpoint.block_size * endpoint.block_count,
            .read_count = 2U,
            .write_count = 1U,
            .last_error = provider.last_error,
        };
        const auto view = block::project_view(frame);

        ok &= expect(view.capability_name == "BlockDevice", "view should preserve capability");
        ok &= expect(view.requirement_role == "app_store", "view should preserve role");
        ok &= expect(view.provider_instance == "host.memory_block_app_store",
                     "view should preserve provider instance");
        ok &= expect(view.block_endpoint == "block.host_app_store",
                     "view should preserve provider-published endpoint");
        ok &= expect(view.provider_instance != view.block_endpoint,
                     "provider instance and block endpoint should stay distinct");
        ok &= expect(view.status == block::EvidenceStatus::degraded, "last error should degrade evidence");
        ok &= expect(view.capacity == MemoryBlock::kBlockSize * MemoryBlock::kBlockCount,
                     "view should report capacity");
        return ok;
    }
}

int main() {
    if (!run_smoke()) {
        return 1;
    }
    std::puts("[backends-contract-block-storage-header-smoke] ok");
    return 0;
}
