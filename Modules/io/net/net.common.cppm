module;

#include <array>
#include <concepts>
#include <cstddef>
#include <cstdint>

export module net.common;

import util.core;
import util.error;
import util.expected;

export namespace net {
    struct ByteView {
        const util::u8* ptr{nullptr};
        util::usize len{0};

        [[nodiscard]] constexpr const util::u8* data() const noexcept {
            return ptr;
        }

        [[nodiscard]] constexpr util::usize size() const noexcept {
            return len;
        }

        [[nodiscard]] constexpr bool empty() const noexcept {
            return len == 0;
        }

        [[nodiscard]] constexpr const util::u8& operator[](util::usize index) const noexcept {
            return ptr[index];
        }

        [[nodiscard]] constexpr const util::u8* begin() const noexcept {
            return ptr;
        }

        [[nodiscard]] constexpr const util::u8* end() const noexcept {
            return ptr + len;
        }

        [[nodiscard]] constexpr ByteView subspan(util::usize offset,
                                                 util::usize count = static_cast<util::usize>(-1)) const noexcept {
            if (offset >= len) {
                return {};
            }
            const auto remaining = len - offset;
            if (count > remaining) {
                count = remaining;
            }
            return ByteView{ptr + offset, count};
        }
    };

    struct MutByteView {
        util::u8* ptr{nullptr};
        util::usize len{0};

        [[nodiscard]] constexpr util::u8* data() const noexcept {
            return ptr;
        }

        [[nodiscard]] constexpr util::usize size() const noexcept {
            return len;
        }

        [[nodiscard]] constexpr bool empty() const noexcept {
            return len == 0;
        }

        [[nodiscard]] constexpr util::u8& operator[](util::usize index) const noexcept {
            return ptr[index];
        }

        [[nodiscard]] constexpr util::u8* begin() const noexcept {
            return ptr;
        }

        [[nodiscard]] constexpr util::u8* end() const noexcept {
            return ptr + len;
        }

        [[nodiscard]] constexpr MutByteView subspan(util::usize offset,
                                                    util::usize count = static_cast<util::usize>(-1)) const noexcept {
            if (offset >= len) {
                return {};
            }
            const auto remaining = len - offset;
            if (count > remaining) {
                count = remaining;
            }
            return MutByteView{ptr + offset, count};
        }

        [[nodiscard]] constexpr operator ByteView() const noexcept {
            return ByteView{ptr, len};
        }
    };

    using errc = util::Errc;

    template <class T>
    using Result = util::Result<T>;

    using IoResult = Result<util::usize>;
    using EventMask = util::u32;
    using StreamSendFn = IoResult (*)(void* ctx, ByteView data) noexcept;

    constexpr IoResult ok(util::usize n) noexcept {
        return IoResult{std::in_place, n};
    }

    constexpr IoResult fail(errc e) noexcept {
        return util::unexpected(e);
    }

    class StreamSenderRef {
    public:
        constexpr StreamSenderRef() noexcept = default;

        static constexpr StreamSenderRef raw(StreamSendFn send, void* ctx) noexcept {
            return StreamSenderRef{send, ctx};
        }

        template <typename Sender>
            requires(
                requires(Sender& value, ByteView data) {
                    { value.send(data) } noexcept -> std::same_as<IoResult>;
                } ||
                requires(Sender& value, ByteView data) {
                    { value(data) } noexcept -> std::same_as<IoResult>;
                })
        static constexpr StreamSenderRef bind(Sender& sender) noexcept {
            return StreamSenderRef{&invoke<Sender>, &sender};
        }

        [[nodiscard]] constexpr explicit operator bool() const noexcept {
            return send_ != nullptr;
        }

        [[nodiscard]] IoResult send(ByteView data) const noexcept {
            if (!send_) {
                return util::unexpected(errc::bad_state);
            }
            return send_(ctx_, data);
        }

    private:
        constexpr StreamSenderRef(StreamSendFn send, void* ctx) noexcept
            : send_(send),
              ctx_(ctx) {
        }

        template <typename Sender>
        static IoResult invoke(void* ctx, ByteView data) noexcept {
            auto* sender = static_cast<Sender*>(ctx);
            if (!sender) {
                return util::unexpected(errc::bad_state);
            }
            if constexpr (requires(Sender& value, ByteView bytes) {
                              { value.send(bytes) } noexcept -> std::same_as<IoResult>;
                          }) {
                return sender->send(data);
            } else {
                return (*sender)(data);
            }
        }

        StreamSendFn send_{nullptr};
        void* ctx_{nullptr};
    };

    enum class AddressFamily : util::u8 {
        unspecified = 0,
        ipv4 = 4,
        ipv6 = 6,
    };

    enum class SocketKind : util::u8 {
        tcp,
        udp,
    };

    enum class SocketState : util::u8 {
        closed,
        opened,
        bound,
        listening,
        connected,
    };

    enum class ShutdownMode : util::u8 {
        read,
        write,
        both,
    };

    enum class NetEvent : util::u32 {
        readable = 1u << 0,
        writable = 1u << 1,
        accepted = 1u << 2,
        closed = 1u << 3,
        error = 1u << 4,
    };

    constexpr EventMask event_mask(NetEvent e) noexcept {
        return static_cast<EventMask>(e);
    }

    constexpr EventMask operator|(NetEvent a, NetEvent b) noexcept {
        return event_mask(a) | event_mask(b);
    }

    constexpr bool has_event(EventMask mask, NetEvent e) noexcept {
        return (mask & event_mask(e)) != 0u;
    }

    struct IpAddress {
        AddressFamily family{AddressFamily::unspecified};
        std::array<util::u8, 16> bytes{};

