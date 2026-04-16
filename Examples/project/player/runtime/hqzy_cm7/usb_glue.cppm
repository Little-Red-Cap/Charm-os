module;

#define CHARM_ALLOW_HAL 1

#include <cstdint>
#include <span>

#include "stm32h7xx_hal.h"

export module player.runtime.hqzy_cm7.usb_glue;

import charm.system.time;
import player.stm32h7.usb_glue_core;
import player.stm32h7.usb_msc_glue;
import usb.class_msc;
import usb.common;
import usb.device_driver;
import usb.driver;
import util.core;

export namespace player::app_test_hqzy::usb_glue {
    struct UsbGlue : player::stm32h7::usb_glue_core::Core {
        player::stm32h7::usb_msc_glue::State msc{};
        util::u64 last_msc_ms{0};
    };

    namespace detail {
        namespace usb_core = player::stm32h7::usb_glue_core;

        inline UsbGlue* from_ctx(void* ctx) noexcept {
            return static_cast<UsbGlue*>(ctx);
        }

        bool ep_open(void* ctx, const usb::driver::EpConfig& cfg,
                     usb::driver::EpCallbacks cb) noexcept {
            auto* glue = from_ctx(ctx);
            return glue && usb_core::ep_open(*glue, glue->pcd, cfg, cb);
        }

        bool ep_close(void* ctx, usb::u8 address) noexcept {
            auto* glue = from_ctx(ctx);
            return glue && usb_core::ep_close(*glue, glue->pcd, address);
        }

        bool ep_send(void* ctx, usb::u8 address,
                     std::span<const usb::u8> data, bool zlp) noexcept {
            auto* glue = from_ctx(ctx);
            return glue && usb_core::ep_send(*glue, glue->pcd, address, data, zlp);
        }

        bool ep_stall(void* ctx, usb::u8 address) noexcept {
            auto* glue = from_ctx(ctx);
            return glue && usb_core::ep_stall(glue->pcd, address);
        }

        bool set_address(void* ctx, usb::u8 address) noexcept {
            auto* glue = from_ctx(ctx);
            return glue && usb_core::set_address(glue->pcd, address);
        }

        bool set_configured(void* ctx, bool configured) noexcept {
            auto* glue = from_ctx(ctx);
            return glue && usb_core::set_configured(*glue, glue->pcd, configured);
        }

        bool connect(void* ctx, bool enable) noexcept {
            auto* glue = from_ctx(ctx);
            return glue && usb_core::connect(*glue, glue->pcd, enable,
                usb_core::ConnectMode::start_stop);
        }
    } // namespace detail

    inline void init(UsbGlue& glue, PCD_HandleTypeDef* pcd) noexcept {
        namespace usb_core = player::stm32h7::usb_glue_core;

        usb_core::bind_pcd(glue, pcd);
        glue.dcd_ops.ep.open = &detail::ep_open;
        glue.dcd_ops.ep.close = &detail::ep_close;
        glue.dcd_ops.ep.send = &detail::ep_send;
        glue.dcd_ops.ep.stall = &detail::ep_stall;
        glue.dcd_ops.set_address = &detail::set_address;
        glue.dcd_ops.set_configured = &detail::set_configured;
        glue.dcd_ops.connect = &detail::connect;
        usb_core::prepare_ep0(glue);
    }

    inline usb::driver::DcdOps& dcd_ops(UsbGlue& glue) noexcept { return glue.dcd_ops; }
    inline usb::driver::DcdDeviceAdapter& adapter(UsbGlue& glue) noexcept { return glue.adapter; }

    inline void on_ready(void* ctx,
                         usb::class_driver::MscBot* bot,
                         const usb::class_driver::MscConfig* cfg) noexcept {
        auto* glue = static_cast<UsbGlue*>(ctx);
        if (!glue) return;
        player::stm32h7::usb_msc_glue::set_ready(glue->msc, bot, cfg);
    }

    inline void poll_msc(UsbGlue& glue) noexcept {
        if (!glue.msc.bot || !glue.msc.cfg) return;
        if (!charm::system::time::bound()) return;
        const auto now = charm::system::time::now_ms();
        if ((now - glue.last_msc_ms) < 1u) return;
        glue.last_msc_ms = now;
        (void)player::stm32h7::usb_msc_glue::poll(
            glue,
            glue.msc,
            glue.dcd_ops,
            &glue);
    }
} // namespace player::app_test_hqzy::usb_glue

extern "C" int charm_usb_setup_hook(PCD_HandleTypeDef* hpcd) {
    namespace usb_core = player::stm32h7::usb_glue_core;

    auto* core = usb_core::from_pcd(hpcd);
    if (!core) return 0;
    const auto view = usb_core::decode_setup(hpcd);
    return usb_core::handle_setup(*core, hpcd, view);
}

extern "C" int charm_usb_data_out_hook(PCD_HandleTypeDef* hpcd, uint8_t epnum) {
    namespace usb_core = player::stm32h7::usb_glue_core;

    auto* core = usb_core::from_pcd(hpcd);
    if (!core) return 0;
    const auto event = usb_core::inspect_data_out(hpcd, epnum);
    return usb_core::handle_data_out(*core, hpcd, event);
}

extern "C" int charm_usb_data_in_hook(PCD_HandleTypeDef* hpcd, uint8_t epnum) {
    namespace usb_core = player::stm32h7::usb_glue_core;

    auto* core = usb_core::from_pcd(hpcd);
    if (!core) return 0;
    const auto event = usb_core::inspect_data_in(hpcd, epnum);
    return usb_core::handle_data_in(*core, hpcd, event);
}

extern "C" int charm_usb_reset_hook(PCD_HandleTypeDef* hpcd) {
    namespace usb_core = player::stm32h7::usb_glue_core;

    auto* core = usb_core::from_pcd(hpcd);
    if (!core) return 0;
    return usb_core::handle_reset(*core, hpcd);
}

extern "C" int charm_usb_suspend_hook(PCD_HandleTypeDef* hpcd) {
    namespace usb_core = player::stm32h7::usb_glue_core;

    auto* core = usb_core::from_pcd(hpcd);
    if (!core) return 0;
    return usb_core::handle_suspend(*core);
}

extern "C" int charm_usb_resume_hook(PCD_HandleTypeDef* hpcd) {
    namespace usb_core = player::stm32h7::usb_glue_core;

    auto* core = usb_core::from_pcd(hpcd);
    if (!core) return 0;
    return usb_core::handle_resume(*core);
}

extern "C" int charm_usb_connect_hook(PCD_HandleTypeDef* hpcd) {
    namespace usb_core = player::stm32h7::usb_glue_core;

    auto* core = usb_core::from_pcd(hpcd);
    if (!core) return 0;
    return usb_core::handle_connect(*core, true);
}

extern "C" int charm_usb_disconnect_hook(PCD_HandleTypeDef* hpcd) {
    namespace usb_core = player::stm32h7::usb_glue_core;

    auto* core = usb_core::from_pcd(hpcd);
    if (!core) return 0;
    return usb_core::handle_connect(*core, false);
}
