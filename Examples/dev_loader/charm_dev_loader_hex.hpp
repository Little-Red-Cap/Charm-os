#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

namespace charm::dev_loader {

enum class HexDecodeCode : std::uint8_t {
    ok,
    empty,
    invalid_char,
    odd_digits,
    output_too_small,
};

struct HexDecodeResult {
    HexDecodeCode code{HexDecodeCode::ok};
    std::uint32_t bytes_written{0};
    std::uint32_t digits_seen{0};
};

[[nodiscard]] constexpr std::string_view hex_decode_code_name(HexDecodeCode code) noexcept {
    using namespace std::literals::string_view_literals;
    switch (code) {
        case HexDecodeCode::ok:
            return "ok"sv;
        case HexDecodeCode::empty:
            return "empty"sv;
        case HexDecodeCode::invalid_char:
            return "invalid_char"sv;
        case HexDecodeCode::odd_digits:
            return "odd_digits"sv;
        case HexDecodeCode::output_too_small:
            return "output_too_small"sv;
    }
    return "unknown"sv;
}

[[nodiscard]] constexpr bool hex_is_space(char ch) noexcept {
    return ch == ' ' || ch == '\t';
}

[[nodiscard]] constexpr int hex_digit_value(char ch) noexcept {
    if (ch >= '0' && ch <= '9') {
        return ch - '0';
    }
    if (ch >= 'a' && ch <= 'f') {
        return ch - 'a' + 10;
    }
    if (ch >= 'A' && ch <= 'F') {
        return ch - 'A' + 10;
    }
    return -1;
}

[[nodiscard]] inline HexDecodeResult hex_decode_bytes(std::string_view input,
                                                      std::span<std::byte> output) noexcept {
    if (output.data() == nullptr && !output.empty()) {
        return {.code = HexDecodeCode::output_too_small};
    }

    std::uint32_t bytes_written = 0;
    std::uint32_t digits_seen = 0;
    int high_nibble = -1;

    for (const char ch : input) {
        if (hex_is_space(ch)) {
            continue;
        }
        const int digit = hex_digit_value(ch);
        if (digit < 0) {
            return HexDecodeResult{
                .code = HexDecodeCode::invalid_char,
                .bytes_written = bytes_written,
                .digits_seen = digits_seen,
            };
        }
        ++digits_seen;
        if (high_nibble < 0) {
            high_nibble = digit;
            continue;
        }
        if (bytes_written >= output.size()) {
            return HexDecodeResult{
                .code = HexDecodeCode::output_too_small,
                .bytes_written = bytes_written,
                .digits_seen = digits_seen,
            };
        }
        output[bytes_written++] = static_cast<std::byte>((high_nibble << 4) | digit);
        high_nibble = -1;
    }

    if (digits_seen == 0U) {
        return {.code = HexDecodeCode::empty};
    }
    if (high_nibble >= 0) {
        return HexDecodeResult{
            .code = HexDecodeCode::odd_digits,
            .bytes_written = bytes_written,
            .digits_seen = digits_seen,
        };
    }

    return HexDecodeResult{
        .code = HexDecodeCode::ok,
        .bytes_written = bytes_written,
        .digits_seen = digits_seen,
    };
}

} // namespace charm::dev_loader
