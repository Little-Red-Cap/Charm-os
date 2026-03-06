module;

#include <cstddef>

export module usb.class_msc_block;

import block.device;
import usb.class_msc;

export namespace usb::class_driver {
    struct MscBlockConfig {
        const char* vendor{"Charm"};
        const char* product{"BlockDevice"};
        const char* revision{"1.00"};
        bool removable{true};
        bool read_only{false};
    };

    inline MscInquiry make_inquiry(const MscBlockConfig& cfg) noexcept {
        MscInquiry inquiry{};
        inquiry.vendor = cfg.vendor;
        inquiry.product = cfg.product;
        inquiry.revision = cfg.revision;
        inquiry.removable = cfg.removable;
        return inquiry;
    }

    inline void apply_block_caps(block::Device& dev, bool read_only) noexcept {
        if (dev.caps == 0) {
            dev.caps = block::caps_from_ops(dev);
        }
        if (read_only) {
            dev.caps &= ~block::to_bits(block::Caps::write);
            dev.caps &= ~block::to_bits(block::Caps::erase);
        }
    }

    inline MscStorage make_storage_from_block_device(block::Device& dev,
                                                     const MscBlockConfig& cfg = {}) noexcept {
        apply_block_caps(dev, cfg.read_only);
        MscStorage storage{};
        storage.dev = &dev;
        storage.inquiry = make_inquiry(cfg);
        storage.read_only = cfg.read_only;
        return storage;
    }

    inline MscStorage make_storage_from_block_device(block::Device& dev,
                                                     const MscInquiry& inquiry,
                                                     bool read_only = false) noexcept {
        apply_block_caps(dev, read_only);
        MscStorage storage{};
        storage.dev = &dev;
        storage.inquiry = inquiry;
        storage.read_only = read_only;
        return storage;
    }
}
