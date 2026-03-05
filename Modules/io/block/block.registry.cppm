module;

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

export module block.registry;

import block.device;
import init.node;
import util.core;
import util.error;

export namespace block {
    using CapId = util::u32;

    struct DeviceDesc {
        std::string_view name{};
        CapId cap{0};
    };

    struct DeviceEndpoint {
        DeviceDesc desc{};
        Device* dev{nullptr};
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

    template <util::usize MaxDevices>
    class Registry {
    public:
        void init() noexcept {
            for (auto& ep : devices_) {
                ep = {};
            }
            count_ = 0;
        }

        util::Result<void> register_device(const DeviceDesc& desc,
                                           Device& dev) noexcept {
            if (desc.name.empty() || desc.cap == 0) {
                return util::unexpected(util::Errc::invalid_arg);
            }
            if (find_device(desc.name) || find_device(desc.cap)) {
                return util::unexpected(util::Errc::exist);
            }
            if (count_ >= devices_.size()) {
                return util::unexpected(util::Errc::buffer_overflow);
            }
            devices_[count_++] = DeviceEndpoint{desc, &dev};
            return {};
        }

        util::Result<void> replace_device(const DeviceDesc& desc,
                                          Device& dev) noexcept {
            if (desc.name.empty() || desc.cap == 0) {
                return util::unexpected(util::Errc::invalid_arg);
            }
            for (util::usize i = 0; i < count_; ++i) {
                if (devices_[i].desc.cap != desc.cap) continue;
                if (devices_[i].desc.name.compare(desc.name) != 0) {
                    return util::unexpected(util::Errc::exist);
                }
                devices_[i] = DeviceEndpoint{desc, &dev};
                return {};
            }
            return util::unexpected(util::Errc::noent);
        }

        Device* open_device(std::string_view name) noexcept {
            auto* ep = find_device(name);
            return ep ? ep->dev : nullptr;
        }

        Device* open_device(CapId cap) noexcept {
            auto* ep = find_device(cap);
            return ep ? ep->dev : nullptr;
        }

        const DeviceEndpoint* find_device(std::string_view name) const noexcept {
            for (util::usize i = 0; i < count_; ++i) {
                if (devices_[i].desc.name.compare(name) == 0) {
                    return &devices_[i];
                }
            }
            return nullptr;
        }

        const DeviceEndpoint* find_device(CapId cap) const noexcept {
            for (util::usize i = 0; i < count_; ++i) {
                if (devices_[i].desc.cap == cap) return &devices_[i];
            }
            return nullptr;
        }

        using VisitFn = void (*)(void* ctx, const DeviceEndpoint& ep) noexcept;

        void list_devices(VisitFn fn, void* ctx) const noexcept {
            if (!fn) return;
            for (util::usize i = 0; i < count_; ++i) {
                fn(ctx, devices_[i]);
            }
        }

        util::usize size() const noexcept { return count_; }
        util::usize capacity() const noexcept { return devices_.size(); }

    private:
        std::array<DeviceEndpoint, MaxDevices> devices_{};
        util::usize count_{0};
    };

    template <typename RegistryT>
    struct RegistryBinding {
        RegistryT* registry{nullptr};
        std::array<init::CapId, 1> provides{};
        init::Node node{};

        explicit RegistryBinding(RegistryT& reg,
                                 const char* cap_name = "block.registry",
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
        Device dev_a{};
        Device dev_b{};
        DeviceDesc a{"block.sd0", cap_id("block.sd0")};
        DeviceDesc b{"block.flash0", cap_id("block.flash0")};
        if (!reg.register_device(a, dev_a)) return false;
        if (reg.register_device(a, dev_a)) return false;
        if (reg.find_device("block.sd0") == nullptr) return false;
        if (reg.open_device(a.cap) != &dev_a) return false;
        if (reg.replace_device(b, dev_b)) return false;
        if (!reg.replace_device(a, dev_b)) return false;
        if (reg.open_device("block.sd0") != &dev_b) return false;
        util::usize count = 0;
        reg.list_devices([](void* ctx, const DeviceEndpoint&) noexcept {
            auto* c = static_cast<util::usize*>(ctx);
            ++(*c);
        }, &count);
        return count == 1;
    }
#endif
}
