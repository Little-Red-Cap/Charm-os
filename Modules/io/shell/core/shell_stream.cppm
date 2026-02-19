module;

#include <span>
#include <string_view>

export module shell_stream;

import shell_core;
import shell_stdio;
import service_stream;
import util.core;

export namespace shell {
    template <service::Stream S>
    struct StreamConsole {
        S* stream{nullptr};

        static util::usize write_cb(void* ctx, Buffer buf) noexcept {
            auto* self = static_cast<StreamConsole*>(ctx);
            if (!self || !self->stream) return 0;
            auto bytes = std::span<const util::u8>(
                reinterpret_cast<const util::u8*>(buf.data), buf.size);
            auto st = self->stream->write(bytes);
            if (!st) return 0;
            return buf.size;
        }

        Console make() noexcept {
            return Console{this, &StreamConsole::write_cb};
        }
    };
}
