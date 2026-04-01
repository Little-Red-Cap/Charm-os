module;

#define CHARM_ALLOW_HAL 1

#include <array>
#include <cstdint>
#include <span>

#include "stm32h7xx_hal.h"

export module player.app_test_hqzy.usb_glue;

import charm.system.time;
import usb.class_msc;
import usb.common;
import usb.device_driver;
import usb.driver;
import util.core;

export namespace player::app_test_hqzy::usb_glue {
    struct UsbGlue {
        usb::driver::DcdDeviceAdapter adapter{};
        usb::driver::DcdOps dcd_ops{};
        usb::class_driver::MscBot* msc_bot{nullptr};
        const usb::class_driver::MscConfig* msc_cfg{nullptr};
        std::array<usb::driver::EpCallbacks, 16> out_cbs{};
        std::array<usb::driver::EpCallbacks, 16> in_cbs{};
        std::array<void*, 16> out_ctxs{};
        std::array<void*, 16> in_ctxs{};
        std::array<std::array<usb::u8, 64>, 16> out_bufs{};
        std::array<usb::u16, 16> out_mps{};
        PCD_HandleTypeDef* pcd{nullptr};
        util::u64 last_msc_ms{0};
    };

    namespace detail {
        inline UsbGlue* g_usb = nullptr;

        inline UsbGlue* from_ctx(void* ctx) noexcept {
            return static_cast<UsbGlue*>(ctx);
        }

        inline UsbGlue* from_pcd(PCD_HandleTypeDef* pcd) noexcept {
            return pcd ? static_cast<UsbGlue*>(pcd->pData) : nullptr;
        }

        bool ep_open(void* ctx, const usb::driver::EpConfig& cfg,
                     usb::driver::EpCallbacks cb) noexcept {
            auto* glue = from_ctx(ctx);
            if (!glue || !glue->pcd) return false;
            std::uint8_t type = EP_TYPE_BULK;
            switch (cfg.type) {
            case usb::driver::EpType::control: type = EP_TYPE_CTRL; break;
            case usb::driver::EpType::isochronous: type = EP_TYPE_ISOC; break;
            case usb::driver::EpType::bulk: type = EP_TYPE_BULK; break;
            case usb::driver::EpType::interrupt: type = EP_TYPE_INTR; break;
            }
            if (HAL_PCD_EP_Open(glue->pcd, cfg.address, cfg.max_packet_size, type) != HAL_OK) {
                return false;
            }
            const std::uint8_t ep_num = static_cast<std::uint8_t>(cfg.address & 0x0F);
            if (cfg.direction == usb::driver::EpDirection::out) {
                glue->out_cbs[ep_num] = cb;
                glue->out_mps[ep_num] = cfg.max_packet_size;
                (void)HAL_PCD_EP_Receive(glue->pcd, cfg.address,
                    glue->out_bufs[ep_num].data(),
                    glue->out_mps[ep_num]);
            } else {
                glue->in_cbs[ep_num] = cb;
            }
            return true;
        }

        bool ep_close(void* ctx, usb::u8 address) noexcept {
            auto* glue = from_ctx(ctx);
            if (!glue || !glue->pcd) return false;
            if (HAL_PCD_EP_Close(glue->pcd, address) != HAL_OK) return false;
            const std::uint8_t ep_num = static_cast<std::uint8_t>(address & 0x0F);
            if ((address & 0x80) != 0) {
                glue->in_cbs[ep_num] = {};
            } else {
                glue->out_cbs[ep_num] = {};
            }
            return true;
        }

        bool ep_send(void* ctx, usb::u8 address,
                     std::span<const usb::u8> data, bool) noexcept {
            auto* glue = from_ctx(ctx);
            if (!glue || !glue->pcd) return false;
            auto* ptr = const_cast<usb::u8*>(data.data());
            return HAL_PCD_EP_Transmit(glue->pcd, address, ptr,
                static_cast<uint16_t>(data.size())) == HAL_OK;
        }

        bool ep_stall(void* ctx, usb::u8 address) noexcept {
            auto* glue = from_ctx(ctx);
            if (!glue || !glue->pcd) return false;
            return HAL_PCD_EP_SetStall(glue->pcd, address) == HAL_OK;
        }

        bool set_address(void* ctx, usb::u8 address) noexcept {
            auto* glue = from_ctx(ctx);
            if (!glue || !glue->pcd) return false;
            return HAL_PCD_SetAddress(glue->pcd, address) == HAL_OK;
        }

        bool set_configured(void* ctx, bool configured) noexcept {
            auto* glue = from_ctx(ctx);
            if (!glue || !glue->pcd) return false;
            return configured ? (HAL_PCD_Start(glue->pcd) == HAL_OK)
                              : (HAL_PCD_Stop(glue->pcd) == HAL_OK);
        }

        bool connect(void* ctx, bool enable) noexcept {
            auto* glue = from_ctx(ctx);
            if (!glue || !glue->pcd) return false;
            return enable ? (HAL_PCD_Start(glue->pcd) == HAL_OK)
                          : (HAL_PCD_Stop(glue->pcd) == HAL_OK);
        }
    } // namespace detail

