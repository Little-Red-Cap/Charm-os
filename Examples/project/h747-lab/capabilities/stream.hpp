#pragma once

#include "capabilities/status.hpp"

#include <concepts>
#include <cstddef>
#include <optional>
#include <span>
#include <string_view>

namespace charm::cap {

template <class T>
concept ByteSink = requires(T& sink, std::span<const std::byte> bytes) {
    { sink.write(bytes) } -> std::same_as<Transfer>;
    { sink.flush() } -> std::same_as<Status>;
};

template <class T>
concept TextSink = ByteSink<T> && requires(T& sink, std::string_view text) {
    { sink.write(text) } -> std::same_as<Transfer>;
};

template <class T>
concept LineSource = requires(T& source) {
    { source.poll_line() } -> std::same_as<std::optional<std::string_view>>;
};

template <ByteSink Sink>
[[nodiscard]] inline Transfer write_bytes(Sink& sink, const std::span<const std::byte> bytes) noexcept {
    return sink.write(bytes);
}

template <TextSink Sink>
[[nodiscard]] inline Transfer write_text(Sink& sink, const std::string_view text) noexcept {
    return sink.write(text);
}

} // namespace charm::cap
