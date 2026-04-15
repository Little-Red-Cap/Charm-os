module;

#include <array>
#include <span>
#include <string_view>

export module block.device.slot_export;

import block.device;
import block.device.slot;
import block.registry;
import util.core;
import util.error;

export namespace block {
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
    class DeviceSlotExport {
    public:
        DeviceSlotExport(RegistryT& registry, const DeviceDesc& desc) noexcept
            : registry_(&registry), desc_(desc) {
        }

        DeviceSlotExport(RegistryT& registry, std::string_view endpoint_name) noexcept
            : DeviceSlotExport(registry, DeviceDesc{endpoint_name, cap_id(endpoint_name)}) {
        }

        util::Result<void> ensure_exported() noexcept {
            const auto publish_before = publish_state();
            const auto before = state();
            if (!registry_ || desc_.name.empty() || desc_.cap == 0) {
                return util::unexpected(util::Errc::invalid_arg);
            }
            if (const auto* ep = registry_->find_device(desc_.cap)) {
                if (ep->desc.name.compare(desc_.name) != 0 || ep->dev != &slot_.device()) {
                    return util::unexpected(util::Errc::exist);
                }
                return {};
            }
            auto exported = registry_->register_device(desc_, slot_.device());
            if (exported) {
                notify_transition(ExportAction::ensure_exported, publish_before, before);
            }
            return exported;
        }

        util::Result<void> attach(Device& target) noexcept {
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
            const auto* ep = registry_->find_device(desc_.cap);
            if (!ep) {
                return util::unexpected(util::Errc::noent);
            }
            if (ep->desc.name.compare(desc_.name) != 0 || ep->dev != &slot_.device()) {
                return util::unexpected(util::Errc::exist);
            }
            slot_.detach();
            auto unexported = registry_->unregister_device(desc_.cap);
            if (unexported) {
                notify_transition(ExportAction::unexport, publish_before, before);
            }
            return unexported;
        }

        [[nodiscard]] bool exported() const noexcept {
            if (!registry_) {
                return false;
            }
            const auto* ep = registry_->find_device(desc_.cap);
            return ep != nullptr &&
                    ep->desc.name.compare(desc_.name) == 0 &&
                    ep->dev == &slot_.device();
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
        [[nodiscard]] Device* target() const noexcept { return slot_.target(); }
        [[nodiscard]] const DeviceDesc& desc() const noexcept { return desc_; }

        void set_observer(ExportObserver observer, void* ctx = nullptr) noexcept {
            observer_ = observer;
            observer_ctx_ = ctx;
        }

        DeviceSlot& slot() noexcept { return slot_; }
        const DeviceSlot& slot() const noexcept { return slot_; }

        Device& device() noexcept { return slot_.device(); }
        const Device& device() const noexcept { return slot_.device(); }

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
        DeviceDesc desc_{};
        DeviceSlot slot_{};
        ExportObserver observer_{nullptr};
        void* observer_ctx_{nullptr};
    };

#ifndef NDEBUG
    inline bool device_slot_export_self_check() noexcept {
        struct DummyDisk {
            static Status read_cb(void*, util::u64, std::span<util::u8> out) noexcept {
                if (out.empty()) {
                    return Status{Errc::invalid_arg};
                }
                out[0] = 0xA5;
                return {};
            }

            Device dev{};

            DummyDisk() noexcept {
                dev.ctx = this;
                dev.read = &DummyDisk::read_cb;
                dev.block_size = 512;
                dev.block_count = 2;
                dev.caps = to_bits(Caps::read);
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

        DeviceSlotExport<Registry<2>> exported{registry, "block.usb0"};
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
        if (registry.open_device("block.usb0") != &exported.device()) return false;

        DummyDisk disk{};
        if (!exported.attach(disk.dev)) return false;
        if (log.count != 2) return false;
        if (log.events[1].action != ExportAction::attach) return false;
        if (log.events[1].before != ExportState::detached ||
            log.events[1].after != ExportState::attached) return false;
        if (!exported.attached()) return false;
        if (exported.state() != ExportState::attached) return false;
        if (exported.generation() != 1) return false;

        util::u8 byte = 0;
        auto st = exported.device().read(exported.device().ctx, 0, std::span<util::u8>(&byte, 1));
        if (!st || byte != 0xA5) return false;

        exported.detach();
        if (log.count != 3) return false;
        if (log.events[2].action != ExportAction::detach) return false;
        if (log.events[2].before != ExportState::attached ||
            log.events[2].after != ExportState::detached) return false;
        if (exported.attached()) return false;
        if (exported.state() != ExportState::detached) return false;
        if (exported.generation() != 2) return false;

        byte = 0;
        st = exported.device().read(exported.device().ctx, 0, std::span<util::u8>(&byte, 1));
        if (st.err != Errc::noent) return false;

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
        if (registry.open_device("block.usb0") != nullptr) return false;

        return exported.unexport().error() == util::Errc::noent;
    }
#endif
}
