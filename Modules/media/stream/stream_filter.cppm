module;

#include <span>
#include <cstddef>
#include <concepts>

export module media.stream.filter;

import util.core;
import util.expected;
import media.stream.types;

export namespace media {
    struct FilterResult {
        util::usize consumed{0};
        util::usize produced{0};
        bool end_of_stream{false};
    };

    template <typename T>
    concept StreamFilter = requires(T& t, std::span<const std::byte> in, std::span<std::byte> out) {
        { t.reset() } noexcept -> std::same_as<Result<void>>;
        { t.process(in, out) } noexcept -> std::same_as<Result<FilterResult>>;
    };

    struct StreamFilterOps {
        using ResetFn = Result<void> (*)(void*) noexcept;
        using ProcessFn = Result<FilterResult> (*)(void*, std::span<const std::byte>, std::span<std::byte>) noexcept;
        using FlushFn = Result<util::usize> (*)(void*, std::span<std::byte>) noexcept;
        using FormatFn = StreamFormat (*)(void*) noexcept;

        ResetFn reset{nullptr};
        ProcessFn process{nullptr};
        FlushFn flush{nullptr};
        FormatFn format{nullptr};
    };

    struct StreamFilterRef {
        void* self{nullptr};
        const StreamFilterOps* ops{nullptr};

        Result<void> reset() noexcept { return ops->reset(self); }

        Result<FilterResult> process(std::span<const std::byte> in,
                                     std::span<std::byte> out) noexcept {
            return ops->process(self, in, out);
        }

        Result<util::usize> flush(std::span<std::byte> out) noexcept {
            if (!ops->flush) return util::unexpected(Errc::not_supported);
            return ops->flush(self, out);
        }

        StreamFormat format() const noexcept {
            if (!ops->format) return {};
            return ops->format(self);
        }
    };

    template <StreamFilter T>
    inline const StreamFilterOps* stream_filter_ops() noexcept {
        static const StreamFilterOps ops{
            .reset = [](void* self) noexcept {
                return static_cast<T*>(self)->reset();
            },
            .process = [](void* self, std::span<const std::byte> in, std::span<std::byte> out) noexcept {
                return static_cast<T*>(self)->process(in, out);
            },
            .flush = [](void* self, std::span<std::byte> out) noexcept -> Result<util::usize> {
                if constexpr (requires(T& t) { t.flush(out); }) {
                    return static_cast<T*>(self)->flush(out);
                } else {
                    return util::unexpected(Errc::not_supported);
                }
            },
            .format = [](void* self) noexcept -> StreamFormat {
                if constexpr (requires(T& t) { t.format(); }) {
                    return static_cast<T*>(self)->format();
                } else {
                    return {};
                }
            }
        };
        return &ops;
    }

    template <StreamFilter T>
    inline StreamFilterRef make_stream_filter_ref(T& filter) noexcept {
        return StreamFilterRef{&filter, stream_filter_ops<T>()};
    }
}
