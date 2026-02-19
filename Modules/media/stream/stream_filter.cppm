module;

#include <cstddef>

export module media.stream.filter;

import util.core;
import util.span;
import media.stream.types;

export namespace media {
    struct FilterResult {
        util::usize consumed{0};
        util::usize produced{0};
        bool end_of_stream{false};
    };

    struct IStreamFilter {
        virtual ~IStreamFilter() = default;

        virtual Result<void> reset() noexcept = 0;
        virtual Result<FilterResult> process(util::span<const std::byte> in,
                                             util::span<std::byte> out) noexcept = 0;

        virtual Result<util::usize> flush(util::span<std::byte> out) noexcept {
            (void)out;
            return util::unexpected(Error{Errc::not_supported, 0});
        }

        virtual StreamFormat format() const noexcept { return {}; }
    };
}
