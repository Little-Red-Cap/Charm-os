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

    struct DcdDeviceCallbacks {
        void* ctx{nullptr};
        void (*on_setup)(void* ctx, const SetupPacket& setup) noexcept { nullptr };
        void (*on_out_data)(void* ctx, std::span<const u8> data) noexcept { nullptr };
        void (*on_in_complete)(void* ctx, std::size_t sent, bool sent_zlp) noexcept { nullptr };
        void (*on_reset)(void* ctx) noexcept { nullptr };
    };

    struct DcdDeviceAdapter {
        DcdDeviceCallbacks callbacks{};

        void handle_setup(const SetupPacket& setup) noexcept {
            if (callbacks.on_setup) callbacks.on_setup(callbacks.ctx, setup);
        }

        void handle_out_data(std::span<const u8> data) noexcept {
            if (callbacks.on_out_data) callbacks.on_out_data(callbacks.ctx, data);
        }

        void handle_in_complete(std::size_t sent, bool sent_zlp) noexcept {
            if (callbacks.on_in_complete) callbacks.on_in_complete(callbacks.ctx, sent, sent_zlp);
        }

        void handle_reset() noexcept {
            if (callbacks.on_reset) callbacks.on_reset(callbacks.ctx);
        }
    };
}
