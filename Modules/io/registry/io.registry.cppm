module;

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

export module io.registry;

import io.channel;
import io.reactor;
import util.core;
import util.error;

export namespace io {
    using CapId = util::u32;
    using EndpointId = util::u32;

    enum class EndpointKind : util::u8 {
        channel,
    };

    enum class EndpointCaps : util::u32 {
        none = 0,
        isr_safe = 1u << 0,
        readable = 1u << 1,
        writable = 1u << 2,
        duplex = readable | writable,
    };

    constexpr EndpointCaps operator|(EndpointCaps a, EndpointCaps b) noexcept {
        return static_cast<EndpointCaps>(static_cast<util::u32>(a) | static_cast<util::u32>(b));
    }

    struct EndpointDesc {
        std::string_view name{};
        CapId cap{0};
        EndpointKind kind{EndpointKind::channel};
        EndpointCaps caps{EndpointCaps::none};
    };

    struct ChannelEndpoint {
        EndpointDesc desc{};
        Channel* ch{nullptr};
        Reactor* reactor{nullptr};
    };

    consteval CapId cap_id(const char* literal) {
        CapId hash = 2166136261u;
        for (const unsigned char* p = reinterpret_cast<const unsigned char*>(literal); *p; ++p) {
            hash ^= static_cast<CapId>(*p);
            hash *= 16777619u;
        }
        return hash;
    }

    template <util::usize MaxEndpoints>
    class Registry {
    public:
        void init() noexcept {
            for (auto& ep : endpoints_) {
                ep = {};
            }
            count_ = 0;
        }

        util::Result<void> register_channel(const EndpointDesc& desc,
                                            Channel& ch,
                                            Reactor* reactor = nullptr) noexcept {
            if (desc.name.empty() || desc.cap == 0) {
                return util::unexpected(util::Errc::invalid_arg);
            }
            if (find_channel(desc.name) || find_channel(desc.cap)) {
                return util::unexpected(util::Errc::already_exists);
            }
            if (count_ >= endpoints_.size()) {
                return util::unexpected(util::Errc::no_space);
            }
            endpoints_[count_++] = ChannelEndpoint{desc, &ch, reactor};
            return {};
        }

        Channel* open_channel(std::string_view name) noexcept {
            auto* ep = find_channel(name);
            return ep ? ep->ch : nullptr;
        }

        Channel* open_channel(CapId cap) noexcept {
            auto* ep = find_channel(cap);
            return ep ? ep->ch : nullptr;
        }

        const ChannelEndpoint* find_channel(std::string_view name) const noexcept {
            for (util::usize i = 0; i < count_; ++i) {
                if (endpoints_[i].desc.name == name) return &endpoints_[i];
            }
            return nullptr;
        }

        const ChannelEndpoint* find_channel(CapId cap) const noexcept {
            for (util::usize i = 0; i < count_; ++i) {
                if (endpoints_[i].desc.cap == cap) return &endpoints_[i];
            }
            return nullptr;
        }

        using VisitFn = void (*)(void* ctx, const ChannelEndpoint& ep) noexcept;

        void list_channels(VisitFn fn, void* ctx) const noexcept {
            if (!fn) return;
            for (util::usize i = 0; i < count_; ++i) {
                fn(ctx, endpoints_[i]);
            }
        }

        util::usize size() const noexcept { return count_; }
        util::usize capacity() const noexcept { return endpoints_.size(); }

    private:
        std::array<ChannelEndpoint, MaxEndpoints> endpoints_{};
        util::usize count_{0};
    };
}
