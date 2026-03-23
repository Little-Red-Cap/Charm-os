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

extern "C" __attribute__((weak)) void charm_io_registry_debug_exist(const char*, util::u32) noexcept {
}

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

    constexpr CapId cap_id(std::string_view sv) {
        CapId hash = 2166136261u;
        for (unsigned char c : sv) {
            hash ^= static_cast<CapId>(c);
            hash *= 16777619u;
        }
        return hash == 0 ? 1u : hash;
    }

    template <std::size_t N>
    consteval CapId cap_id(const char (&literal)[N]) {
        return cap_id(std::string_view{literal, N > 0 ? (N - 1) : 0});
    }

    template <util::usize MaxEndpoints>
    class Registry {
    public:
        void init() noexcept {
            for (auto& ep : endpoints_) {
                ep = {};
            }
            used_ = {};
            count_ = 0;
        }

        util::Result<void> register_channel(const EndpointDesc& desc,
                                            Channel& ch,
                                            Reactor* reactor = nullptr) noexcept {
            if (desc.name.empty() || desc.cap == 0) {
                return util::unexpected(util::Errc::invalid_arg);
            }
            if (find_channel(desc.name) || find_channel(desc.cap)) {
                charm_io_registry_debug_exist(desc.name.data(), desc.cap);
                return util::unexpected(util::Errc::exist);
            }
            if (count_ >= endpoints_.size()) {
                return util::unexpected(util::Errc::buffer_overflow);
            }
            const auto slot = find_free_slot();
            if (slot >= endpoints_.size()) {
                return util::unexpected(util::Errc::buffer_overflow);
            }
            endpoints_[slot] = ChannelEndpoint{desc, &ch, reactor};
            mark_used(slot);
            ++count_;
            return {};
        }

        util::Result<void> replace_channel(const EndpointDesc& desc,
                                           Channel& ch,
                                           Reactor* reactor = nullptr) noexcept {
            if (desc.name.empty() || desc.cap == 0) {
                return util::unexpected(util::Errc::invalid_arg);
            }
            for (util::usize i = 0; i < endpoints_.size(); ++i) {
                if (!is_used(i)) continue;
                if (endpoints_[i].desc.cap != desc.cap) continue;
                if (endpoints_[i].desc.name.compare(desc.name) != 0) {
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
            for (util::usize i = 0; i < endpoints_.size(); ++i) {
                if (!is_used(i)) continue;
                if (endpoints_[i].desc.name.compare(name) == 0) {
                    return &endpoints_[i];
                }
            }
            return nullptr;
        }

        const ChannelEndpoint* find_channel(CapId cap) const noexcept {
            for (util::usize i = 0; i < endpoints_.size(); ++i) {
                if (!is_used(i)) continue;
                if (endpoints_[i].desc.cap == cap) return &endpoints_[i];
            }
            return nullptr;
        }

        using VisitFn = void (*)(void* ctx, const ChannelEndpoint& ep) noexcept;

        void list_channels(VisitFn fn, void* ctx) const noexcept {
            if (!fn) return;
            for (util::usize i = 0; i < endpoints_.size(); ++i) {
                if (!is_used(i)) continue;
                fn(ctx, endpoints_[i]);
            }
        }

        util::usize size() const noexcept { return count_; }
        util::usize capacity() const noexcept { return endpoints_.size(); }

    private:
        static constexpr util::usize kWordBits = 32;
        static constexpr util::usize kWordCount = (MaxEndpoints + kWordBits - 1) / kWordBits;

        static constexpr util::usize word_index(util::usize idx) noexcept {
            return idx / kWordBits;
        }

        static constexpr util::u32 bit_mask(util::usize idx) noexcept {
            return static_cast<util::u32>(1u << (idx % kWordBits));
        }

        bool is_used(util::usize idx) const noexcept {
            return (used_[word_index(idx)] & bit_mask(idx)) != 0u;
        }

        void mark_used(util::usize idx) noexcept {
            used_[word_index(idx)] |= bit_mask(idx);
        }

        util::usize find_free_slot() const noexcept {
            for (util::usize word = 0; word < used_.size(); ++word) {
                const util::u32 used = used_[word];
                const util::u32 free = ~used;
                if (free == 0u) continue;
                for (util::u32 bit = 0; bit < kWordBits; ++bit) {
                    if ((free & (1u << bit)) == 0u) continue;
                    const util::usize idx = word * kWordBits + bit;
                    if (idx < endpoints_.size()) return idx;
                    return endpoints_.size();
                }
            }
            return endpoints_.size();
        }

        std::array<ChannelEndpoint, MaxEndpoints> endpoints_{};
        std::array<util::u32, kWordCount> used_{};
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
