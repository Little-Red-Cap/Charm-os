module;

#include <array>
#include <cstddef>
#include <optional>
#include <span>
#include <string_view>

export module block.file.node;

import block.device;
import block.file;
import block.registry;
import init.node;
import util.core;
import util.error;

export namespace block {
    template <typename RegistryT>
    struct FileBinding {
        std::optional<FileDevice> file{};
        RegistryT* registry{nullptr};
        DeviceDesc desc{};
        const char* path{nullptr};
        util::u64 block_size{0};
        const char* registry_cap_name{"block.registry"};
        std::array<init::CapId, 1> provides{};
        std::array<init::CapId, 1> requires_caps{};
        init::Node node{};

        FileBinding(RegistryT& reg,
                    const char* file_path,
                    util::u64 block_sz,
                    const char* cap_name = "block.sd0",
                    init::Phase phase = init::Phase::core,
                    util::u32 runlevel_mask = static_cast<util::u32>(init::Runlevel::all)) noexcept
            : registry(&reg),
              desc{cap_name, cap_id(cap_name)},
              path(file_path),
              block_size(block_sz) {
            provides[0] = init::cap_id(cap_name);
            requires_caps[0] = init::cap_id("block.registry");
            node = init::Node{
                cap_name,
                phase,
                runlevel_mask,
                std::span<const init::CapId>(provides.data(), provides.size()),
                std::span<const init::CapId>(requires_caps.data(), requires_caps.size()),
                &FileBinding::init_trampoline,
                nullptr,
                this
            };
        }

        constexpr std::string_view capability_name(init::CapId id) const noexcept {
            if (id == provides[0]) {
                return desc.name;
            }
            if (id == requires_caps[0]) {
                return std::string_view{registry_cap_name};
            }
            return {};
        }

        static util::Result<void> init_trampoline(void* ctx) noexcept {
            auto* self = static_cast<FileBinding*>(ctx);
            if (!self || !self->registry || !self->path || self->block_size == 0) {
                return util::unexpected(util::Errc::invalid_arg);
            }
            if (!self->file) self->file.emplace();
            const auto st = self->file->open(self->path, self->block_size);
            if (!st) return util::unexpected(util::Errc::io);
            auto& dev = self->file->device();
            if (dev.caps == 0) {
                dev.caps = caps_from_ops(dev);
            }
            return self->registry->register_device(self->desc, dev);
        }
    };
}
