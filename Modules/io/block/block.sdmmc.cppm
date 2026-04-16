module;

#include <array>
#include <span>
#include <string_view>

export module block.sdmmc;

import block.device;
import block.registry;
import init.node;
import util.core;
import util.error;

export namespace block {
    struct SdmmcInfo {
        util::u64 block_size{0};
        util::u64 block_count{0};
    };

    struct SdmmcConfig {
        util::u32 clock_hz{0};
        util::u8 bus_width{4};
        bool use_dma{false};
    };

    struct SdmmcOps {
        Status (*init)(void* ctx, const SdmmcConfig& cfg, SdmmcInfo& out) noexcept { nullptr };
        Status (*read)(void* ctx, util::u64 lba, std::span<util::u8> data) noexcept { nullptr };
        Status (*write)(void* ctx, util::u64 lba, std::span<const util::u8> data) noexcept { nullptr };
        Status (*erase)(void* ctx, util::u64 lba, util::u64 count) noexcept { nullptr };
        Status (*flush)(void* ctx) noexcept { nullptr };
    };

    struct SdmmcHandle {
        void* ctx{nullptr};
        const SdmmcOps* ops{nullptr};
    };

    template <typename RegistryT>
    struct SdmmcBinding {
        SdmmcHandle handle{};
        SdmmcConfig config{};
        RegistryT* registry{nullptr};
        DeviceDesc desc{};
        Device device{};
        const char* registry_cap_name{"block.registry"};
        const char* hal_cap_name{nullptr};
        std::array<init::CapId, 1> provides{};
        std::array<init::CapId, 2> requires_caps{};
        util::usize requires_count{0};
        init::Node node{};

        SdmmcBinding(RegistryT& reg,
                     SdmmcHandle h,
                     const SdmmcConfig& cfg,
                     const char* cap_name = "block.sd0",
                     const char* hal_cap_name = nullptr,
                     init::Phase phase = init::Phase::core,
                     util::u32 runlevel_mask = static_cast<util::u32>(init::Runlevel::all)) noexcept
            : handle(h),
              config(cfg),
              registry(&reg),
              desc{cap_name, cap_id(cap_name)},
              hal_cap_name(hal_cap_name) {
            provides[0] = init::cap_id(cap_name);
            requires_caps[0] = init::cap_id("block.registry");
            requires_count = 1;
            if (hal_cap_name) {
                requires_caps[1] = init::cap_id(hal_cap_name);
                requires_count = 2;
            }
            node = init::Node{
                cap_name,
                phase,
                runlevel_mask,
                std::span<const init::CapId>(provides.data(), provides.size()),
                std::span<const init::CapId>(requires_caps.data(), requires_count),
                &SdmmcBinding::init_trampoline,
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
            if (requires_count > 1 && id == requires_caps[1]) {
                return std::string_view{hal_cap_name ? hal_cap_name : ""};
            }
            return {};
        }

        static util::Result<void> init_trampoline(void* ctx) noexcept {
            auto* self = static_cast<SdmmcBinding*>(ctx);
            if (!self || !self->registry || !self->handle.ops) {
                return util::unexpected(util::Errc::invalid_arg);
            }
            if (!self->handle.ops->init || !self->handle.ops->read) {
                return util::unexpected(util::Errc::invalid_arg);
            }
            SdmmcInfo info{};
            auto st = self->handle.ops->init(self->handle.ctx, self->config, info);
            if (!st) return util::unexpected(st.err);
            if (info.block_size == 0 || info.block_count == 0) {
                return util::unexpected(util::Errc::invalid_arg);
            }
            self->device.ctx = self;
            self->device.read = &SdmmcBinding::read_impl;
            self->device.write = &SdmmcBinding::write_impl;
            self->device.erase = &SdmmcBinding::erase_impl;
            self->device.flush = &SdmmcBinding::flush_impl;
            self->device.block_size = info.block_size;
            self->device.block_count = info.block_count;
            self->device.caps = caps_from_ops(self->device);
            return self->registry->register_device(self->desc, self->device);
        }

    private:
        static Status read_impl(void* ctx, util::u64 lba, std::span<util::u8> data) noexcept {
            auto* self = static_cast<SdmmcBinding*>(ctx);
            if (!self || !self->handle.ops || !self->handle.ops->read) {
                return Status{Errc::nosys};
            }
            if (data.empty() || (data.size() % self->device.block_size) != 0) {
                return Status{Errc::inval};
            }
            return self->handle.ops->read(self->handle.ctx, lba, data);
        }

        static Status write_impl(void* ctx, util::u64 lba, std::span<const util::u8> data) noexcept {
            auto* self = static_cast<SdmmcBinding*>(ctx);
            if (!self || !self->handle.ops || !self->handle.ops->write) {
                return Status{Errc::nosys};
            }
            if (data.empty() || (data.size() % self->device.block_size) != 0) {
                return Status{Errc::inval};
            }
            return self->handle.ops->write(self->handle.ctx, lba, data);
        }

        static Status erase_impl(void* ctx, util::u64 lba, util::u64 count) noexcept {
            auto* self = static_cast<SdmmcBinding*>(ctx);
            if (!self || !self->handle.ops || !self->handle.ops->erase) {
                return Status{Errc::nosys};
            }
            if (count == 0) return Status{Errc::inval};
            return self->handle.ops->erase(self->handle.ctx, lba, count);
        }

        static Status flush_impl(void* ctx) noexcept {
            auto* self = static_cast<SdmmcBinding*>(ctx);
            if (!self || !self->handle.ops || !self->handle.ops->flush) {
                return Status{Errc::ok};
            }
            return self->handle.ops->flush(self->handle.ctx);
        }
    };
}