    inline void init(UsbGlue& glue, PCD_HandleTypeDef* pcd) noexcept {
        glue.pcd = pcd;
        if (glue.pcd) {
            glue.pcd->pData = &glue;
        }
        glue.dcd_ops.ep.open = &detail::ep_open;
        glue.dcd_ops.ep.close = &detail::ep_close;
        glue.dcd_ops.ep.send = &detail::ep_send;
        glue.dcd_ops.ep.stall = &detail::ep_stall;
        glue.dcd_ops.set_address = &detail::set_address;
        glue.dcd_ops.set_configured = &detail::set_configured;
        glue.dcd_ops.connect = &detail::connect;
        detail::g_usb = &glue;
    }

    inline usb::driver::DcdOps& dcd_ops(UsbGlue& glue) noexcept { return glue.dcd_ops; }
    inline usb::driver::DcdDeviceAdapter& adapter(UsbGlue& glue) noexcept { return glue.adapter; }

    inline void on_ready(void* ctx,
                         usb::class_driver::MscBot* bot,
                         const usb::class_driver::MscConfig* cfg) noexcept {
        auto* glue = static_cast<UsbGlue*>(ctx);
        if (!glue) return;
        glue->msc_bot = bot;
        glue->msc_cfg = cfg;
        for (auto& c : glue->out_ctxs) c = bot;
        for (auto& c : glue->in_ctxs) c = bot;
    }

    inline void poll_msc(UsbGlue& glue) noexcept {
        if (!glue.msc_bot || !glue.msc_cfg) return;
        if (!charm::system::time::bound()) return;
        const auto now = charm::system::time::now_ms();
        if ((now - glue.last_msc_ms) < 1u) return;
        glue.last_msc_ms = now;
        (void)usb::device::examples::send_msc_in_packet(
            glue.dcd_ops,
            glue.pcd,
            *glue.msc_bot,
            *glue.msc_cfg);
    }
} // namespace player::app_test_hqzy::usb_glue

extern "C" int charm_usb_setup_hook(PCD_HandleTypeDef* hpcd) {
    if (!hpcd) return 0;
    auto* glue = player::app_test_hqzy::usb_glue::detail::from_pcd(hpcd);
    if (!glue) return 0;
    usb::SetupPacket setup{};
    setup.bm_request_type = hpcd->Setup[0];
    setup.b_request = hpcd->Setup[1];
    setup.w_value = static_cast<usb::u16>(hpcd->Setup[2] | (hpcd->Setup[3] << 8));
    setup.w_index = static_cast<usb::u16>(hpcd->Setup[4] | (hpcd->Setup[5] << 8));
    setup.w_length = static_cast<usb::u16>(hpcd->Setup[6] | (hpcd->Setup[7] << 8));
    glue->adapter.handle_setup(setup);
    return 1;
}

extern "C" int charm_usb_data_out_hook(PCD_HandleTypeDef* hpcd, uint8_t epnum) {
    if (!hpcd) return 0;
    auto* glue = player::app_test_hqzy::usb_glue::detail::from_pcd(hpcd);
    if (!glue) return 0;
    const auto len = hpcd->OUT_ep[epnum].xfer_count;
    auto& cb = glue->out_cbs[epnum];
    if (cb.on_out && len > 0) {
        cb.on_out(glue->out_ctxs[epnum],
            std::span<const usb::u8>(glue->out_bufs[epnum].data(), len));
    }
    const auto addr = static_cast<uint8_t>(epnum & 0x0F);
    (void)HAL_PCD_EP_Receive(hpcd, addr,
        glue->out_bufs[epnum].data(),
        glue->out_mps[epnum]);
    return 1;
}

extern "C" int charm_usb_data_in_hook(PCD_HandleTypeDef* hpcd, uint8_t epnum) {
    if (!hpcd) return 0;
    auto* glue = player::app_test_hqzy::usb_glue::detail::from_pcd(hpcd);
    if (!glue) return 0;
    auto& cb = glue->in_cbs[epnum];
    if (cb.on_in_complete) {
        const auto sent = hpcd->IN_ep[epnum].xfer_count;
        cb.on_in_complete(glue->in_ctxs[epnum], sent, false);
    }
    return 1;
}

extern "C" int charm_usb_reset_hook(PCD_HandleTypeDef*) {
    auto* glue = player::app_test_hqzy::usb_glue::detail::g_usb;
    if (glue) {
        glue->adapter.handle_reset();
        return 1;
    }
    return 0;
}

extern "C" int charm_usb_suspend_hook(PCD_HandleTypeDef*) {
    auto* glue = player::app_test_hqzy::usb_glue::detail::g_usb;
    if (glue) {
        glue->adapter.handle_suspend();
        return 1;
    }
    return 0;
}

extern "C" int charm_usb_resume_hook(PCD_HandleTypeDef*) {
    auto* glue = player::app_test_hqzy::usb_glue::detail::g_usb;
    if (glue) {
        glue->adapter.handle_resume();
        return 1;
    }
    return 0;
}

extern "C" int charm_usb_connect_hook(PCD_HandleTypeDef*) {
    auto* glue = player::app_test_hqzy::usb_glue::detail::g_usb;
    if (glue) {
        glue->adapter.handle_connect(true);
        return 1;
    }
    return 0;
}

extern "C" int charm_usb_disconnect_hook(PCD_HandleTypeDef*) {
    auto* glue = player::app_test_hqzy::usb_glue::detail::g_usb;
    if (glue) {
        glue->adapter.handle_connect(false);
        return 1;
    }
    return 0;
}
