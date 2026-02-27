module;

#include <span>
#include <cstddef>
#include <concepts>

export module media.stream.sink;

import util.core;
import media.stream.types;

export namespace media {
    using FillCallback = util::usize (*)(std::span<std::byte> dst, void* user) noexcept;

    struct SinkConfig {
        StreamFormat format{};
        util::u32 period_frames{0};
    };

    template <typename T>
    concept StreamSink = requires(T& t, const SinkConfig& cfg, FillCallback cb, void* user) {
        { t.open(cfg) } noexcept -> std::same_as<Result<void>>;
        { t.start() } noexcept -> std::same_as<Result<void>>;
        { t.stop() } noexcept -> std::same_as<Result<void>>;
        { t.close() } noexcept -> std::same_as<void>;
        { t.set_fill_callback(cb, user) } noexcept -> std::same_as<void>;
        { t.format() } noexcept -> std::same_as<StreamFormat>;
    };

    struct StreamSinkOps {
        using OpenFn = Result<void> (*)(void*, const SinkConfig&) noexcept;
        using StartFn = Result<void> (*)(void*) noexcept;
        using StopFn = Result<void> (*)(void*) noexcept;
        using CloseFn = void (*)(void*) noexcept;
        using SetCbFn = void (*)(void*, FillCallback, void*) noexcept;
        using FormatFn = StreamFormat (*)(void*) noexcept;

        OpenFn open{nullptr};
        StartFn start{nullptr};
        StopFn stop{nullptr};
        CloseFn close{nullptr};
        SetCbFn set_fill_callback{nullptr};
        FormatFn format{nullptr};
    };

    struct StreamSinkRef {
        void* self{nullptr};
        const StreamSinkOps* ops{nullptr};

        Result<void> open(const SinkConfig& cfg) noexcept { return ops->open(self, cfg); }
        Result<void> start() noexcept { return ops->start(self); }
        Result<void> stop() noexcept { return ops->stop(self); }
        void close() noexcept { ops->close(self); }
        void set_fill_callback(FillCallback cb, void* user) noexcept { ops->set_fill_callback(self, cb, user); }
        StreamFormat format() const noexcept { return ops->format(self); }
    };

    template <StreamSink T>
    inline const StreamSinkOps* stream_sink_ops() noexcept {
        static const StreamSinkOps ops{
            .open = [](void* self, const SinkConfig& cfg) noexcept {
                return static_cast<T*>(self)->open(cfg);
            },
            .start = [](void* self) noexcept {
                return static_cast<T*>(self)->start();
            },
            .stop = [](void* self) noexcept {
                return static_cast<T*>(self)->stop();
            },
            .close = [](void* self) noexcept {
                static_cast<T*>(self)->close();
            },
            .set_fill_callback = [](void* self, FillCallback cb, void* user) noexcept {
                static_cast<T*>(self)->set_fill_callback(cb, user);
            },
            .format = [](void* self) noexcept -> StreamFormat {
                return static_cast<T*>(self)->format();
            }
        };
        return &ops;
    }

    template <StreamSink T>
    inline StreamSinkRef make_stream_sink_ref(T& sink) noexcept {
        return StreamSinkRef{&sink, stream_sink_ops<T>()};
    }
}
