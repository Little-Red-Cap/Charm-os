module;

#include <cstddef>

export module media.stream.source;

import util.core;
import util.span;
import media.stream.types;

export namespace media {
    enum class SeekWhence : util::u8 {
        set,
        cur,
        end
    };

    struct IStreamSource {
        virtual ~IStreamSource() = default;

        virtual Result<util::usize> read(util::span<std::byte> out) noexcept = 0;
        virtual Result<util::u64> seek(util::i64 offset, SeekWhence whence) noexcept = 0;
        virtual Result<util::u64> tell() noexcept = 0;
        virtual Result<util::u64> size() noexcept = 0;

        virtual Result<util::usize> read_at(util::u64, util::span<std::byte>) noexcept {
            return util::unexpected(Error{Errc::not_supported, 0});
        }
    };
}
