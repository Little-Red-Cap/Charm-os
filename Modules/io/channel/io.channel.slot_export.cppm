module;

#include <span>
#include <string_view>

export module io.channel.slot_export;

import io.channel;
import io.channel.slot;
import io.reactor;
import io.registry;
import util.core;
import util.error;

export namespace io {
    enum class ExportState : util::u8 {
        missing,
        detached,
        attached,
    };

    template <typename RegistryT>
    class ChannelSlotExport {
    public:
        ChannelSlotExport(RegistryT& registry,
                          const EndpointDesc& desc,
                          Reactor* reactor = nullptr) noexcept
            : registry_(&registry), desc_(desc), reactor_(reactor) {
        }

        ChannelSlotExport(RegistryT& registry,
                          std::string_view endpoint_name,
                          EndpointCaps caps = EndpointCaps::duplex,
                          Reactor* reactor = nullptr) noexcept
            : ChannelSlotExport(registry,
                                EndpointDesc{
                                    endpoint_name,
                                    cap_id(endpoint_name),
                                    EndpointKind::channel,
                                    caps
                                },
                                reactor) {
        }

        util::Result<void> ensure_exported() noexcept {
            if (!registry_ || desc_.name.empty() || desc_.cap == 0) {
                return util::unexpected(util::Errc::invalid_arg);
            }
            if (const auto* ep = registry_->find_channel(desc_.cap)) {
                if (ep->desc.name.compare(desc_.name) != 0 ||
                    ep->ch != &slot_.channel() ||
                    ep->reactor != reactor_) {
                    return util::unexpected(util::Errc::exist);
                }
                return {};
            }
            return registry_->register_channel(desc_, slot_.channel(), reactor_);
        }

        util::Result<void> attach(Channel& target) noexcept {
            auto exported = ensure_exported();
            if (!exported) {
                return exported;
            }
            return slot_.attach(target);
        }

        void detach() noexcept {
            slot_.detach();
        }

        util::Result<void> unexport() noexcept {
            if (!registry_ || desc_.name.empty() || desc_.cap == 0) {
                return util::unexpected(util::Errc::invalid_arg);
            }
            const auto* ep = registry_->find_channel(desc_.cap);
            if (!ep) {
                return util::unexpected(util::Errc::noent);
            }
            if (ep->desc.name.compare(desc_.name) != 0 ||
                ep->ch != &slot_.channel() ||
                ep->reactor != reactor_) {
                return util::unexpected(util::Errc::exist);
            }
            slot_.detach();
            return registry_->unregister_channel(desc_.cap);
        }

        [[nodiscard]] bool exported() const noexcept {
            if (!registry_) {
                return false;
            }
            const auto* ep = registry_->find_channel(desc_.cap);
            return ep != nullptr &&
                   ep->desc.name.compare(desc_.name) == 0 &&
                   ep->ch == &slot_.channel() &&
                   ep->reactor == reactor_;
        }

        [[nodiscard]] PublishState publish_state() const noexcept {
            if (!registry_) {
                return PublishState::missing;
            }
            return registry_->publish_state(desc_.cap);
        }

        [[nodiscard]] bool published() const noexcept {
            return publish_state() == PublishState::published;
        }

        [[nodiscard]] ExportState state() const noexcept {
            if (!exported()) {
                return ExportState::missing;
            }
            return attached() ? ExportState::attached : ExportState::detached;
        }

        [[nodiscard]] bool attached() const noexcept { return slot_.attached(); }
        [[nodiscard]] util::u32 generation() const noexcept { return slot_.generation(); }
        [[nodiscard]] Channel* target() const noexcept { return slot_.target(); }
        [[nodiscard]] const EndpointDesc& desc() const noexcept { return desc_; }
        [[nodiscard]] Reactor* reactor() const noexcept { return reactor_; }

        ChannelSlot& slot() noexcept { return slot_; }
        const ChannelSlot& slot() const noexcept { return slot_; }

        Channel& channel() noexcept { return slot_.channel(); }
        const Channel& channel() const noexcept { return slot_.channel(); }

    private:
        RegistryT* registry_{nullptr};
        EndpointDesc desc_{};
        Reactor* reactor_{nullptr};
        ChannelSlot slot_{};
    };

#ifndef NDEBUG
    inline bool channel_slot_export_self_check() noexcept {
        struct DummyChannel {
            static result read_cb(void*, MutByteView out) noexcept {
                if (out.empty()) {
                    return fail(errc::invalid_arg);
                }
                out[0] = 0x5A;
                return ok(1);
            }

            static result write_cb(void*, ByteView in) noexcept {
                return in.empty() ? fail(errc::invalid_arg) : ok(in.size());
            }

            static result flush_cb(void*) noexcept {
                return ok(1);
            }

            Channel ch{};

            DummyChannel() noexcept
                : ch{
                    this,
                    ChannelOps{
                        &DummyChannel::read_cb,
                        &DummyChannel::write_cb,
                        &DummyChannel::flush_cb
                    }
                } {
            }
        };

        Registry<2> registry{};
        registry.init();

        ChannelSlotExport<Registry<2>> exported{
            registry,
            "io.usb0",
            EndpointCaps::duplex
        };
        if (exported.publish_state() != PublishState::missing) return false;
        if (exported.published()) return false;
        if (exported.exported()) return false;
        if (exported.state() != ExportState::missing) return false;
        if (!exported.ensure_exported()) return false;
        if (exported.publish_state() != PublishState::published) return false;
        if (!exported.published()) return false;
        if (!exported.exported()) return false;
        if (exported.state() != ExportState::detached) return false;
        if (registry.open_channel("io.usb0") != &exported.channel()) return false;

        util::u8 byte = 0;
        auto r = exported.channel().read(std::span<util::u8>(&byte, 1));
        if (r.error() != errc::noent) return false;

        DummyChannel target{};
        if (!exported.attach(target.ch)) return false;
        if (!exported.attached()) return false;
        if (exported.state() != ExportState::attached) return false;
        if (exported.generation() != 1) return false;

        byte = 0;
        r = exported.channel().read(std::span<util::u8>(&byte, 1));
        if (!r || r.value() != 1 || byte != 0x5A) return false;

        exported.detach();
        if (exported.attached()) return false;
        if (exported.state() != ExportState::detached) return false;
        if (exported.generation() != 2) return false;

        byte = 0;
        r = exported.channel().read(std::span<util::u8>(&byte, 1));
        if (r.error() != errc::noent) return false;

        if (!exported.unexport()) return false;
        if (exported.publish_state() != PublishState::missing) return false;
        if (exported.published()) return false;
        if (exported.exported()) return false;
        if (exported.state() != ExportState::missing) return false;
        if (registry.open_channel("io.usb0") != nullptr) return false;

        return exported.unexport().error() == util::Errc::noent;
    }
#endif
}
