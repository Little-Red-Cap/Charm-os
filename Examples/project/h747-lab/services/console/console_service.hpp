#pragma once

#include "console.h"
#include "capabilities/stream.hpp"
#include "capabilities/time.hpp"
#include "port.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

namespace h747::console {

class ConsoleStream {
public:
    [[nodiscard]] charm::cap::Transfer write(const std::span<const std::byte> bytes) const noexcept {
        for (const auto byte : bytes) {
            write_char(static_cast<char>(std::to_integer<std::uint8_t>(byte)));
        }
        return charm::cap::transferred(bytes.size());
    }

    [[nodiscard]] charm::cap::Transfer write(const std::string_view text) const noexcept {
        for (const char ch : text) {
            write_char(ch);
        }
        return charm::cap::transferred(text.size());
    }

    [[nodiscard]] charm::cap::Status flush() const noexcept {
        return charm::cap::Status::ok();
    }
};

inline ConsoleStream stream() noexcept {
    return {};
}

class Clock {
public:
    [[nodiscard]] charm::cap::Milliseconds tick_ms() const noexcept {
        return {h747::port::tick_ms()};
    }

    void delay(const charm::cap::Milliseconds duration) const noexcept {
        h747::port::delay_ms(duration.value);
    }
};

class Writer {
public:
    void write(const std::string_view text) const noexcept {
        for (const char ch : text) {
            write_char(ch);
        }
    }

    void write(const std::span<const std::byte> bytes) const noexcept {
        for (const auto byte : bytes) {
            write_char(static_cast<char>(std::to_integer<std::uint8_t>(byte)));
        }
    }

    void line(const std::string_view text) const noexcept {
        write(text);
        write_char('\n');
    }
};

inline Writer writer() noexcept {
    return {};
}

class ConsoleLineSource {
public:
    [[nodiscard]] std::optional<std::string_view> poll_line() noexcept {
        if (h747::console::poll_line(buffer_.data(), static_cast<std::uint32_t>(buffer_.size()), length_)) {
            return std::string_view{buffer_.data()};
        }
        return std::nullopt;
    }

private:
    std::array<char, 128> buffer_{};
    std::uint32_t length_{0U};
};

template <std::size_t Capacity>
class LineBuffer {
public:
    std::optional<std::string_view> poll() noexcept {
        if (poll_line(buffer_.data(), static_cast<std::uint32_t>(buffer_.size()), length_)) {
            return std::string_view{buffer_.data()};
        }
        return std::nullopt;
    }

private:
    std::array<char, Capacity> buffer_{};
    std::uint32_t length_{0U};
};

static_assert(charm::cap::TextSink<ConsoleStream>);
static_assert(charm::cap::LineSource<ConsoleLineSource>);

} // namespace h747::console
