module;

#include <span>
#include <cstddef>
#include <concepts>

export module media.stream.source;

import util.core;
import util.expected;
import media.stream.types;

export namespace media {
    enum class SeekWhence : util::u8 {
        set,
        cur,
        end
    };

    template <typename T>
    concept StreamSource = requires(T& t, std::span<std::byte> out, util::i64 offset, SeekWhence whence) {
        { t.read(out) } noexcept -> std::same_as<Result<util::usize>>;
        { t.seek(offset, whence) } noexcept -> std::same_as<Result<util::i64>>;
        { t.tell() } noexcept -> std::same_as<Result<util::i64>>;
        { t.size() } noexcept -> std::same_as<Result<util::i64>>;
    };

    struct StreamSourceOps {
        using ReadFn = Result<util::usize> (*)(void*, std::span<std::byte>) noexcept;
        using SeekFn = Result<util::i64> (*)(void*, util::i64, SeekWhence) noexcept;
        using TellFn = Result<util::i64> (*)(void*) noexcept;
        using SizeFn = Result<util::i64> (*)(void*) noexcept;
        using ReadAtFn = Result<util::usize> (*)(void*, util::i64, std::span<std::byte>) noexcept;

        ReadFn read{nullptr};
        SeekFn seek{nullptr};
        TellFn tell{nullptr};
        SizeFn size{nullptr};
        ReadAtFn read_at{nullptr};
    };

    struct StreamSourceRef {
        void* self{nullptr};
        const StreamSourceOps* ops{nullptr};

        Result<util::usize> read(std::span<std::byte> out) noexcept {
            return ops->read(self, out);
        }

        Result<util::i64> seek(util::i64 offset, SeekWhence whence) noexcept {
            return ops->seek(self, offset, whence);
        }

        Result<util::i64> tell() noexcept {
            return ops->tell(self);
        }

        Result<util::i64> size() noexcept {
            return ops->size(self);
        }

        Result<util::usize> read_at(util::i64 offset, std::span<std::byte> out) noexcept {
            if (!ops->read_at) return util::unexpected(Errc::not_supported);
            return ops->read_at(self, offset, out);
        }
    };

    template <StreamSource T>
    inline const StreamSourceOps* stream_source_ops() noexcept {
        static const StreamSourceOps ops{
            .read = [](void* self, std::span<std::byte> out) noexcept {
                return static_cast<T*>(self)->read(out);
            },
            .seek = [](void* self, util::i64 offset, SeekWhence whence) noexcept {
                return static_cast<T*>(self)->seek(offset, whence);
            },
            .tell = [](void* self) noexcept {
                return static_cast<T*>(self)->tell();
            },
            .size = [](void* self) noexcept {
                return static_cast<T*>(self)->size();
            },
            .read_at = [] (void* self, util::i64 offset, std::span<std::byte> out) noexcept -> Result<util::usize> {
                if constexpr (requires(T& t) { t.read_at(offset, out); }) {
                    return static_cast<T*>(self)->read_at(offset, out);
                } else {
                    return util::unexpected(Errc::not_supported);
                }
            }
        };
        return &ops;
    }

    template <StreamSource T>
    inline StreamSourceRef make_stream_source_ref(T& src) noexcept {
        return StreamSourceRef{&src, stream_source_ops<T>()};
    }
}