        [[nodiscard]] constexpr bool is_unspecified() const noexcept {
            return family == AddressFamily::unspecified;
        }

        [[nodiscard]] constexpr bool is_ipv4() const noexcept {
            return family == AddressFamily::ipv4;
        }

        [[nodiscard]] constexpr bool is_ipv6() const noexcept {
            return family == AddressFamily::ipv6;
        }

        [[nodiscard]] constexpr bool is_any() const noexcept {
            if (family == AddressFamily::unspecified) {
                return true;
            }
            const util::usize limit = family == AddressFamily::ipv4 ? 4u : bytes.size();
            for (util::usize i = 0; i < limit; ++i) {
                if (bytes[i] != 0u) {
                    return false;
                }
            }
            return true;
        }

        [[nodiscard]] static constexpr IpAddress ipv4(util::u8 a,
                                                      util::u8 b,
                                                      util::u8 c,
                                                      util::u8 d) noexcept {
            IpAddress ip{};
            ip.family = AddressFamily::ipv4;
            ip.bytes[0] = a;
            ip.bytes[1] = b;
            ip.bytes[2] = c;
            ip.bytes[3] = d;
            return ip;
        }

        [[nodiscard]] static constexpr IpAddress ipv4_any() noexcept {
            return ipv4(0, 0, 0, 0);
        }

        [[nodiscard]] static constexpr IpAddress ipv4_loopback() noexcept {
            return ipv4(127, 0, 0, 1);
        }

        [[nodiscard]] static constexpr IpAddress ipv4_broadcast() noexcept {
            return ipv4(255, 255, 255, 255);
        }

        [[nodiscard]] constexpr bool is_ipv4_limited_broadcast() const noexcept {
            return is_ipv4()
                && bytes[0] == 255u
                && bytes[1] == 255u
                && bytes[2] == 255u
                && bytes[3] == 255u;
        }
    };

    struct Endpoint {
        IpAddress address{};
        util::u16 port{0};

        [[nodiscard]] constexpr AddressFamily family() const noexcept {
            return address.family;
        }

        [[nodiscard]] constexpr bool is_any() const noexcept {
            return address.is_any() && port == 0;
        }

        [[nodiscard]] static constexpr Endpoint ipv4(util::u8 a,
                                                     util::u8 b,
                                                     util::u8 c,
                                                     util::u8 d,
                                                     util::u16 port) noexcept {
            return Endpoint{IpAddress::ipv4(a, b, c, d), port};
        }

        [[nodiscard]] static constexpr Endpoint ipv4_any(util::u16 port) noexcept {
            return Endpoint{IpAddress::ipv4_any(), port};
        }

        [[nodiscard]] static constexpr Endpoint ipv4_loopback(util::u16 port) noexcept {
            return Endpoint{IpAddress::ipv4_loopback(), port};
        }

        [[nodiscard]] static constexpr Endpoint ipv4_broadcast(util::u16 port) noexcept {
            return Endpoint{IpAddress::ipv4_broadcast(), port};
        }
    };

    [[nodiscard]] constexpr Result<void> validate_supported_family_v0(AddressFamily family) noexcept {
        if (family == AddressFamily::unspecified) {
            return util::unexpected(errc::invalid_arg);
        }
        if (family != AddressFamily::ipv4) {
            return util::unexpected(errc::not_supported);
        }
        return {};
    }

    [[nodiscard]] constexpr Result<void> validate_bind_endpoint_v0(const Endpoint& ep) noexcept {
        return validate_supported_family_v0(ep.family());
    }

    [[nodiscard]] constexpr Result<void> validate_remote_endpoint_v0(const Endpoint& ep) noexcept {
        auto supported = validate_supported_family_v0(ep.family());
        if (!supported) {
            return util::unexpected(supported.error());
        }
        if (ep.port == 0 || ep.address.is_any()) {
            return util::unexpected(errc::invalid_arg);
        }
        return {};
    }

    struct SocketHandle {
        util::i32 value{-1};

        [[nodiscard]] constexpr bool valid() const noexcept {
            return value >= 0;
        }

        [[nodiscard]] static constexpr SocketHandle invalid() noexcept {
            return SocketHandle{};
        }
    };
}

#ifndef NDEBUG
namespace net {
    inline bool net_common_self_check() noexcept {
        constexpr auto loopback = IpAddress::ipv4_loopback();
        static_assert(loopback.is_ipv4());
        static_assert(!loopback.is_any());
        static_assert(!loopback.is_ipv4_limited_broadcast());

        constexpr auto broadcast = IpAddress::ipv4_broadcast();
        static_assert(broadcast.is_ipv4());
        static_assert(!broadcast.is_any());
        static_assert(broadcast.is_ipv4_limited_broadcast());

        constexpr auto any = Endpoint::ipv4_any(8080);
        static_assert(any.family() == AddressFamily::ipv4);
        static_assert(any.address.is_any());
        static_assert(any.port == 8080);

        constexpr auto broadcast_endpoint = Endpoint::ipv4_broadcast(8081);
        static_assert(broadcast_endpoint.family() == AddressFamily::ipv4);
        static_assert(broadcast_endpoint.address.is_ipv4_limited_broadcast());
        static_assert(broadcast_endpoint.port == 8081);

        if (!validate_bind_endpoint_v0(any)) return false;
        if (validate_remote_endpoint_v0(Endpoint::ipv4_any(9000))) return false;

        constexpr auto mask = NetEvent::readable | NetEvent::writable;
        static_assert(has_event(mask, NetEvent::readable));
        static_assert(has_event(mask, NetEvent::writable));
        static_assert(!has_event(mask, NetEvent::closed));

        constexpr SocketHandle invalid{};
        static_assert(!invalid.valid());
        return true;
    }
}
#endif
