module;

#include <array>

export module boot_flow;

import util.core;
import util.alias;
import boot_core;
import boot_storage;

export namespace boot {
    struct BootConfig {
        Partition slot_a{};
        Partition slot_b{};
        Partition info{};
    };

    inline bool read_header(const Storage& s, const Partition& p, ImageHeader& out) noexcept {
        auto buf = util::span<util::u8>(reinterpret_cast<util::u8*>(&out), sizeof(ImageHeader));
        return storage_read(s, p.offset, buf);
    }

    inline bool verify_partition(const Storage& s, const Partition& p) noexcept {
        ImageHeader h{};
        if (!read_header(s, p, h)) return false;
        if (h.magic != k_magic || h.version != k_version) return false;
        if (h.image_size > p.size || h.payload_size > p.size) return false;
        if (h.payload_size == 0) return false;
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
        return crc == h.payload_crc32;
    }

    inline bool read_boot_info(const Storage& s, const Partition& p, BootInfo& info) noexcept {
        auto buf = util::span<util::u8>(reinterpret_cast<util::u8*>(&info), sizeof(BootInfo));
        return storage_read(s, p.offset, buf);
    }

    inline bool write_boot_info(const Storage& s, const Partition& p, const BootInfo& info) noexcept {
        auto buf = util::span<const util::u8>(reinterpret_cast<const util::u8*>(&info), sizeof(BootInfo));
        return storage_write(s, p.offset, buf);
    }

    inline BootResult select_slot(const Storage& s, const BootConfig& cfg, BootInfo& info) noexcept {
        if (!read_boot_info(s, cfg.info, info)) {
            info = {};
        }
        const Partition& pa = cfg.slot_a;
        const Partition& pb = cfg.slot_b;
        const bool a_ok = verify_partition(s, pa);
        const bool b_ok = verify_partition(s, pb);

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

    inline void mark_success(const Storage& s, const BootConfig& cfg, BootInfo& info, Slot slot) noexcept {
        info.active = slot;
        info.pending = slot;
        ++info.counter;
        (void)write_boot_info(s, cfg.info, info);
    }
}
