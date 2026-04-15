export module boot_launch;

import util.core;

export import boot_core;
export import boot_flow;
export import boot_plan;
export import boot_storage;

export namespace boot {
    struct BootTarget {
        BootPlan plan{};
        Partition partition{};
        ImageHeader header{};
        util::u32 payload_offset{0};
        util::u32 storage_entry_offset{0};
        bool header_loaded{false};

        constexpr explicit operator bool() const noexcept {
            return static_cast<bool>(plan) && header_loaded;
        }
    };

    inline BootTarget resolve_boot_target(const Storage& s, const BootConfig& cfg,
                                          BootPlan plan) noexcept {
        BootTarget target{};
        target.plan = plan;
        if (!target.plan) {
            return target;
        }

        target.partition = partition_for_slot(cfg, target.plan.boot.slot);
        if (!read_header(s, target.partition, target.header)) {
            return target;
        }
        if (target.header.magic != k_magic || target.header.version != k_version) {
            return target;
        }
        if (!image_layout_valid(target.partition, target.header)) {
            return target;
        }

        target.payload_offset =
            target.partition.offset + static_cast<util::u32>(sizeof(ImageHeader));
        target.storage_entry_offset = target.payload_offset + target.header.entry_offset;
        target.header_loaded = true;
        return target;
    }
}
