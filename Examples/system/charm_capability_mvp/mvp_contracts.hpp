#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

namespace charm::mvp {
    enum class StatusCode : std::uint8_t {
        ok = 0,
        invalid_argument,
        unavailable,
        io_error,
        out_of_range,
    };

    struct Status {
        StatusCode code{StatusCode::ok};

        [[nodiscard]] constexpr bool is_ok() const noexcept {
            return code == StatusCode::ok;
        }
    };

    struct Transfer {
        Status status{};
        std::size_t bytes{0};

        [[nodiscard]] constexpr bool is_ok() const noexcept {
            return status.is_ok();
        }
    };

    struct ClockSample {
        Status status{};
        std::uint64_t milliseconds{0};

        [[nodiscard]] constexpr bool is_ok() const noexcept {
            return status.is_ok();
        }
    };

    struct TextSink {
        using WriteFn = Transfer (*)(void*, std::string_view) noexcept;
        using FlushFn = Status (*)(void*) noexcept;

        void* context{nullptr};
        WriteFn write_fn{nullptr};
        FlushFn flush_fn{nullptr};

        [[nodiscard]] constexpr bool valid() const noexcept {
            return write_fn != nullptr && flush_fn != nullptr;
        }

        [[nodiscard]] Transfer write(std::string_view text) const noexcept {
            return valid() ? write_fn(context, text)
                           : Transfer{Status{StatusCode::unavailable}, 0};
        }

        [[nodiscard]] Status flush() const noexcept {
            return valid() ? flush_fn(context) : Status{StatusCode::unavailable};
        }
    };

    struct Clock {
        using NowFn = ClockSample (*)(void*) noexcept;

        void* context{nullptr};
        NowFn now_fn{nullptr};

        [[nodiscard]] constexpr bool valid() const noexcept {
            return now_fn != nullptr;
        }

        [[nodiscard]] ClockSample now_ms() const noexcept {
            return valid() ? now_fn(context)
                           : ClockSample{Status{StatusCode::unavailable}, 0};
        }
    };

    struct BlockDevice {
        using ReadFn = Status (*)(void*, std::uint64_t, std::span<std::byte>) noexcept;
        using WriteFn = Status (*)(void*, std::uint64_t, std::span<const std::byte>) noexcept;
        using FlushFn = Status (*)(void*) noexcept;
        using GeometryFn = std::uint64_t (*)(void*) noexcept;

        void* context{nullptr};
        ReadFn read_fn{nullptr};
        WriteFn write_fn{nullptr};
        FlushFn flush_fn{nullptr};
        GeometryFn block_size_fn{nullptr};
        GeometryFn block_count_fn{nullptr};

        [[nodiscard]] constexpr bool valid() const noexcept {
            return read_fn != nullptr && write_fn != nullptr && flush_fn != nullptr &&
                   block_size_fn != nullptr && block_count_fn != nullptr;
        }

        [[nodiscard]] Status read(std::uint64_t lba, std::span<std::byte> out) const noexcept {
            return valid() ? read_fn(context, lba, out) : Status{StatusCode::unavailable};
        }

        [[nodiscard]] Status write(std::uint64_t lba,
                                   std::span<const std::byte> in) const noexcept {
            return valid() ? write_fn(context, lba, in) : Status{StatusCode::unavailable};
        }

        [[nodiscard]] Status flush() const noexcept {
            return valid() ? flush_fn(context) : Status{StatusCode::unavailable};
        }

        [[nodiscard]] std::uint64_t block_size() const noexcept {
            return valid() ? block_size_fn(context) : 0;
        }

        [[nodiscard]] std::uint64_t block_count() const noexcept {
            return valid() ? block_count_fn(context) : 0;
        }
    };
}
