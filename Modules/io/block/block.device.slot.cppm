module;

#include <span>

export module block.device.slot;

import block.device;
import util.core;
import util.error;

export namespace block {
    class DeviceSlot {
    public:
        DeviceSlot() noexcept {
            device_.ctx = this;
            device_.read = &DeviceSlot::read_trampoline;
            device_.write = &DeviceSlot::write_trampoline;
            device_.erase = &DeviceSlot::erase_trampoline;
            device_.flush = &DeviceSlot::flush_trampoline;
        }

        util::Result<void> attach(Device& target) noexcept {
            if (&target == &device_ || !target.read || target.block_size == 0 || target.block_count == 0) {
                return util::unexpected(util::Errc::invalid_arg);
            }
            target_ = &target;
            device_.block_size = target.block_size;
            device_.block_count = target.block_count;
            device_.caps = target.caps ? target.caps : caps_from_ops(target);
            ++generation_;
            return {};
        }

        void detach() noexcept {
            target_ = nullptr;
            ++generation_;
        }

        [[nodiscard]] bool attached() const noexcept { return target_ != nullptr; }
        [[nodiscard]] util::u32 generation() const noexcept { return generation_; }
        [[nodiscard]] Device* target() const noexcept { return target_; }

        Device& device() noexcept { return device_; }
        const Device& device() const noexcept { return device_; }

    private:
        static Status read_trampoline(void* ctx, util::u64 lba, std::span<util::u8> data) noexcept {
            auto* self = static_cast<DeviceSlot*>(ctx);
            if (!self || !self->target_ || !self->target_->read) {
                return Status{Errc::noent};
            }
            return self->target_->read(self->target_->ctx, lba, data);
        }

        static Status write_trampoline(void* ctx, util::u64 lba, std::span<const util::u8> data) noexcept {
            auto* self = static_cast<DeviceSlot*>(ctx);
            if (!self || !self->target_) {
                return Status{Errc::noent};
            }
            if (!self->target_->write) {
                return Status{Errc::nosys};
            }
            return self->target_->write(self->target_->ctx, lba, data);
        }

        static Status erase_trampoline(void* ctx, util::u64 lba, util::u64 count) noexcept {
            auto* self = static_cast<DeviceSlot*>(ctx);
            if (!self || !self->target_) {
                return Status{Errc::noent};
            }
            if (!self->target_->erase) {
                return Status{Errc::nosys};
            }
            return self->target_->erase(self->target_->ctx, lba, count);
        }

        static Status flush_trampoline(void* ctx) noexcept {
            auto* self = static_cast<DeviceSlot*>(ctx);
            if (!self || !self->target_) {
                return Status{Errc::noent};
            }
            if (!self->target_->flush) {
                return Status{Errc::ok};
            }
            return self->target_->flush(self->target_->ctx);
        }

        Device* target_{nullptr};
        util::u32 generation_{0};
        Device device_{};
    };
}
