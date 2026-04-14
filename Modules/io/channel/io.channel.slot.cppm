export module io.channel.slot;

import io.channel;
import util.core;
import util.error;

export namespace io {
    class ChannelSlot {
    public:
        ChannelSlot() noexcept
            : channel_{this,
                       ChannelOps{
                           &ChannelSlot::read_trampoline,
                           &ChannelSlot::write_trampoline,
                           &ChannelSlot::flush_trampoline}} {
        }

        util::Result<void> attach(Channel& target) noexcept {
            if (&target == &channel_) {
                return util::unexpected(util::Errc::invalid_arg);
            }
            target_ = &target;
            ++generation_;
            return {};
        }

        void detach() noexcept {
            target_ = nullptr;
            ++generation_;
        }

        [[nodiscard]] bool attached() const noexcept { return target_ != nullptr; }
        [[nodiscard]] util::u32 generation() const noexcept { return generation_; }
        [[nodiscard]] Channel* target() const noexcept { return target_; }

        Channel& channel() noexcept { return channel_; }
        const Channel& channel() const noexcept { return channel_; }

    private:
        static result read_trampoline(void* ctx, MutByteView buf) noexcept {
            auto* self = static_cast<ChannelSlot*>(ctx);
            if (!self) {
                return fail(errc::invalid_arg);
            }
            if (!self->target_) {
                return fail(errc::noent);
            }
            return self->target_->read(buf);
        }

        static result write_trampoline(void* ctx, ByteView buf) noexcept {
            auto* self = static_cast<ChannelSlot*>(ctx);
            if (!self) {
                return fail(errc::invalid_arg);
            }
            if (!self->target_) {
                return fail(errc::noent);
            }
            return self->target_->write(buf);
        }

        static result flush_trampoline(void* ctx) noexcept {
            auto* self = static_cast<ChannelSlot*>(ctx);
            if (!self) {
                return fail(errc::invalid_arg);
            }
            if (!self->target_) {
                return fail(errc::noent);
            }
            return self->target_->flush();
        }

        Channel* target_{nullptr};
        util::u32 generation_{0};
        Channel channel_{};
    };
}
