module;

#include <array>
#include <cstddef>
#include <cstdint>

export module at.transport_channel;

import util.core;
import io.channel;
import at.parser;
import at.session;

export namespace at {
    template <util::usize MaxQueue, util::usize LineCap, util::usize RxBufSize>
    class ChannelBridge {
    public:
        explicit ChannelBridge(Session<MaxQueue, LineCap>& session) noexcept
            : session_(&session) {
            session_->set_sender(&ChannelBridge::send_trampoline, this);
        }

        void set_channel(io::Channel* ch) noexcept {
            channel_ = ch;
        }

        void poll() noexcept {
            if (!channel_) return;
            auto r = channel_->read(MutByteView{rx_buf_.data(), rx_buf_.size()});
            if (!r || *r == 0) return;
            session_->feed(ByteView{rx_buf_.data(), *r});
        }

    private:
        static bool send_trampoline(void* ctx, ByteView data) noexcept {
            auto* self = static_cast<ChannelBridge*>(ctx);
            if (!self || !self->channel_) return false;
            auto r = self->channel_->write(data);
            return r.has_value();
        }

        Session<MaxQueue, LineCap>* session_{nullptr};
        std::array<util::u8, RxBufSize> rx_buf_{};
        io::Channel* channel_{nullptr};
    };
}
