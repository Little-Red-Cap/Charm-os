module;

#include <array>
#include <cstddef>

export module usb.class_mux;

import usb.common;
import usb.device;

export namespace usb::device {
    struct ClassMuxSlot {
        void* ctx{nullptr};
        const ClassOps* ops{nullptr};
        usb::u8 first_interface{0};
        usb::u8 interface_count{0};
        const usb::u8* endpoints{nullptr};
        std::size_t endpoint_count{0};
    };

    template <std::size_t MaxSlots = 4>
    class CompositeClassMux {
    public:
        enum class StandardKind : usb::u8 {
            get_status,
            clear_feature,
            set_feature,
        };

        bool add_slot(const ClassMuxSlot& slot) noexcept {
            if (slot_count_ >= MaxSlots) return false;
            slots_[slot_count_++] = slot;
            return true;
        }

        const ClassOps* class_ops() const noexcept {
            static const ClassOps ops{
                &CompositeClassMux::handle_setup,
                &CompositeClassMux::handle_control_out,
                &CompositeClassMux::handle_get_status,
                &CompositeClassMux::handle_clear_feature,
                &CompositeClassMux::handle_set_feature,
                &CompositeClassMux::handle_vendor_setup,
                &CompositeClassMux::handle_vendor_out,
                &CompositeClassMux::handle_reset,
            };
            return &ops;
        }

    private:
        static bool handle_setup(void* ctx,
                                 const ControlRequest& req,
                                 ControlResponse& resp) noexcept {
            auto* self = static_cast<CompositeClassMux*>(ctx);
            if (!self) return false;
            return self->dispatch_setup(req, resp, false);
        }

        static bool handle_control_out(void* ctx,
                                       const ControlRequest& req,
                                       ControlResponse& resp) noexcept {
            auto* self = static_cast<CompositeClassMux*>(ctx);
            if (!self) return false;
            return self->dispatch_setup(req, resp, true);
        }

        static bool handle_get_status(void* ctx,
                                      const SetupPacket& setup,
                                      ControlResponse& resp) noexcept {
            auto* self = static_cast<CompositeClassMux*>(ctx);
            return self ? self->dispatch_standard(setup, resp, StandardKind::get_status) : false;
        }

        static bool handle_clear_feature(void* ctx,
                                         const SetupPacket& setup,
                                         ControlResponse& resp) noexcept {
            auto* self = static_cast<CompositeClassMux*>(ctx);
            return self ? self->dispatch_standard(setup, resp, StandardKind::clear_feature) : false;
        }

        static bool handle_set_feature(void* ctx,
                                       const SetupPacket& setup,
                                       ControlResponse& resp) noexcept {
            auto* self = static_cast<CompositeClassMux*>(ctx);
            return self ? self->dispatch_standard(setup, resp, StandardKind::set_feature) : false;
        }

        static bool handle_vendor_setup(void* ctx,
                                        const ControlRequest& req,
                                        ControlResponse& resp) noexcept {
            auto* self = static_cast<CompositeClassMux*>(ctx);
            if (!self) return false;
            return self->dispatch_vendor(req, resp, false);
        }

        static bool handle_vendor_out(void* ctx,
                                      const ControlRequest& req,
                                      ControlResponse& resp) noexcept {
            auto* self = static_cast<CompositeClassMux*>(ctx);
            if (!self) return false;
            return self->dispatch_vendor(req, resp, true);
        }

        static void handle_reset(void* ctx) noexcept {
            auto* self = static_cast<CompositeClassMux*>(ctx);
            if (!self) return;
            for (std::size_t index = 0; index < self->slot_count_; ++index) {
                const auto& slot = self->slots_[index];
                if (slot.ops && slot.ops->reset) {
                    slot.ops->reset(slot.ctx);
                }
            }
        }

