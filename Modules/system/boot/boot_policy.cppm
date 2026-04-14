module;

#include <span>
#include <array>

export module boot_policy;

import util.core;
import boot_core;
import boot_storage;
import boot_flow;

export namespace boot {
    struct Policy {
        util::u32 min_version{0};
        util::u32 sign_key{0};
        bool require_signature{false};
        using SignatureVerify = bool (*)(const ImageHeader&, util::u32 crc, const void* ctx) noexcept;
        SignatureVerify verify{nullptr};
        const void* verify_ctx{nullptr};
    };

    // NOTE: This is a weak checksum-based signature and is NOT secure.
    inline util::u32 calc_signature(util::u32 key, const util::u8* data, util::usize len) noexcept {
        util::u32 crc = key;
        return crc32_update(crc, data, len);
    }

    inline bool verify_signature(const ImageHeader& h, const Policy& policy, util::u32 crc) noexcept {
        if (policy.verify) {
            return policy.verify(h, crc, policy.verify_ctx);
        }
        const util::u32 sig = calc_signature(policy.sign_key,
            reinterpret_cast<const util::u8*>(&crc), sizeof(crc));
        return h.signature == sig;
    }

    inline bool verify_version(const ImageHeader& h, const Policy& p, const BootInfo& info) noexcept {
        if (h.image_version < h.min_version) return false;
        if (h.image_version < p.min_version) return false;
        if (h.image_version < info.min_version) return false;
        return true;
    }

    inline BootStatus verify_partition_policy_status(const Storage& s, const Partition& p,
                                                     const Policy& policy, const BootInfo& info) noexcept {
        ImageHeader h{};
        if (!read_header(s, p, h)) return BootStatus::io_error;
        if (h.magic != k_magic || h.version != k_version) return BootStatus::invalid;
        if (h.payload_size == 0) return BootStatus::invalid;
        const util::u32 header_size = static_cast<util::u32>(sizeof(ImageHeader));
        if (h.payload_size + header_size > p.size) return BootStatus::invalid;
        if (h.image_size < h.payload_size + header_size || h.image_size > p.size) return BootStatus::invalid;
        if (!verify_version(h, policy, info)) return BootStatus::invalid;

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
        if (crc != h.payload_crc32) return BootStatus::invalid;
        // require_signature has priority; otherwise honor the signed_image flag.
        if (policy.require_signature || (h.flags & static_cast<util::u16>(ImageFlags::signed_image)) != 0) {
            if (!verify_signature(h, policy, crc)) return BootStatus::invalid;
        }
        return BootStatus::ok;
    }

    inline bool verify_partition_policy(const Storage& s, const Partition& p,
                                        const Policy& policy, const BootInfo& info) noexcept {
        return verify_partition_policy_status(s, p, policy, info) == BootStatus::ok;
    }

    inline BootResult select_slot_policy(const Storage& s, const BootConfig& cfg,
                                         BootInfo& info, const Policy& policy) noexcept {
        if (!read_boot_info(s, cfg.info, info)) {
            info = {};
        }
        const auto a_status = verify_partition_policy_status(s, cfg.slot_a, policy, info);
        const auto b_status = verify_partition_policy_status(s, cfg.slot_b, policy, info);
        return select_slot_candidate(info, a_status, b_status).boot;
    }
}
