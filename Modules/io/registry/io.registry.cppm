module;

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

export module io.registry;

import io.channel;
import io.reactor;
import init.node;
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
        return hash == 0 ? 1u : hash;
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
                return util::unexpected(util::Errc::exist);
            }
            if (count_ >= endpoints_.size()) {
                return util::unexpected(util::Errc::buffer_overflow);
            }
            endpoints_[count_++] = ChannelEndpoint{desc, &ch, reactor};
            return {};
        }

        util::Result<void> replace_channel(const EndpointDesc& desc,
                                           Channel& ch,
                                           Reactor* reactor = nullptr) noexcept {
            if (desc.name.empty() || desc.cap == 0) {
                return util::unexpected(util::Errc::invalid_arg);
            }
            for (util::usize i = 0; i < count_; ++i) {
                if (endpoints_[i].desc.cap != desc.cap) continue;
                if (endpoints_[i].desc.name != desc.name) {
                    return util::unexpected(util::Errc::exist);
                }
                endpoints_[i] = ChannelEndpoint{desc, &ch, reactor};
                return {};
            }
            return util::unexpected(util::Errc::noent);
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

    template <typename RegistryT>
    struct RegistryBinding {
        RegistryT* registry{nullptr};
        std::array<init::CapId, 1> provides{};
        init::Node node{};

        explicit RegistryBinding(RegistryT& reg,
                                 const char* cap_name = "io.registry",
                                 init::Phase phase = init::Phase::core,
                                 util::u32 runlevel_mask = static_cast<util::u32>(init::Runlevel::all)) noexcept
            : registry(&reg) {
            provides[0] = init::cap_id(cap_name);
            node = init::Node{
                cap_name,
                phase,
                runlevel_mask,
                std::span<const init::CapId>(provides.data(), provides.size()),
                {},
                &RegistryBinding::init_trampoline,
                nullptr,
                this
            };
        }

        static util::Result<void> init_trampoline(void* ctx) noexcept {
            auto* self = static_cast<RegistryBinding*>(ctx);
            if (!self || !self->registry) {
                return util::unexpected(util::Errc::invalid_arg);
            }
            self->registry->init();
            return {};
        }
    };

#ifndef NDEBUG
    inline bool registry_self_check() noexcept {
        Registry<4> reg{};
        reg.init();
        Channel ch_a{};
        Channel ch_b{};
        EndpointDesc a{"io.console0", cap_id("io.console0"), EndpointKind::channel, EndpointCaps::duplex};
        EndpointDesc b{"io.uart1", cap_id("io.uart1"), EndpointKind::channel, EndpointCaps::readable};
        if (!reg.register_channel(a, ch_a)) return false;
        if (reg.register_channel(a, ch_a)) return false;
        if (reg.find_channel("io.console0") == nullptr) return false;
        if (reg.open_channel(a.cap) != &ch_a) return false;
        if (reg.replace_channel(b, ch_b)) return false;
        if (!reg.replace_channel(a, ch_b)) return false;
        if (reg.open_channel("io.console0") != &ch_b) return false;
        util::usize count = 0;
        reg.list_channels([](void* ctx, const ChannelEndpoint&) noexcept {
            auto* c = static_cast<util::usize*>(ctx);
            ++(*c);
        }, &count);
        return count == 1;
    }
#endif
}
