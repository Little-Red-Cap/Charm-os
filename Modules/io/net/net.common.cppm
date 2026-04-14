module;

#include <array>
#include <cstddef>
#include <cstdint>
#include <utility>

export module net.common;

import util.core;
import util.error;
import util.expected;

export namespace net {
    template <class T>
    class View {
    public:
        using element_type = T;
        using value_type = T;
        using size_type = util::usize;
        using pointer = T*;
        using reference = T&;
        using iterator = pointer;

        constexpr View() noexcept = default;

        constexpr View(pointer data, size_type size) noexcept
            : data_(data), size_(size) {}

        [[nodiscard]] constexpr pointer data() const noexcept {
            return data_;
        }

        [[nodiscard]] constexpr size_type size() const noexcept {
            return size_;
        }

        [[nodiscard]] constexpr bool empty() const noexcept {
            return size_ == 0;
        }

        [[nodiscard]] constexpr reference operator[](size_type index) const noexcept {
            return data_[index];
        }

        [[nodiscard]] constexpr iterator begin() const noexcept {
            return data_;
        }

        [[nodiscard]] constexpr iterator end() const noexcept {
            return data_ + size_;
        }

        [[nodiscard]] constexpr View subspan(size_type offset) const noexcept {
            if (offset >= size_) {
                return {};
            }
            return View{data_ + offset, size_ - offset};
        }

        [[nodiscard]] constexpr View subspan(size_type offset, size_type count) const noexcept {
            if (offset >= size_) {
                return {};
            }
            const size_type remaining = size_ - offset;
            return View{data_ + offset, count < remaining ? count : remaining};
        }

    private:
        pointer data_{nullptr};
        size_type size_{0};
    };

    using ByteView = View<const util::u8>;
    using MutByteView = View<util::u8>;

    using errc = util::Errc;

    template <class T>
    using Result = util::Result<T>;

    using IoResult = Result<util::usize>;
    using EventMask = util::u32;

    constexpr IoResult ok(util::usize n) noexcept {
        return IoResult{std::in_place, n};
    }

    constexpr IoResult fail(errc e) noexcept {
        return util::unexpected(e);
    }

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

        constexpr auto any = Endpoint::ipv4_any(8080);
        static_assert(any.family() == AddressFamily::ipv4);
        static_assert(any.address.is_any());
        static_assert(any.port == 8080);

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
