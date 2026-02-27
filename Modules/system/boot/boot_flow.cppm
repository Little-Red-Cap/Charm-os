module;

#include <span>
#include <array>

export module boot_flow;

import util.core;
import boot_core;
import boot_storage;

export namespace boot {
    struct BootConfig {
        Partition slot_a{};
        Partition slot_b{};
        Partition info{};
    };

    inline bool read_header(const Storage& s, const Partition& p, ImageHeader& out) noexcept {
        auto buf = std::span<util::u8>(reinterpret_cast<util::u8*>(&out), sizeof(ImageHeader));
        return storage_read(s, p.offset, buf);
    }

    inline BootStatus verify_partition_status(const Storage& s, const Partition& p) noexcept {
        ImageHeader h{};
        if (!read_header(s, p, h)) return BootStatus::io_error;
        if (h.magic != k_magic || h.version != k_version) return BootStatus::invalid;
        if (h.payload_size == 0) return BootStatus::invalid;
        const util::u32 header_size = static_cast<util::u32>(sizeof(ImageHeader));
        if (h.payload_size + header_size > p.size) return BootStatus::invalid;
        if (h.image_size < h.payload_size + header_size || h.image_size > p.size) return BootStatus::invalid;
        // TODO: validate entry_offset and handle compressed images when implemented.
        std::array<util::u8, 128> buf{};
        util::u32 crc = 0;
        util::u32 remaining = h.payload_size;
        util::u32 offset = p.offset + static_cast<util::u32>(sizeof(ImageHeader));
        while (remaining > 0) {
            const auto chunk = remaining > buf.size() ? static_cast<util::u32>(buf.size()) : remaining;
            if (!storage_read(s, offset, std::span<util::u8>(buf.data(), chunk))) return BootStatus::io_error;
            crc = crc32_update(crc, buf.data(), chunk);
            offset += chunk;
            remaining -= chunk;
        }
        return crc == h.payload_crc32 ? BootStatus::ok : BootStatus::invalid;
    }

    inline bool verify_partition(const Storage& s, const Partition& p) noexcept {
        return verify_partition_status(s, p) == BootStatus::ok;
    }

    inline util::u32 boot_info_crc(const BootInfo& info) noexcept {
        auto tmp = info;
        tmp.crc = 0;
        const auto* data = reinterpret_cast<const util::u8*>(&tmp);
        return crc32_update(0, data, sizeof(BootInfo));
    }

    inline bool boot_info_valid(const BootInfo& info) noexcept {
        if (info.magic != k_boot_info_magic) return false;
        if (info.version != k_version) return false;
        if (info.size != sizeof(BootInfo)) return false;
        return info.crc == boot_info_crc(info);
    }

    inline bool read_boot_info(const Storage& s, const Partition& p, BootInfo& info) noexcept {
        BootInfo a{};
        BootInfo b{};
        if (!storage_read(s, p.offset, std::span<util::u8>(reinterpret_cast<util::u8*>(&a), sizeof(BootInfo)))) {
            return false;
        }
        if (p.size < sizeof(BootInfo) * 2) {
            if (!boot_info_valid(a)) return false;
            info = a;
            return true;
        }
        (void)storage_read(s, p.offset + static_cast<util::u32>(sizeof(BootInfo)),
                           std::span<util::u8>(reinterpret_cast<util::u8*>(&b), sizeof(BootInfo)));
        const bool a_ok = boot_info_valid(a);
        const bool b_ok = boot_info_valid(b);
        if (a_ok && b_ok) {
            info = (b.counter >= a.counter) ? b : a;
            return true;
        }
        if (a_ok) {
            info = a;
            return true;
        }
        if (b_ok) {
            info = b;
            return true;
        }
        return false;
    }

    inline bool write_boot_info(const Storage& s, const Partition& p, const BootInfo& info) noexcept {
        BootInfo out = info;
        out.magic = k_boot_info_magic;
        out.version = k_version;
        out.size = static_cast<util::u16>(sizeof(BootInfo));
        out.crc = boot_info_crc(out);

        const util::u32 slot_size = static_cast<util::u32>(sizeof(BootInfo));
        if (p.size < slot_size) return false;
        if (p.size < slot_size * 2) {
            auto buf = std::span<const util::u8>(reinterpret_cast<const util::u8*>(&out), sizeof(BootInfo));
            return storage_write(s, p.offset, buf);
        }

        BootInfo cur{};
        const bool cur_ok = read_boot_info(s, p, cur);
        const util::u32 offset = (cur_ok && cur.counter <= out.counter)
            ? p.offset + slot_size
            : p.offset;
        auto buf = std::span<const util::u8>(reinterpret_cast<const util::u8*>(&out), sizeof(BootInfo));
        return storage_write(s, offset, buf);
    }

    inline BootResult select_slot(const Storage& s, const BootConfig& cfg, BootInfo& info) noexcept {
        if (!read_boot_info(s, cfg.info, info)) {
            info = {};
        }
        const Partition& pa = cfg.slot_a;
        const Partition& pb = cfg.slot_b;
        const auto a_status = verify_partition_status(s, pa);
        const auto b_status = verify_partition_status(s, pb);
        const bool a_ok = a_status == BootStatus::ok;
        const bool b_ok = b_status == BootStatus::ok;

        auto pick = [&](Slot slot) -> BootResult {
            return {BootStatus::ok, slot};
        };

        if (info.pending == Slot::a && a_ok) return pick(Slot::a);
        if (info.pending == Slot::b && b_ok) return pick(Slot::b);
        if (info.active == Slot::a && a_ok) return pick(Slot::a);
        if (info.active == Slot::b && b_ok) return pick(Slot::b);
        if (a_ok) return pick(Slot::a);
        if (b_ok) return pick(Slot::b);
        if (a_status == BootStatus::io_error || b_status == BootStatus::io_error) {
            return {BootStatus::io_error, Slot::a};
        }
        return {BootStatus::invalid, Slot::a};
    }

    inline bool mark_success(const Storage& s, const BootConfig& cfg, BootInfo& info, Slot slot) noexcept {
        info.active = slot;
        info.pending = slot;
        ++info.counter;
        return write_boot_info(s, cfg.info, info);
    }
}
