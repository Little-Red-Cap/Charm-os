module;

#include <array>
#include <span>

export module block.spi_flash;

import block.device;
import block.registry;
import init.node;
import util.core;
import util.error;

export namespace block {
    struct SpiFlashInfo {
        util::u64 block_size{0};
        util::u64 block_count{0};
    };

    struct SpiFlashConfig {
        util::u32 clock_hz{0};
        bool quad_enable{false};
    };

    struct SpiFlashOps {
        Status (*init)(void* ctx, const SpiFlashConfig& cfg, SpiFlashInfo& out) noexcept { nullptr };
        Status (*read)(void* ctx, util::u64 addr, std::span<util::u8> data) noexcept { nullptr };
        Status (*write)(void* ctx, util::u64 addr, std::span<const util::u8> data) noexcept { nullptr };
        Status (*erase)(void* ctx, util::u64 addr, util::u64 size) noexcept { nullptr };
        Status (*flush)(void* ctx) noexcept { nullptr };
    };

    struct SpiFlashHandle {
        void* ctx{nullptr};
        const SpiFlashOps* ops{nullptr};
    };

    template <typename RegistryT>
    struct SpiFlashBinding {
        SpiFlashHandle handle{};
        SpiFlashConfig config{};
        RegistryT* registry{nullptr};
        DeviceDesc desc{};
        Device device{};
        std::array<init::CapId, 1> provides{};
        std::array<init::CapId, 2> requires_caps{};
        util::usize requires_count{0};
        init::Node node{};

        SpiFlashBinding(RegistryT& reg,
                        SpiFlashHandle h,
                        const SpiFlashConfig& cfg,
                        const char* cap_name = "block.flash0",
                        const char* hal_cap_name = nullptr,
                        init::Phase phase = init::Phase::core,
                        util::u32 runlevel_mask = static_cast<util::u32>(init::Runlevel::all)) noexcept
            : handle(h),
              config(cfg),
              registry(&reg),
              desc{cap_name, cap_id(cap_name)} {
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
                &SpiFlashBinding::init_trampoline,
                nullptr,
                this
            };
        }

        static util::Result<void> init_trampoline(void* ctx) noexcept {
            auto* self = static_cast<SpiFlashBinding*>(ctx);
            if (!self || !self->registry || !self->handle.ops) {
                return util::unexpected(util::Errc::invalid_arg);
            }
            if (!self->handle.ops->init || !self->handle.ops->read) {
                return util::unexpected(util::Errc::invalid_arg);
            }
            SpiFlashInfo info{};
            auto st = self->handle.ops->init(self->handle.ctx, self->config, info);
            if (!st) return util::unexpected(st.err);
            if (info.block_size == 0 || info.block_count == 0) {
                return util::unexpected(util::Errc::invalid_arg);
            }
            self->device.ctx = self;
            self->device.read = &SpiFlashBinding::read_impl;
            self->device.write = &SpiFlashBinding::write_impl;
            self->device.erase = &SpiFlashBinding::erase_impl;
            self->device.flush = &SpiFlashBinding::flush_impl;
            self->device.block_size = info.block_size;
            self->device.block_count = info.block_count;
            return self->registry->register_device(self->desc, self->device);
        }

    private:
        static Status read_impl(void* ctx, util::u64 lba, std::span<util::u8> data) noexcept {
            auto* self = static_cast<SpiFlashBinding*>(ctx);
            if (!self || !self->handle.ops || !self->handle.ops->read) {
                return Status{Errc::nosys};
            }
            if (data.empty()) return Status{Errc::inval};
            const util::u64 addr = lba * self->device.block_size;
            return self->handle.ops->read(self->handle.ctx, addr, data);
        }

        static Status write_impl(void* ctx, util::u64 lba, std::span<const util::u8> data) noexcept {
            auto* self = static_cast<SpiFlashBinding*>(ctx);
            if (!self || !self->handle.ops || !self->handle.ops->write) {
                return Status{Errc::nosys};
            }
            if (data.empty()) return Status{Errc::inval};
            const util::u64 addr = lba * self->device.block_size;
            return self->handle.ops->write(self->handle.ctx, addr, data);
        }

        static Status erase_impl(void* ctx, util::u64 lba, util::u64 count) noexcept {
            auto* self = static_cast<SpiFlashBinding*>(ctx);
            if (!self || !self->handle.ops || !self->handle.ops->erase) {
                return Status{Errc::nosys};
            }
            if (count == 0) return Status{Errc::inval};
            const util::u64 addr = lba * self->device.block_size;
            const util::u64 size = count * self->device.block_size;
            return self->handle.ops->erase(self->handle.ctx, addr, size);
        }

        static Status flush_impl(void* ctx) noexcept {
            auto* self = static_cast<SpiFlashBinding*>(ctx);
            if (!self || !self->handle.ops || !self->handle.ops->flush) {
                return Status{Errc::ok};
            }
            return self->handle.ops->flush(self->handle.ctx);
        }
    };
}
