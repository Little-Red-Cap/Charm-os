#include "Backends/contract/block_storage.hpp"
#include "Modules/core/capability/relations.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <span>
#include <string_view>

namespace cap {
    using Status = charm::backend::contract::block::Status;
    using StatusCode = charm::backend::contract::block::StatusCode;
    using BlockDevice = charm::backend::contract::block::BlockDevice;
}

namespace requirement_label {
    struct app_store {
        static constexpr std::string_view name{"app_store"};
    };
}

namespace provider_instance {
    struct tag {
        using charm_provider_instance_tag = void;
    };

    struct host_memory_block_app_store : tag {
        static constexpr std::string_view name{"host.memory_block_app_store"};
    };

    struct host_memory_block_resource : tag {
        static constexpr std::string_view name{"host.memory_block_resource"};
    };
}

namespace provider_type {
    struct host_memory_block_provider {
        static constexpr std::string_view name{"host memory block provider"};
    };
}

namespace adapter {
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

namespace media_kind {
    struct host_memory {
        static constexpr std::string_view name{"host_memory"};
    };
}

namespace hal {
    struct file_api {
        static constexpr std::string_view name{"file_api"};
    };
}

namespace block {
    using BlockEndpoint = charm::backend::contract::block::BlockEndpoint;
    using charm::backend::contract::block::read;
    using charm::backend::contract::block::write;
}

namespace block_evidence {
    using EvidenceStatus = charm::backend::contract::block::EvidenceStatus;
    using BlockEvidenceFrame = charm::backend::contract::block::EvidenceFrame;
    using BlockEvidenceView = charm::backend::contract::block::EvidenceView;
    using charm::backend::contract::block::project_view;
    using charm::backend::contract::block::status_from_error;
}

namespace relation = charm::capability;

namespace {
    enum class ContractKey : std::uint8_t {
        block_device,
    };
    enum class RequirementKey : std::uint8_t {
        app_store,
    };
    enum class ProvisionKey : std::uint8_t {
        memory_block,
    };

    constexpr relation::Requirement<ContractKey, RequirementKey> requirement{
        RequirementKey::app_store, ContractKey::block_device};
    constexpr relation::Provision<ContractKey, ProvisionKey> provision{
        ProvisionKey::memory_block, ContractKey::block_device};
    constexpr relation::Binding<RequirementKey, ProvisionKey> binding{
        RequirementKey::app_store, ProvisionKey::memory_block};

    struct AppStoreMeta {
        using provider_instance = provider_instance::host_memory_block_app_store;
        using provider_type = provider_type::host_memory_block_provider;
        using backend = backend::host;
        using runtime_domain = runtime_domain::host_process;
        using adapter = adapter::host_memory_block_adapter;
    };
    using AppStoreMediaKind = media_kind::host_memory;

    static_assert(requirement.contract == provision.contract);
    static_assert(binding.requirement == requirement.key);
    static_assert(binding.provision == provision.key);

    struct MemoryBlockProvider {
        static constexpr std::uint64_t kBlockSize = 16;
        static constexpr std::uint64_t kBlockCount = 2;

        std::array<std::byte, kBlockSize * kBlockCount> bytes{};
        std::size_t read_count{0};
        std::size_t write_count{0};
        cap::StatusCode last_error{cap::StatusCode::ok};

        [[nodiscard]] std::uint64_t block_size() const noexcept {
            return kBlockSize;
        }

        [[nodiscard]] std::uint64_t block_count() const noexcept {
            return kBlockCount;
        }

        [[nodiscard]] cap::Status read(const std::uint64_t lba, const std::span<std::byte> out) noexcept {
            ++read_count;
            if (out.size() != kBlockSize || lba >= kBlockCount) {
                last_error = cap::StatusCode::invalid_argument;
                return {last_error};
            }
            const auto offset = static_cast<std::size_t>(lba * kBlockSize);
            std::memcpy(out.data(), bytes.data() + offset, out.size());
            last_error = cap::StatusCode::ok;
            return {};
        }

        [[nodiscard]] cap::Status write(const std::uint64_t lba, const std::span<const std::byte> in) noexcept {
            ++write_count;
            if (in.size() != kBlockSize || lba >= kBlockCount) {
                last_error = cap::StatusCode::invalid_argument;
                return {last_error};
            }
            const auto offset = static_cast<std::size_t>(lba * kBlockSize);
            std::memcpy(bytes.data() + offset, in.data(), in.size());
            last_error = cap::StatusCode::ok;
            return {};
        }

        [[nodiscard]] cap::Status flush() noexcept {
            last_error = cap::StatusCode::ok;
            return {};
        }

        [[nodiscard]] block::BlockEndpoint publish_endpoint() noexcept {
            return block::BlockEndpoint{
                .name = "block.host_app_store",
                .block_size = block_size(),
                .block_count = block_count(),
                .ctx = this,
                .read_fn = &MemoryBlockProvider::endpoint_read,
                .write_fn = &MemoryBlockProvider::endpoint_write,
            };
        }

        [[nodiscard]] static cap::Status endpoint_read(void* ctx,
                                                       const std::uint64_t lba,
                                                       const std::span<std::byte> out) noexcept {
            auto* self = static_cast<MemoryBlockProvider*>(ctx);
            if (!self) {
                return {cap::StatusCode::no_entry};
            }
            return self->read(lba, out);
        }

