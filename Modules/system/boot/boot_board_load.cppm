export module boot_board_load;

import platform.board;

export import boot_load;

export namespace boot {
    constexpr platform::board::BootLoadKind to_board_boot_load_kind(BootLoadKind kind) noexcept {
        return kind == BootLoadKind::xip
            ? platform::board::BootLoadKind::xip
            : platform::board::BootLoadKind::copy_to_ram;
    }

    inline BootLoadedImage resolve_boot_loaded_image(const BootLoadPlan& load,
                                                     const platform::board::BootLoadDesc& desc) noexcept {
        BootLoadedImage image{};
        image.load = load;
        if (!image.load || !desc.resolve_payload_base) {
            return image;
        }

        image.payload_base = desc.resolve_payload_base(
            desc.ctx,
            to_board_boot_load_kind(image.load.kind),
            image.load.storage_payload_offset,
            image.load.storage_entry_offset,
            image.load.entry_offset,
            image.load.payload_size,
            image.load.image_size,
            image.load.target.header.flags);
        if (image.payload_base == 0) {
            return image;
        }

        image.entry_addr = image.payload_base + image.load.entry_offset;
        image.address_resolved = true;
        image.payload_ready = !image.load.transfer_required;
        return image;
    }

    inline BootLoadedImage resolve_boot_loaded_image(const Storage& s, const BootConfig& cfg,
                                                     BootPlan plan,
                                                     const platform::board::BootLoadDesc& desc) noexcept {
        return resolve_boot_loaded_image(prepare_boot_load(s, cfg, plan), desc);
    }

    inline bool prepare_boot_loaded_image(BootLoadedImage& image,
                                          const platform::board::BootLoadDesc& desc) noexcept {
        if (!image.load || !image.address_resolved) {
            return false;
        }
        if (image.payload_ready) {
            return true;
        }
        if (!desc.load_payload) {
            return false;
        }

        image.payload_ready = desc.load_payload(
            desc.ctx,
            to_board_boot_load_kind(image.load.kind),
            image.payload_base,
            image.load.storage_payload_offset,
            image.load.payload_size,
            image.load.target.header.flags);
        return image.payload_ready;
    }

    inline BootLoadedImage resolve_boot_loaded_image(const BootLoadPlan& load,
                                                     const platform::board::BoardCaps& caps) noexcept {
        return resolve_boot_loaded_image(load, caps.boot_load);
    }

    inline BootLoadedImage resolve_boot_loaded_image(const Storage& s, const BootConfig& cfg,
                                                     BootPlan plan,
                                                     const platform::board::BoardCaps& caps) noexcept {
        return resolve_boot_loaded_image(s, cfg, plan, caps.boot_load);
    }

    inline bool prepare_boot_loaded_image(BootLoadedImage& image,
                                          const platform::board::BoardCaps& caps) noexcept {
        return prepare_boot_loaded_image(image, caps.boot_load);
    }
}
