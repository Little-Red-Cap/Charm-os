module;

#include <array>
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

    enum class ExportAction : util::u8 {
        ensure_exported,
        attach,
        detach,
        unexport,
    };

    struct ExportTransition {
        ExportAction action{ExportAction::ensure_exported};
        PublishState publish_before{PublishState::missing};
        PublishState publish_after{PublishState::missing};
        ExportState before{ExportState::missing};
        ExportState after{ExportState::missing};
    };

    using ExportObserver = void (*)(void* ctx, const ExportTransition& transition) noexcept;

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
            const auto publish_before = publish_state();
            const auto before = state();
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
            auto exported = registry_->register_channel(desc_, slot_.channel(), reactor_);
            if (exported) {
                notify_transition(ExportAction::ensure_exported, publish_before, before);
            }
            return exported;
        }

        util::Result<void> attach(Channel& target) noexcept {
            const auto publish_before = publish_state();
            const auto before = state();
            auto exported = ensure_exported();
            if (!exported) {
                return exported;
            }
            auto attached = slot_.attach(target);
            if (attached) {
                notify_transition(ExportAction::attach, publish_before, before);
            }
            return attached;
        }

        void detach() noexcept {
            const auto publish_before = publish_state();
            const auto before = state();
            slot_.detach();
            notify_transition(ExportAction::detach, publish_before, before);
        }

        util::Result<void> unexport() noexcept {
            const auto publish_before = publish_state();
            const auto before = state();
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
            auto unexported = registry_->unregister_channel(desc_.cap);
            if (unexported) {
                notify_transition(ExportAction::unexport, publish_before, before);
            }
            return unexported;
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

        void set_observer(ExportObserver observer, void* ctx = nullptr) noexcept {
            observer_ = observer;
            observer_ctx_ = ctx;
        }

        ChannelSlot& slot() noexcept { return slot_; }
        const ChannelSlot& slot() const noexcept { return slot_; }

        Channel& channel() noexcept { return slot_.channel(); }
        const Channel& channel() const noexcept { return slot_.channel(); }

    private:
        void notify_transition(ExportAction action,
                               PublishState publish_before,
                               ExportState before) noexcept {
            if (!observer_) {
                return;
            }
            const auto publish_after = publish_state();
            const auto after = state();
            if (publish_before == publish_after && before == after) {
                return;
            }
            observer_(observer_ctx_,
                      ExportTransition{
                          action,
                          publish_before,
                          publish_after,
                          before,
                          after
                      });
        }

        RegistryT* registry_{nullptr};
        EndpointDesc desc_{};
        Reactor* reactor_{nullptr};
        ChannelSlot slot_{};
        ExportObserver observer_{nullptr};
        void* observer_ctx_{nullptr};
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

        struct TransitionLog {
            std::array<ExportTransition, 4> events{};
            util::usize count{0};

            static void on_transition(void* ctx, const ExportTransition& transition) noexcept {
                auto* self = static_cast<TransitionLog*>(ctx);
                if (!self || self->count >= self->events.size()) {
                    return;
                }
                self->events[self->count++] = transition;
            }
        };

        Registry<2> registry{};
        registry.init();

        ChannelSlotExport<Registry<2>> exported{
            registry,
            "io.usb0",
            EndpointCaps::duplex
        };
        TransitionLog log{};
        exported.set_observer(&TransitionLog::on_transition, &log);
        if (exported.publish_state() != PublishState::missing) return false;
        if (exported.published()) return false;
        if (exported.exported()) return false;
        if (exported.state() != ExportState::missing) return false;
        if (!exported.ensure_exported()) return false;
        if (log.count != 1) return false;
        if (log.events[0].action != ExportAction::ensure_exported) return false;
        if (log.events[0].publish_before != PublishState::missing ||
            log.events[0].publish_after != PublishState::published) return false;
        if (log.events[0].before != ExportState::missing ||
            log.events[0].after != ExportState::detached) return false;
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
        if (log.count != 2) return false;
        if (log.events[1].action != ExportAction::attach) return false;
        if (log.events[1].before != ExportState::detached ||
            log.events[1].after != ExportState::attached) return false;
        if (!exported.attached()) return false;
        if (exported.state() != ExportState::attached) return false;
        if (exported.generation() != 1) return false;

        byte = 0;
        r = exported.channel().read(std::span<util::u8>(&byte, 1));
        if (!r || r.value() != 1 || byte != 0x5A) return false;

        exported.detach();
        if (log.count != 3) return false;
        if (log.events[2].action != ExportAction::detach) return false;
        if (log.events[2].before != ExportState::attached ||
            log.events[2].after != ExportState::detached) return false;
        if (exported.attached()) return false;
        if (exported.state() != ExportState::detached) return false;
        if (exported.generation() != 2) return false;

        byte = 0;
        r = exported.channel().read(std::span<util::u8>(&byte, 1));
        if (r.error() != errc::noent) return false;

        if (!exported.unexport()) return false;
        if (log.count != 4) return false;
        if (log.events[3].action != ExportAction::unexport) return false;
        if (log.events[3].publish_before != PublishState::published ||
            log.events[3].publish_after != PublishState::missing) return false;
        if (log.events[3].before != ExportState::detached ||
            log.events[3].after != ExportState::missing) return false;
        if (exported.publish_state() != PublishState::missing) return false;
        if (exported.published()) return false;
        if (exported.exported()) return false;
        if (exported.state() != ExportState::missing) return false;
        if (registry.open_channel("io.usb0") != nullptr) return false;

        return exported.unexport().error() == util::Errc::noent;
    }
#endif
}
