module;

#include <cstddef>
#include <cstdint>
#include <span>
#include <concepts>
#include <string_view>
#include <array>

export module service_stream;

import util.core;
import service_ring_buffer;

export namespace service {
    enum class StreamResult : util::u8 { ok = 0, error, timeout, closed, would_block, notsup };

    struct StreamStatus {
        StreamResult result{StreamResult::ok};
        constexpr explicit operator bool() const noexcept { return result == StreamResult::ok; }
    };

    enum class StreamMode : util::u8 { blocking = 0, nonblocking };

    struct StreamTimeout {
        util::u32 ms{0};
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

    template <typename T>
    concept StreamTimed = requires(T& s, std::span<util::u8> buf, std::span<const util::u8> in, util::u32 ms) {
        { s.read_timeout(buf, ms) } -> std::same_as<StreamStatus>;
        { s.write_timeout(in, ms) } -> std::same_as<StreamStatus>;
    };

    template <typename T>
    concept StreamModeControl = requires(T& s, StreamMode mode) {
        { s.set_mode(mode) } -> std::same_as<StreamStatus>;
    };

    template <Stream S>
    inline StreamStatus write(S& s, std::string_view sv) noexcept {
        auto bytes = std::span<const util::u8>(reinterpret_cast<const util::u8*>(sv.data()), sv.size());
        return s.write(bytes);
    }

    template <Stream S>
    inline StreamStatus read_timeout(S& s, std::span<util::u8> buf, StreamTimeout to) noexcept {
        if constexpr (StreamTimed<S>) {
            return s.read_timeout(buf, to.ms);
        } else {
            (void)to;
            return s.read(buf);
        }
    }

    template <Stream S>
    inline StreamStatus write_timeout(S& s, std::span<const util::u8> buf, StreamTimeout to) noexcept {
        if constexpr (StreamTimed<S>) {
            return s.write_timeout(buf, to.ms);
        } else {
            (void)to;
            return s.write(buf);
        }
    }

    template <Stream S>
    inline StreamStatus set_mode(S& s, StreamMode mode) noexcept {
        if constexpr (StreamModeControl<S>) {
            return s.set_mode(mode);
        } else {
            (void)s;
            (void)mode;
            return StreamStatus{StreamResult::notsup};
        }
    }

    template <Stream S, util::usize Capacity>
    class BufferedStream {
    public:
        explicit BufferedStream(S& s) : stream_(&s) {}

        StreamStatus read(std::span<util::u8> buf) noexcept {
            if (!stream_) return StreamStatus{StreamResult::error};
            return stream_->read(buf);
        }

        StreamStatus write(std::span<const util::u8> in) noexcept {
            if (!stream_) return StreamStatus{StreamResult::error};
            for (util::usize i = 0; i < in.size(); ++i) {
                if (!buffer_.push(in[i])) {
                    auto st = flush();
                    if (!st) return st;
                    if (!buffer_.push(in[i])) {
                        return StreamStatus{StreamResult::error};
                    }
                }
            }
            return StreamStatus{StreamResult::ok};
        }

        StreamStatus flush() noexcept {
            if (!stream_) return StreamStatus{StreamResult::error};
            std::array<util::u8, Capacity> temp{};
            while (!buffer_.empty()) {
                util::usize n = 0;
                while (n < Capacity && !buffer_.empty()) {
                    util::u8 b{};
                    (void)buffer_.pop(b);
                    temp[n++] = b;
                }
                auto st = stream_->write(std::span<const util::u8>(temp.data(), n));
                if (!st) return st;
            }
            return stream_->flush();
        }

    private:
        S* stream_{nullptr};
        RingBuffer<util::u8, Capacity> buffer_{};
    };
}
