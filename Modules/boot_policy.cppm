module;

#include <array>

export module boot_policy;

import util.core;
import util.alias;
import boot_core;
import boot_storage;
import boot_flow;

export namespace boot {
    struct Policy {
        util::u32 min_version{0};
        util::u32 sign_key{0};
        bool require_signature{false};
    };

    inline util::u32 calc_signature(util::u32 key, const util::u8* data, util::usize len) noexcept {
        util::u32 crc = key;
        return crc32_update(crc, data, len);
    }

    inline bool verify_signature(const ImageHeader& h, util::u32 key, util::u32 crc) noexcept {
        const util::u32 sig = calc_signature(key, reinterpret_cast<const util::u8*>(&crc), sizeof(crc));
        return h.signature == sig;
    }

    inline bool verify_version(const ImageHeader& h, const Policy& p, const BootInfo& info) noexcept {
        if (h.image_version < h.min_version) return false;
        if (h.image_version < p.min_version) return false;
        if (h.image_version < info.min_version) return false;
        return true;
    }

    inline bool verify_partition_policy(const Storage& s, const Partition& p,
                                        const Policy& policy, const BootInfo& info) noexcept {
        ImageHeader h{};
        if (!read_header(s, p, h)) return false;
        if (h.magic != k_magic || h.version != k_version) return false;
        if (h.image_size > p.size || h.payload_size > p.size) return false;
        if (h.payload_size == 0) return false;
        if (!verify_version(h, policy, info)) return false;

        std::array<util::u8, 128> buf{};
        util::u32 crc = 0;
        util::u32 remaining = h.payload_size;
        util::u32 offset = p.offset + static_cast<util::u32>(sizeof(ImageHeader));
        while (remaining > 0) {
            const auto chunk = remaining > buf.size() ? static_cast<util::u32>(buf.size()) : remaining;
            if (!storage_read(s, offset, util::span<util::u8>(buf.data(), chunk))) return false;
            crc = crc32_update(crc, buf.data(), chunk);
            offset += chunk;
            remaining -= chunk;
        }
        if (crc != h.payload_crc32) return false;
        if (policy.require_signature || (h.flags & static_cast<util::u16>(ImageFlags::signed_image)) != 0) {
            if (!verify_signature(h, policy.sign_key, crc)) return false;
        }
        return true;
    }

    inline BootResult select_slot_policy(const Storage& s, const BootConfig& cfg,
                                         BootInfo& info, const Policy& policy) noexcept {
        if (!read_boot_info(s, cfg.info, info)) {
            info = {};
        }
        const bool a_ok = verify_partition_policy(s, cfg.slot_a, policy, info);
        const bool b_ok = verify_partition_policy(s, cfg.slot_b, policy, info);

        auto pick = [&](Slot slot) -> BootResult {
            return {BootStatus::ok, slot};
        };

        if (info.pending == Slot::a && a_ok) return pick(Slot::a);
        if (info.pending == Slot::b && b_ok) return pick(Slot::b);
        if (info.active == Slot::a && a_ok) return pick(Slot::a);
        if (info.active == Slot::b && b_ok) return pick(Slot::b);
        if (a_ok) return pick(Slot::a);
        if (b_ok) return pick(Slot::b);
        return {BootStatus::invalid, Slot::a};
    }
}
