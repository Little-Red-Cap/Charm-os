module;

#include <cstddef>
#include <cstdint>
#include <span>
#include <concepts>
#include <string_view>

export module service_stream;

import util.core;

export namespace service {
    enum class StreamResult : util::u8 { ok = 0, error, timeout, closed };

    struct StreamStatus {
        StreamResult result{StreamResult::ok};
        constexpr explicit operator bool() const noexcept { return result == StreamResult::ok; }
    };

    struct StreamSpan {
        std::span<util::u8> data;
        constexpr util::usize size() const noexcept { return data.size(); }
    };

    template <typename T>
    concept Stream = requires(T& s, std::span<util::u8> buf, std::span<const util::u8> in) {
        { s.read(buf) } -> std::same_as<StreamStatus>;
        { s.write(in) } -> std::same_as<StreamStatus>;
        { s.flush() } -> std::same_as<StreamStatus>;
    };

    template <Stream S>
    inline StreamStatus write(S& s, std::string_view sv) noexcept {
        auto bytes = std::span<const util::u8>(reinterpret_cast<const util::u8*>(sv.data()), sv.size());
        return s.write(bytes);
    }
}
