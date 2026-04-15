export module boot_board_load;

import platform.board;

export import boot_load;

export namespace boot {
    constexpr platform::board::BootLoadKind to_board_boot_load_kind(BootLoadKind kind) noexcept {
        return kind == BootLoadKind::xip
            ? platform::board::BootLoadKind::xip
            : platform::board::BootLoadKind::copy_to_ram;
    }

    constexpr platform::board::BootLoadResolveRequest
    make_board_boot_load_resolve_request(const BootLoadPlan& load) noexcept {
        return platform::board::BootLoadResolveRequest{
            .kind = to_board_boot_load_kind(load.kind),
            .storage_payload_offset = load.storage_payload_offset,
            .storage_entry_offset = load.storage_entry_offset,
            .entry_offset = load.entry_offset,
            .payload_size = load.payload_size,
            .image_size = load.image_size,
            .image_flags = load.target.header.flags
        };
    }

    constexpr platform::board::BootLoadTransferRequest
    make_board_boot_load_transfer_request(const BootLoadedImage& image) noexcept {
        return platform::board::BootLoadTransferRequest{
            .kind = to_board_boot_load_kind(image.load.kind),
            .payload_base = image.payload_base,
            .storage_payload_offset = image.load.storage_payload_offset,
            .payload_size = image.load.payload_size,
            .image_flags = image.load.target.header.flags
        };
    }

    inline BootLoadedImage resolve_boot_loaded_image(const BootLoadPlan& load,
                                                     const platform::board::BootLoadDesc& desc) noexcept {
        BootLoadedImage image{};
        image.load = load;
        if (!image.load || !desc.resolve_payload_base) {
            return image;
        }

        const auto request = make_board_boot_load_resolve_request(image.load);
        image.payload_base = desc.resolve_payload_base(desc.ctx, request);
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

        const auto request = make_board_boot_load_transfer_request(image);
        image.payload_ready = desc.load_payload(desc.ctx, request);
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
