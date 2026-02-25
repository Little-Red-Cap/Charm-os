module;

#include <cstddef>

export module media.stream.sink;

import util.core;
import util.span;
import media.stream.types;

export namespace media {
    using FillCallback = util::usize (*)(util::span<std::byte> dst, void* user) noexcept;

    struct SinkConfig {
        StreamFormat format{};
        util::u32 period_frames{0};
    };

    struct IStreamSink {
        virtual ~IStreamSink() = default;

        virtual Result<void> open(const SinkConfig& cfg) noexcept = 0;
        virtual Result<void> start() noexcept = 0;
        virtual Result<void> stop() noexcept = 0;
        virtual void close() noexcept = 0;

        virtual void set_fill_callback(FillCallback cb, void* user) noexcept = 0;
        virtual StreamFormat format() const noexcept = 0;
    };
}