        [[nodiscard]] static cap::Status endpoint_write(void* ctx,
                                                        const std::uint64_t lba,
                                                        const std::span<const std::byte> in) noexcept {
            auto* self = static_cast<MemoryBlockProvider*>(ctx);
            if (!self) {
                return {cap::StatusCode::no_entry};
            }
            return self->write(lba, in);
        }
    };

    static_assert(cap::BlockDevice::satisfied_by<MemoryBlockProvider>);

    struct StorageConsumer {
        std::size_t read_ops{0};
        std::size_t write_ops{0};

        [[nodiscard]] bool write_then_read(block::BlockEndpoint& endpoint) noexcept {
            std::array<std::byte, MemoryBlockProvider::kBlockSize> in{};
            std::array<std::byte, MemoryBlockProvider::kBlockSize> out{};
            in[0] = std::byte{0x43};
            in[1] = std::byte{0x48};

            const auto written = block::write(endpoint, 0, in);
            ++write_ops;
            if (!written.is_ok()) {
                return false;
            }
            const auto read = block::read(endpoint, 0, out);
            ++read_ops;
            return read.is_ok() && out[0] == in[0] && out[1] == in[1];
        }
    };

    [[nodiscard]] block_evidence::BlockEvidenceFrame make_evidence(const MemoryBlockProvider& provider,
                                                                   const block::BlockEndpoint& endpoint) noexcept {
        const auto block_size = provider.block_size();
        const auto block_count = provider.block_count();
        return block_evidence::BlockEvidenceFrame{
            .capability_name = cap::BlockDevice::label,
            .requirement_role = requirement_label::app_store::name,
            .provider_instance = AppStoreMeta::provider_instance::name,
            .provider_type = AppStoreMeta::provider_type::name,
            .block_endpoint = endpoint.name,
            .runtime_domain = AppStoreMeta::runtime_domain::name,
            .media_kind = AppStoreMediaKind::name,
            .status = block_evidence::status_from_error(provider.last_error),
            .block_size = block_size,
            .block_count = block_count,
            .capacity = block_size * block_count,
            .read_count = provider.read_count,
            .write_count = provider.write_count,
            .last_error = provider.last_error,
        };
    }

    bool expect(const bool condition, const char* message) {
        if (!condition) {
            std::fprintf(stderr, "[ERR] %s\n", message);
            return false;
        }
        return true;
    }

    bool run_smoke() {
        MemoryBlockProvider provider{};
        auto endpoint = provider.publish_endpoint();
        StorageConsumer consumer{};

        bool ok = true;
        ok &= expect(endpoint.name == "block.host_app_store", "provider should publish stable block endpoint");
        ok &= expect(endpoint.ctx == &provider, "endpoint should carry provider-owned context");
        ok &= expect(endpoint.name != AppStoreMeta::provider_instance::name,
                     "BlockEndpoint must not replace provider instance identity");

        ok &= expect(consumer.write_then_read(endpoint), "storage consumer should use BlockEndpoint for block access");
        ok &= expect(provider.read_count == 1U && provider.write_count == 1U,
                     "provider should record read/write counters as evidence");

        std::array<std::byte, MemoryBlockProvider::kBlockSize - 1> short_read{};
        const auto bad_read = block::read(endpoint, 0, short_read);
        ok &= expect(!bad_read.is_ok(), "invalid read should return degraded provider status");
        ok &= expect(provider.last_error == cap::StatusCode::invalid_argument,
                     "invalid read should update provider last_error evidence");

        const auto evidence = make_evidence(provider, endpoint);
        const auto view = block_evidence::project_view(evidence);

        ok &= expect(evidence.capability_name == "BlockDevice", "evidence should keep capability name separate");
        ok &= expect(evidence.requirement_role == "app_store", "evidence should keep requirement role separate");
        ok &= expect(evidence.provider_instance == "host.memory_block_app_store",
                     "evidence should expose provider instance");
        ok &= expect(evidence.provider_type == "host memory block provider",
                     "provider type should remain metadata");
        ok &= expect(evidence.block_endpoint == "block.host_app_store",
                     "evidence should expose provider-published block endpoint");
        ok &= expect(evidence.provider_instance != evidence.block_endpoint,
                     "provider instance and BlockEndpoint should stay distinct");
        ok &= expect(view.runtime_domain == "host_process", "view should carry runtime domain metadata");
        ok &= expect(view.media_kind == "host_memory", "view should carry media kind metadata");
        ok &= expect(view.block_size == MemoryBlockProvider::kBlockSize, "view should report block size");
        ok &= expect(view.block_count == MemoryBlockProvider::kBlockCount, "view should report block count");
        ok &= expect(view.capacity == MemoryBlockProvider::kBlockSize * MemoryBlockProvider::kBlockCount,
                     "view should report capacity");
        ok &= expect(view.read_count == 2U && view.write_count == 1U,
                     "view should report read/write counters");
        ok &= expect(view.status == block_evidence::EvidenceStatus::degraded,
                     "last_error should degrade evidence without changing BlockDevice API");
        ok &= expect(view.last_error == cap::StatusCode::invalid_argument,
                     "view should report last provider error");
        return ok;
    }
}

int main() {
    if (!run_smoke()) {
        return 1;
    }
    std::puts("[block-storage-provider-smoke] ok");
    return 0;
}
