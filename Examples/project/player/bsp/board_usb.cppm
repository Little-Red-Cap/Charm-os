module;

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

#include "stm32h7xx_hal.h"
#include "stm32h7xx_hal_pcd.h"
#include "usbd_def.h"

export module player.stm32h7.board_usb;

import usb.class_msc;
import usb.common;
import usb.device_driver;
import usb.driver;

extern "C" {
    extern PCD_HandleTypeDef hpcd_USB_OTG_FS;
}

export namespace player::stm32h7::board {
    usb::driver::DcdDeviceAdapter& usb_adapter() noexcept;
    usb::driver::DcdOps& usb_dcd_ops() noexcept;

    void usb_set_ready(void* ctx,
                       usb::class_driver::MscBot* bot,
                       const usb::class_driver::MscConfig* cfg) noexcept;
    void usb_poll_msc(PCD_HandleTypeDef* pcd) noexcept;
}

namespace {
    usb::driver::DcdDeviceAdapter g_usb_adapter{};
    usb::driver::DcdOps g_usb_dcd_ops{};
    usb::class_driver::MscBot* g_msc_bot = nullptr;
    const usb::class_driver::MscConfig* g_msc_cfg = nullptr;
    std::array<usb::driver::EpCallbacks, 16> g_usb_out_cbs{};
    std::array<usb::driver::EpCallbacks, 16> g_usb_in_cbs{};
    std::array<void*, 16> g_usb_out_ctx{};
    std::array<void*, 16> g_usb_in_ctx{};
    std::array<std::array<usb::u8, 64>, 16> g_usb_out_bufs{};
    std::array<usb::u16, 16> g_usb_out_mps{};

    inline PCD_HandleTypeDef* usb_pcd(void* ctx) noexcept {
        return static_cast<PCD_HandleTypeDef*>(ctx);
    }

    void usb_set_ready(void*, usb::class_driver::MscBot* bot,
                       const usb::class_driver::MscConfig* cfg) noexcept {
        g_msc_bot = bot;
        g_msc_cfg = cfg;
        if (bot && cfg) {
            const std::uint8_t out_ep = static_cast<std::uint8_t>(cfg->ep_out & 0x0F);
            const std::uint8_t in_ep = static_cast<std::uint8_t>(cfg->ep_in & 0x0F);
            g_usb_out_ctx[out_ep] = bot;
            g_usb_in_ctx[in_ep] = bot;
        }
    }

    bool usb_ep_open(void* ctx, const usb::driver::EpConfig& cfg,
                     usb::driver::EpCallbacks cb) noexcept {
        auto* pcd = usb_pcd(ctx);
        if (!pcd) return false;
        std::uint8_t type = USBD_EP_TYPE_BULK;
        switch (cfg.type) {
        case usb::driver::EpType::control: type = USBD_EP_TYPE_CTRL; break;
        case usb::driver::EpType::isochronous: type = USBD_EP_TYPE_ISOC; break;
        case usb::driver::EpType::bulk: type = USBD_EP_TYPE_BULK; break;
        case usb::driver::EpType::interrupt: type = USBD_EP_TYPE_INTR; break;
        }
        if (HAL_PCD_EP_Open(pcd, cfg.address, cfg.max_packet_size, type) != HAL_OK) {
            return false;
        }
        const std::uint8_t ep_num = static_cast<std::uint8_t>(cfg.address & 0x0F);
        if (cfg.direction == usb::driver::EpDirection::out) {
            g_usb_out_cbs[ep_num] = cb;
            g_usb_out_ctx[ep_num] = (g_msc_bot && g_msc_cfg && cfg.address == g_msc_cfg->ep_out)
                ? static_cast<void*>(g_msc_bot)
                : nullptr;
            g_usb_out_mps[ep_num] = cfg.max_packet_size;
            (void)HAL_PCD_EP_Receive(pcd, cfg.address,
                g_usb_out_bufs[ep_num].data(),
                g_usb_out_mps[ep_num]);
        } else {
            g_usb_in_cbs[ep_num] = cb;
            g_usb_in_ctx[ep_num] = (g_msc_bot && g_msc_cfg && cfg.address == g_msc_cfg->ep_in)
                ? static_cast<void*>(g_msc_bot)
                : nullptr;
        }
        return true;
    }

    bool usb_ep_close(void* ctx, usb::u8 address) noexcept {
        auto* pcd = usb_pcd(ctx);
        if (!pcd) return false;
        if (HAL_PCD_EP_Close(pcd, address) != HAL_OK) return false;
        const std::uint8_t ep_num = static_cast<std::uint8_t>(address & 0x0F);
        if ((address & 0x80) != 0) {
            g_usb_in_cbs[ep_num] = {};
            g_usb_in_ctx[ep_num] = nullptr;
        } else {
            g_usb_out_cbs[ep_num] = {};
            g_usb_out_ctx[ep_num] = nullptr;
        }
        return true;
    }

    bool usb_ep_send(void* ctx, usb::u8 address,
                     std::span<const usb::u8> data, bool) noexcept {
        auto* pcd = usb_pcd(ctx);
        if (!pcd) return false;
        auto* ptr = const_cast<usb::u8*>(data.data());
        return HAL_PCD_EP_Transmit(pcd, address, ptr,
            static_cast<uint16_t>(data.size())) == HAL_OK;
    }

