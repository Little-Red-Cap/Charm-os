export module boot_load;

import util.core;

export import boot_core;
export import boot_launch;
export import boot_storage;

export namespace boot {
    enum class BootLoadKind : util::u8 {
        copy_to_ram = 0,
        xip
    };

    struct BootLoadPlan {
        BootTarget target{};
        BootLoadKind kind{BootLoadKind::copy_to_ram};
        util::u32 storage_payload_offset{0};
        util::u32 storage_entry_offset{0};
        util::u32 entry_offset{0};
        util::u32 payload_size{0};
        util::u32 image_size{0};
        bool transfer_required{true};

        constexpr explicit operator bool() const noexcept {
            return static_cast<bool>(target);
        }
    };

    struct BootLoadedImage {
        BootLoadPlan load{};
        util::usize payload_base{0};
        util::usize entry_addr{0};
        bool address_resolved{false};
        bool payload_ready{false};

        constexpr explicit operator bool() const noexcept {
            return static_cast<bool>(load) && address_resolved && payload_ready;
        }
    };

    constexpr BootLoadKind select_boot_load_kind(const ImageHeader& header) noexcept {
        return image_has_flag(header, ImageFlags::xip_payload)
            ? BootLoadKind::xip
            : BootLoadKind::copy_to_ram;
    }

    inline BootLoadPlan make_boot_load_plan(BootTarget target) noexcept {
        BootLoadPlan load{};
        load.target = target;
        if (!load.target) {
            return load;
        }

        load.kind = select_boot_load_kind(load.target.header);
        load.storage_payload_offset = load.target.payload_offset;
        load.storage_entry_offset = load.target.storage_entry_offset;
        load.entry_offset = load.target.header.entry_offset;
        load.payload_size = load.target.header.payload_size;
        load.image_size = load.target.header.image_size;
        load.transfer_required = load.kind != BootLoadKind::xip;
        return load;
    }

    inline BootLoadPlan prepare_boot_load(const Storage& s, const BootConfig& cfg,
                                          BootPlan plan) noexcept {
        return make_boot_load_plan(resolve_boot_target(s, cfg, plan));
    }
}