        bool dispatch_standard(const SetupPacket& setup,
                               ControlResponse& resp,
                               StandardKind kind) noexcept {
            const auto* slot = resolve_slot(setup);
            if (slot && slot->ops) {
                if (auto slot_fn = slot_standard_fn(*slot, kind)) {
                    return slot_fn(slot->ctx, setup, resp);
                }
            }
            return false;
        }

        bool dispatch_setup(const ControlRequest& req,
                            ControlResponse& resp,
                            bool out_stage) noexcept {
            const auto* slot = resolve_slot(req.setup);
            if (slot && slot->ops) {
                auto fn = out_stage ? slot->ops->control_out : slot->ops->setup;
                if (fn && fn(slot->ctx, req, resp)) {
                    return true;
                }
            }

            for (std::size_t index = 0; index < slot_count_; ++index) {
                const auto& candidate = slots_[index];
                if (&candidate == slot || !candidate.ops) continue;
                auto fn = out_stage ? candidate.ops->control_out : candidate.ops->setup;
                if (fn && fn(candidate.ctx, req, resp)) {
                    return true;
                }
            }
            return false;
        }

        bool dispatch_vendor(const ControlRequest& req,
                             ControlResponse& resp,
                             bool out_stage) noexcept {
            const auto* slot = resolve_slot(req.setup);
            if (slot && slot->ops) {
                auto fn = out_stage ? slot->ops->vendor_out : slot->ops->vendor_setup;
                if (fn && fn(slot->ctx, req, resp)) {
                    return true;
                }
            }

            for (std::size_t index = 0; index < slot_count_; ++index) {
                const auto& candidate = slots_[index];
                if (&candidate == slot || !candidate.ops) continue;
                auto fn = out_stage ? candidate.ops->vendor_out : candidate.ops->vendor_setup;
                if (fn && fn(candidate.ctx, req, resp)) {
                    return true;
                }
            }
            return false;
        }

        const ClassMuxSlot* resolve_slot(const SetupPacket& setup) const noexcept {
            const auto recipient = usb::request_recipient(setup.bm_request_type);
            if (recipient == usb::RequestRecipient::interface) {
                const auto interface_number = static_cast<usb::u8>(setup.w_index & 0xFFu);
                return find_by_interface(interface_number);
            }
            if (recipient == usb::RequestRecipient::endpoint) {
                const auto endpoint_address = static_cast<usb::u8>(setup.w_index & 0xFFu);
                return find_by_endpoint(endpoint_address);
            }
            return nullptr;
        }

        const ClassMuxSlot* find_by_interface(usb::u8 interface_number) const noexcept {
            for (std::size_t index = 0; index < slot_count_; ++index) {
                const auto& slot = slots_[index];
                if (slot.interface_count == 0) continue;
                if (interface_number >= slot.first_interface &&
                    interface_number < static_cast<usb::u8>(slot.first_interface + slot.interface_count)) {
                    return &slot;
                }
            }
            return nullptr;
        }

        const ClassMuxSlot* find_by_endpoint(usb::u8 endpoint_address) const noexcept {
            for (std::size_t index = 0; index < slot_count_; ++index) {
                const auto& slot = slots_[index];
                for (std::size_t ep = 0; ep < slot.endpoint_count; ++ep) {
                    if (slot.endpoints && slot.endpoints[ep] == endpoint_address) {
                        return &slot;
                    }
                }
            }
            return nullptr;
        }

        static bool (*slot_standard_fn(const ClassMuxSlot& slot,
                                       StandardKind kind) noexcept)(void*, const SetupPacket&, ControlResponse&) {
            if (!slot.ops) return nullptr;
            switch (kind) {
            case StandardKind::get_status:
                return slot.ops->get_status;
            case StandardKind::clear_feature:
                return slot.ops->clear_feature;
            case StandardKind::set_feature:
                return slot.ops->set_feature;
            }
            return nullptr;
        }

        std::array<ClassMuxSlot, MaxSlots> slots_{};
        std::size_t slot_count_{0};
    };
}