    bool usb_ep_stall(void* ctx, usb::u8 address) noexcept {
        auto* pcd = usb_pcd(ctx);
        if (!pcd) return false;
        return HAL_PCD_EP_SetStall(pcd, address) == HAL_OK;
    }

    bool usb_set_address(void* ctx, usb::u8 address) noexcept {
        auto* pcd = usb_pcd(ctx);
        if (!pcd) return false;
        return HAL_PCD_SetAddress(pcd, address) == HAL_OK;
    }

    bool usb_set_configured(void* ctx, bool configured) noexcept {
        auto* pcd = usb_pcd(ctx);
        if (!pcd) return false;
        return configured ? (HAL_PCD_Start(pcd) == HAL_OK)
                          : (HAL_PCD_Stop(pcd) == HAL_OK);
    }

    bool usb_connect(void* ctx, bool enable) noexcept {
        auto* pcd = usb_pcd(ctx);
        if (!pcd) return false;
        return enable ? (HAL_PCD_Start(pcd) == HAL_OK)
                      : (HAL_PCD_Stop(pcd) == HAL_OK);
    }
}

#if defined(__GNUC__)
#define CHARM_WEAK __attribute__((weak))
#else
#define CHARM_WEAK
#endif

export namespace player::stm32h7::board {
    usb::driver::DcdDeviceAdapter& usb_adapter() noexcept {
        return g_usb_adapter;
    }

    usb::driver::DcdOps& usb_dcd_ops() noexcept {
        static bool inited = false;
        if (!inited) {
            g_usb_dcd_ops.ep.open = &usb_ep_open;
            g_usb_dcd_ops.ep.close = &usb_ep_close;
            g_usb_dcd_ops.ep.send = &usb_ep_send;
            g_usb_dcd_ops.ep.stall = &usb_ep_stall;
            g_usb_dcd_ops.set_address = &usb_set_address;
            g_usb_dcd_ops.set_configured = &usb_set_configured;
            g_usb_dcd_ops.connect = &usb_connect;
            inited = true;
        }
        return g_usb_dcd_ops;
    }

    void usb_set_ready(void* ctx,
                       usb::class_driver::MscBot* bot,
                       const usb::class_driver::MscConfig* cfg) noexcept {
        ::usb_set_ready(ctx, bot, cfg);
    }

    void usb_poll_msc(PCD_HandleTypeDef* pcd) noexcept {
        if (!g_msc_bot || !g_msc_cfg || !pcd) return;
        (void)usb::device::examples::send_msc_in_packet(
            g_usb_dcd_ops, pcd, *g_msc_bot, *g_msc_cfg);
    }
}

extern "C" CHARM_WEAK void OTG_FS_IRQHandler(void) {
    HAL_PCD_IRQHandler(&hpcd_USB_OTG_FS);
}

extern "C" CHARM_WEAK void HAL_PCD_SetupStageCallback(PCD_HandleTypeDef* hpcd) {
    if (!hpcd) return;
    usb::SetupPacket setup{};
    setup.bm_request_type = hpcd->Setup[0];
    setup.b_request = hpcd->Setup[1];
    setup.w_value = static_cast<usb::u16>(hpcd->Setup[2] | (hpcd->Setup[3] << 8));
    setup.w_index = static_cast<usb::u16>(hpcd->Setup[4] | (hpcd->Setup[5] << 8));
    setup.w_length = static_cast<usb::u16>(hpcd->Setup[6] | (hpcd->Setup[7] << 8));
    g_usb_adapter.handle_setup(setup);
}

extern "C" CHARM_WEAK void HAL_PCD_DataOutStageCallback(PCD_HandleTypeDef* hpcd, uint8_t epnum) {
    if (!hpcd) return;
    const auto len = hpcd->OUT_ep[epnum].xfer_count;
    auto& cb = g_usb_out_cbs[epnum];
    if (cb.on_out && len > 0) {
        cb.on_out(g_usb_out_ctx[epnum],
            std::span<const usb::u8>(g_usb_out_bufs[epnum].data(), len));
    }
    const auto addr = static_cast<uint8_t>(epnum & 0x0F);
    (void)HAL_PCD_EP_Receive(hpcd, addr,
        g_usb_out_bufs[epnum].data(),
        g_usb_out_mps[epnum]);
}

extern "C" CHARM_WEAK void HAL_PCD_DataInStageCallback(PCD_HandleTypeDef* hpcd, uint8_t epnum) {
    if (!hpcd) return;
    auto& cb = g_usb_in_cbs[epnum];
    if (cb.on_in_complete) {
        const auto sent = hpcd->IN_ep[epnum].xfer_count;
        cb.on_in_complete(g_usb_in_ctx[epnum], sent, false);
    }
}

extern "C" CHARM_WEAK void HAL_PCD_ResetCallback(PCD_HandleTypeDef*) {
    g_usb_adapter.handle_reset();
}

extern "C" CHARM_WEAK void HAL_PCD_SuspendCallback(PCD_HandleTypeDef*) {
    g_usb_adapter.handle_suspend();
}

extern "C" CHARM_WEAK void HAL_PCD_ResumeCallback(PCD_HandleTypeDef*) {
    g_usb_adapter.handle_resume();
}

extern "C" CHARM_WEAK void HAL_PCD_ConnectCallback(PCD_HandleTypeDef*) {
    g_usb_adapter.handle_connect(true);
}

extern "C" CHARM_WEAK void HAL_PCD_DisconnectCallback(PCD_HandleTypeDef*) {
    g_usb_adapter.handle_connect(false);
}
