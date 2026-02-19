module;

#include <cstddef>
#include <span>

export module usb.driver;

import usb.common;

export namespace usb::driver {
    enum class EpDirection : u8 {
        out = 0,
        in = 1,
    };

    enum class EpType : u8 {
        control,
        isochronous,
        bulk,
        interrupt,
    };

    struct EpConfig {
        u8 address{0};
        EpDirection direction{EpDirection::out};
        EpType type{EpType::bulk};
        u16 max_packet_size{64};
        u8 interval{0};
    };

    struct EpCallbacks {
        void (*on_out)(void* ctx, std::span<const u8> data) noexcept { nullptr };
        void (*on_in_complete)(void* ctx, std::size_t sent, bool sent_zlp) noexcept { nullptr };
        void (*on_stall)(void* ctx) noexcept { nullptr };
    };

    struct EpDriverOps {
        bool (*open)(void* ctx, const EpConfig& cfg, EpCallbacks cb) noexcept { nullptr };
        bool (*close)(void* ctx, u8 address) noexcept { nullptr };
        bool (*send)(void* ctx, u8 address, std::span<const u8> data, bool zlp) noexcept { nullptr };
        bool (*stall)(void* ctx, u8 address) noexcept { nullptr };
    };

    struct DcdOps {
        EpDriverOps ep{};
        bool (*set_address)(void* ctx, u8 address) noexcept { nullptr };
        bool (*set_configured)(void* ctx, bool configured) noexcept { nullptr };
    };
}
