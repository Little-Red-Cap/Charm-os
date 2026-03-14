module;

#include <cstdio>
#include <cstddef>
#include <cstdint>

export module example.pc.console;

import io.channel;
import io.registry;
import out.channel;
import util.core;
import util.error;

namespace example::pc::detail {
    struct StdioCtx {
        std::FILE* file{};
    };

    io::result stdio_read(void*, io::MutByteView) noexcept {
        return io::fail(io::errc::not_supported);
    }

    io::result stdio_write(void* ctx, io::ByteView buf) noexcept {
        if (!ctx) return io::fail(io::errc::invalid_arg);
        auto* self = static_cast<StdioCtx*>(ctx);
        if (!self->file) return io::fail(io::errc::invalid_arg);
        if (buf.empty()) return io::ok(0);
        const auto n = std::fwrite(buf.data(), 1, buf.size(), self->file);
        if (n == 0) return io::fail(io::errc::io_error);
        return io::ok(static_cast<util::usize>(n));
    }

    io::result stdio_flush(void* ctx) noexcept {
        if (!ctx) return io::fail(io::errc::invalid_arg);
        auto* self = static_cast<StdioCtx*>(ctx);
        if (!self->file) return io::fail(io::errc::invalid_arg);
        if (std::fflush(self->file) != 0) return io::fail(io::errc::io_error);
        return io::ok(0);
    }
}

export namespace example::pc {
    inline io::Channel& stdout_channel() noexcept {
        static detail::StdioCtx ctx{stdout};
        static io::Channel ch{
            &ctx,
            io::ChannelOps{&detail::stdio_read, &detail::stdio_write, &detail::stdio_flush}
        };
        return ch;
    }

    inline io::Channel& stderr_channel() noexcept {
        static detail::StdioCtx ctx{stderr};
        static io::Channel ch{
            &ctx,
            io::ChannelOps{&detail::stdio_read, &detail::stdio_write, &detail::stdio_flush}
        };
        return ch;
    }

    inline out::channel_sink stdout_sink() noexcept {
        return out::make_channel_sink(stdout_channel());
    }

    inline out::channel_sink stderr_sink() noexcept {
        return out::make_channel_sink(stderr_channel());
    }

    template <typename RegistryT>
    inline util::Result<void> register_stdout(RegistryT& registry) noexcept {
        io::EndpointDesc desc{
            "io.console0",
            io::cap_id("io.console0"),
            io::EndpointKind::channel,
            io::EndpointCaps::writable
        };
        return registry.register_channel(desc, stdout_channel());
    }
}
