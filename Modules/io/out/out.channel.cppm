module;

#include <cstddef>
#include <cstdint>

export module out.channel;

import io.channel;
import out.core;
import out.sink;
import util.expected;

export namespace out {

    struct channel_sink {
        io::Channel* channel{};

        result<std::size_t> write(bytes b) noexcept {
            if (!channel) return util::unexpected(errc::io_error);
            using io_byte = io::ByteView::element_type;
            auto view = io::ByteView{
                reinterpret_cast<const io_byte*>(b.data()),
                b.size()
            };
            auto r = channel->write(view);
            if (!r) return util::unexpected(r.error());
            return ok(r.value());
        }

        result<std::size_t> flush() noexcept {
            if (!channel) return util::unexpected(errc::io_error);
            auto r = channel->flush();
            if (!r) return util::unexpected(r.error());
            return ok(r.value());
        }
    };

    inline channel_sink make_channel_sink(io::Channel& ch) noexcept {
        return channel_sink{&ch};
    }
}
