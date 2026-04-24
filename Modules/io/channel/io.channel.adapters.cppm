module;

#include <cstddef>
#include <cstdint>

export module io.channel.adapters;

import io.channel;

export namespace io {
    // Platform-side adapter template: bind platform RX/TX callbacks once,
    // then expose a unified io::Channel for higher-level protocols.
    template <class Ctx>
    struct ChannelAdapter {
        Ctx* ctx{};
        ReadFn read{};
        WriteFn write{};
        FlushFn flush{};

        Channel channel() noexcept {
            return Channel{
                this,
                ChannelOps{
                    &ChannelAdapter::read_trampoline,
                    &ChannelAdapter::write_trampoline,
                    &ChannelAdapter::flush_trampoline
                }
            };
        }

    private:
        static result read_trampoline(void* self, MutByteView buf) noexcept {
            auto* adapter = static_cast<ChannelAdapter*>(self);
            if (!adapter || !adapter->read) return fail(errc::invalid);
            return adapter->read(adapter->ctx, buf);
        }

        static result write_trampoline(void* self, ByteView buf) noexcept {
            auto* adapter = static_cast<ChannelAdapter*>(self);
            if (!adapter || !adapter->write) return fail(errc::invalid);
            return adapter->write(adapter->ctx, buf);
        }

        static result flush_trampoline(void* self) noexcept {
            auto* adapter = static_cast<ChannelAdapter*>(self);
            if (!adapter || !adapter->flush) return ok(0);
            return adapter->flush(adapter->ctx);
        }
    };

    template <class Ctx>
    using UartChannel = ChannelAdapter<Ctx>;

    template <class Ctx>
    using CdcChannel = ChannelAdapter<Ctx>;

    template <class Ctx>
    using TcpChannel = ChannelAdapter<Ctx>;
}
