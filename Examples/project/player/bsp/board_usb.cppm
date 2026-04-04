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
    USBD_StatusTypeDef USBD_LL_Init(USBD_HandleTypeDef* pdev);
    void MX_USB_OTG_FS_PCD_Init(void);
}

export namespace player::stm32h7::board {
    usb::driver::DcdDeviceAdapter& usb_adapter() noexcept;
    usb::driver::DcdOps& usb_dcd_ops() noexcept;

    void usb_hw_init() noexcept;
    void usb_enable_hooks(bool enable) noexcept;
    void usb_set_ready(void* ctx,
                       usb::class_driver::MscBot* bot,
                       const usb::class_driver::MscConfig* cfg) noexcept;
    void usb_poll_msc(PCD_HandleTypeDef* pcd) noexcept;
    void usb_init_early(bool use_st_stack) noexcept;
    struct UsbDiag {
        std::uint32_t setup_calls;
        std::uint32_t out0_calls;
        std::uint32_t in0_calls;
        std::uint32_t out1_calls;
        std::uint32_t in1_calls;
        std::uint32_t reset_calls;
        std::uint32_t connect_calls;
        std::uint32_t set_cfg_calls;
        std::uint8_t set_cfg_last;
        std::uint8_t set_cfg_last_ok;
        std::uint8_t set_cfg_open_out_ok;
        std::uint8_t set_cfg_open_in_ok;
        std::uint8_t set_cfg_arm_out_ok;
        std::uint32_t set_addr_calls;
        std::uint8_t set_addr_last;
        std::uint8_t set_addr_last_ok;
        std::uint32_t set_addr_nonzero_calls;
        std::uint8_t set_addr_last_nonzero;
        std::uint32_t class_setup_calls;
        std::uint8_t class_last_bm;
        std::uint8_t class_last_b;
        std::uint16_t class_last_wv;
        std::uint16_t class_last_wl;
        std::uint32_t ep0_in_calls;
        std::uint32_t ep0_in_fail;
        std::uint32_t ep0_in_bytes;
        std::uint32_t ep0_in_zlp;
        std::uint32_t ep0_last_len;
        std::uint8_t ep0_last_zlp;
        std::array<std::uint8_t, 8> setup_raw;
        std::array<std::uint8_t, 4> setup_bm{};
        std::array<std::uint8_t, 4> setup_b{};
        std::array<std::uint16_t, 4> setup_wv{};
        std::array<std::uint16_t, 4> setup_wl{};
        std::uint8_t setup_hist_count{0};
        std::uint8_t setup_hist_head{0};
        std::uint8_t bm_request_type;
        std::uint8_t b_request;
        std::uint16_t w_value;
        std::uint16_t w_index;
        std::uint16_t w_length;
    };
    struct UsbHwDiag {
        std::uint32_t rcc_usb_src;
        std::uint32_t usb_fs_clk_en;
        std::uint32_t gpioa_moder;
        std::uint32_t gpioa_afr0;
        std::uint32_t gpioa_afr1;
        std::uint32_t gpioa_pupd;
        std::uint32_t pin_dm;
        std::uint32_t pin_dp;
    };
    UsbDiag usb_diag_snapshot() noexcept;
    UsbHwDiag usb_hw_diag_snapshot() noexcept;
}

namespace {
    usb::driver::DcdDeviceAdapter g_usb_adapter{};
    usb::driver::DcdOps g_usb_dcd_ops{};
    usb::class_driver::MscBot* g_msc_bot = nullptr;
    const usb::class_driver::MscConfig* g_msc_cfg = nullptr;
    bool g_msc_eps_opened = false;
    std::array<usb::driver::EpCallbacks, 16> g_usb_out_cbs{};
    std::array<usb::driver::EpCallbacks, 16> g_usb_in_cbs{};
    std::array<void*, 16> g_usb_out_ctx{};
    std::array<void*, 16> g_usb_in_ctx{};
    std::array<std::array<usb::u8, 64>, 16> g_usb_out_bufs{};
    std::array<usb::u16, 16> g_usb_out_mps{};
    bool g_usb_hooks_enabled = false;
    bool g_usb_ep0_prepared = false;
    std::uint32_t g_setup_calls = 0;
    std::uint32_t g_out0_calls = 0;
    std::uint32_t g_in0_calls = 0;
    std::uint32_t g_out1_calls = 0;
    std::uint32_t g_in1_calls = 0;
    std::uint32_t g_reset_calls = 0;
    std::uint32_t g_connect_calls = 0;
    std::uint32_t g_set_cfg_calls = 0;
    std::uint8_t g_set_cfg_last = 0;
    std::uint8_t g_set_cfg_last_ok = 0;
    std::uint8_t g_set_cfg_open_out_ok = 0;
    std::uint8_t g_set_cfg_open_in_ok = 0;
    std::uint8_t g_set_cfg_arm_out_ok = 0;
    std::uint32_t g_set_addr_calls = 0;
    std::uint8_t g_set_addr_last = 0;
    std::uint8_t g_set_addr_last_ok = 0;
    std::uint32_t g_set_addr_nonzero_calls = 0;
    std::uint8_t g_set_addr_last_nonzero = 0;
    std::uint32_t g_class_setup_calls = 0;
    std::uint8_t g_class_last_bm = 0;
    std::uint8_t g_class_last_b = 0;
    std::uint16_t g_class_last_wv = 0;
    std::uint16_t g_class_last_wl = 0;
    std::uint32_t g_ep0_in_calls = 0;
    std::uint32_t g_ep0_in_fail = 0;
    std::uint32_t g_ep0_in_bytes = 0;
    std::uint32_t g_ep0_in_zlp = 0;
    std::uint32_t g_ep0_last_len = 0;
    std::uint8_t g_ep0_last_zlp = 0;
    std::array<std::uint8_t, 8> g_setup_raw{};
    std::array<std::uint8_t, 4> g_setup_hist_bm{};
    std::array<std::uint8_t, 4> g_setup_hist_b{};
    std::array<std::uint16_t, 4> g_setup_hist_wv{};
    std::array<std::uint16_t, 4> g_setup_hist_wl{};
    std::uint8_t g_setup_hist_count = 0;
    std::uint8_t g_setup_hist_head = 0;
    std::uint8_t g_last_bm = 0;
    std::uint8_t g_last_b = 0;
    std::uint16_t g_last_w_value = 0;
    std::uint16_t g_last_w_index = 0;
    std::uint16_t g_last_w_length = 0;

    inline PCD_HandleTypeDef* usb_pcd(void* ctx) noexcept {
        return static_cast<PCD_HandleTypeDef*>(ctx);
    }

    void usb_prepare_ep0(PCD_HandleTypeDef* pcd) noexcept {
        if (!pcd || g_usb_ep0_prepared) return;
        const std::uint8_t ep0_mps = 64;
        g_usb_out_mps[0] = ep0_mps;
        (void)HAL_PCD_EP_Open(pcd, 0x00, ep0_mps, USBD_EP_TYPE_CTRL);
        (void)HAL_PCD_EP_Open(pcd, 0x80, ep0_mps, USBD_EP_TYPE_CTRL);
        (void)HAL_PCD_EP_Receive(pcd, 0x00, g_usb_out_bufs[0].data(), g_usb_out_mps[0]);
        g_usb_ep0_prepared = true;
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
                     std::span<const usb::u8> data, bool zlp) noexcept {
        auto* pcd = usb_pcd(ctx);
        if (!pcd) return false;
        const bool is_ep0_in = (address == 0x80);
        if (address == 0x80) {
            g_ep0_in_calls++;
            g_ep0_in_bytes += static_cast<std::uint32_t>(data.size());
            g_ep0_last_len = static_cast<std::uint32_t>(data.size());
            g_ep0_last_zlp = zlp ? 1u : 0u;
            if (data.empty()) {
                g_ep0_in_zlp++;
            }
        }
        auto* ptr = const_cast<usb::u8*>(data.data());
        const auto ok = HAL_PCD_EP_Transmit(pcd, address, ptr,
            static_cast<uint16_t>(data.size())) == HAL_OK;
        if (!ok && is_ep0_in) {
            g_ep0_in_fail++;
        }
        return ok;
    }

    bool usb_ep_stall(void* ctx, usb::u8 address) noexcept {
        auto* pcd = usb_pcd(ctx);
        if (!pcd) return false;
        return HAL_PCD_EP_SetStall(pcd, address) == HAL_OK;
    }

    bool usb_set_address(void* ctx, usb::u8 address) noexcept {
        auto* pcd = usb_pcd(ctx);
        if (!pcd) return false;
        const auto ok = (HAL_PCD_SetAddress(pcd, address) == HAL_OK);
        g_set_addr_calls++;
        g_set_addr_last = address;
        g_set_addr_last_ok = ok ? 1u : 0u;
        if (address != 0u) {
            g_set_addr_nonzero_calls++;
            g_set_addr_last_nonzero = address;
        }
        return ok;
    }

    bool usb_set_configured(void* ctx, bool configured) noexcept {
        auto* pcd = usb_pcd(ctx);
        if (!pcd) return false;
        g_set_cfg_calls++;
        g_set_cfg_last = configured ? 1u : 0u;
        g_set_cfg_last_ok = 0;
        g_set_cfg_open_out_ok = 0;
        g_set_cfg_open_in_ok = 0;
        g_set_cfg_arm_out_ok = 0;
        if (!configured) {
            if (g_msc_cfg) {
                (void)HAL_PCD_EP_Close(pcd, g_msc_cfg->ep_out);
                (void)HAL_PCD_EP_Close(pcd, g_msc_cfg->ep_in);
            }
            g_msc_eps_opened = false;
            g_set_cfg_last_ok = 1;
            return true;
        }
        if (!g_msc_cfg) {
            g_set_cfg_last_ok = 1;
            return true;
        }
        if (!g_msc_eps_opened) {
            usb::driver::EpConfig out{};
            out.address = g_msc_cfg->ep_out;
            out.direction = usb::driver::EpDirection::out;
            out.type = usb::driver::EpType::bulk;
            out.max_packet_size = g_msc_cfg->ep_mps;
            usb::driver::EpConfig in{};
            in.address = g_msc_cfg->ep_in;
            in.direction = usb::driver::EpDirection::in;
            in.type = usb::driver::EpType::bulk;
            in.max_packet_size = g_msc_cfg->ep_mps;
            const bool out_ok = usb_ep_open(pcd, out, g_usb_out_cbs[g_msc_cfg->ep_out & 0x0F]);
            const bool in_ok = usb_ep_open(pcd, in, g_usb_in_cbs[g_msc_cfg->ep_in & 0x0F]);
            g_set_cfg_open_out_ok = out_ok ? 1u : 0u;
            g_set_cfg_open_in_ok = in_ok ? 1u : 0u;
            g_msc_eps_opened = out_ok && in_ok;
            if (g_msc_eps_opened) {
                const auto ep = static_cast<std::uint8_t>(g_msc_cfg->ep_out & 0x0F);
                g_usb_out_mps[ep] = g_msc_cfg->ep_mps;
                const auto ok = HAL_PCD_EP_Receive(pcd, g_msc_cfg->ep_out,
                    g_usb_out_bufs[ep].data(),
                    g_usb_out_mps[ep]) == HAL_OK;
                g_set_cfg_arm_out_ok = ok ? 1u : 0u;
                g_set_cfg_last_ok = ok ? 1u : 0u;
            } else {
                g_set_cfg_last_ok = 0;
            }
        } else {
            const auto ep = static_cast<std::uint8_t>(g_msc_cfg->ep_out & 0x0F);
            g_usb_out_mps[ep] = g_msc_cfg->ep_mps;
            const auto ok = HAL_PCD_EP_Receive(pcd, g_msc_cfg->ep_out,
                g_usb_out_bufs[ep].data(),
                g_usb_out_mps[ep]) == HAL_OK;
            g_set_cfg_arm_out_ok = ok ? 1u : 0u;
            g_set_cfg_last_ok = ok ? 1u : 0u;
        }
        return true;
    }

    bool usb_connect(void* ctx, bool enable) noexcept {
        auto* pcd = usb_pcd(ctx);
        if (!pcd) return false;
        if (enable) {
            usb_prepare_ep0(pcd);
            return HAL_PCD_DevConnect(pcd) == HAL_OK;
        }
        return HAL_PCD_DevDisconnect(pcd) == HAL_OK;
    }
}

#if defined(__GNUC__)
#define CHARM_WEAK __attribute__((weak))
#else
#define CHARM_WEAK
#endif

extern "C" CHARM_WEAK void MX_USB_OTG_FS_PCD_Init(void) {
}

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

    void usb_hw_init() noexcept {
        static bool inited = false;
        static USBD_HandleTypeDef g_usb_ll{};
        if (inited) return;
        inited = true;
        g_usb_out_mps[0] = 64;
        g_usb_ll.id = DEVICE_FS;
        (void)USBD_LL_Init(&g_usb_ll);
    }

    void usb_init_early(bool use_st_stack) noexcept {
        if (use_st_stack) {
            MX_USB_OTG_FS_PCD_Init();
            return;
        }
        usb_hw_init();
    }

    void usb_poll_msc(PCD_HandleTypeDef* pcd) noexcept {
        if (!g_usb_hooks_enabled || !g_msc_bot || !g_msc_cfg || !pcd) return;
        if (g_msc_bot->take_out_rearm()) {
            const auto ep = static_cast<std::uint8_t>(g_msc_cfg->ep_out & 0x0F);
            g_usb_out_mps[ep] = g_msc_cfg->ep_mps;
            (void)HAL_PCD_EP_Receive(pcd, g_msc_cfg->ep_out,
                g_usb_out_bufs[ep].data(), g_usb_out_mps[ep]);
        }
        if (g_msc_bot->take_stall_in()) {
            (void)HAL_PCD_EP_SetStall(pcd, g_msc_cfg->ep_in);
            return;
        }
        if (g_msc_bot->take_stall_out()) {
            (void)HAL_PCD_EP_SetStall(pcd, g_msc_cfg->ep_out);
            return;
        }
        (void)usb::device::examples::send_msc_in_packet(
            g_usb_dcd_ops, pcd, *g_msc_bot, *g_msc_cfg);
    }

    void usb_enable_hooks(bool enable) noexcept {
        g_usb_hooks_enabled = enable;
    }

    UsbDiag usb_diag_snapshot() noexcept {
        return UsbDiag{
            g_setup_calls,
            g_out0_calls,
            g_in0_calls,
            g_out1_calls,
            g_in1_calls,
            g_reset_calls,
            g_connect_calls,
            g_set_cfg_calls,
            g_set_cfg_last,
            g_set_cfg_last_ok,
            g_set_cfg_open_out_ok,
            g_set_cfg_open_in_ok,
            g_set_cfg_arm_out_ok,
            g_set_addr_calls,
            g_set_addr_last,
            g_set_addr_last_ok,
            g_set_addr_nonzero_calls,
            g_set_addr_last_nonzero,
            g_class_setup_calls,
            g_class_last_bm,
            g_class_last_b,
            g_class_last_wv,
            g_class_last_wl,
            g_ep0_in_calls,
            g_ep0_in_fail,
            g_ep0_in_bytes,
            g_ep0_in_zlp,
            g_ep0_last_len,
            g_ep0_last_zlp,
            g_setup_raw,
            g_setup_hist_bm,
            g_setup_hist_b,
            g_setup_hist_wv,
            g_setup_hist_wl,
            g_setup_hist_count,
            g_setup_hist_head,
            g_last_bm,
            g_last_b,
            g_last_w_value,
            g_last_w_index,
            g_last_w_length
        };
    }

    UsbHwDiag usb_hw_diag_snapshot() noexcept {
        UsbHwDiag diag{};
        diag.rcc_usb_src = static_cast<std::uint32_t>(__HAL_RCC_GET_USB_SOURCE());
        diag.usb_fs_clk_en = (__HAL_RCC_USB2_OTG_FS_IS_CLK_ENABLED() != 0u) ? 1u : 0u;
        diag.gpioa_moder = static_cast<std::uint32_t>(GPIOA->MODER);
        diag.gpioa_afr0 = static_cast<std::uint32_t>(GPIOA->AFR[0]);
        diag.gpioa_afr1 = static_cast<std::uint32_t>(GPIOA->AFR[1]);
        diag.gpioa_pupd = static_cast<std::uint32_t>(GPIOA->PUPDR);
        diag.pin_dm = static_cast<std::uint32_t>(HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_11));
        diag.pin_dp = static_cast<std::uint32_t>(HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_12));
        return diag;
    }
}

extern "C" CHARM_WEAK void OTG_FS_IRQHandler(void) {
    HAL_PCD_IRQHandler(&hpcd_USB_OTG_FS);
}

extern "C" CHARM_WEAK void HAL_PCD_SetupStageCallback(PCD_HandleTypeDef* hpcd) {
    if (!hpcd) return;
    if (g_usb_hooks_enabled) {
        auto* setup_bytes = reinterpret_cast<std::uint8_t*>(hpcd->Setup);
        for (std::size_t i = 0; i < g_setup_raw.size(); ++i) {
            g_setup_raw[i] = setup_bytes[i];
        }
        usb::SetupPacket setup{};
        setup.bm_request_type = setup_bytes[0];
        setup.b_request = setup_bytes[1];
        setup.w_value = static_cast<usb::u16>(setup_bytes[2] | (setup_bytes[3] << 8));
        setup.w_index = static_cast<usb::u16>(setup_bytes[4] | (setup_bytes[5] << 8));
        setup.w_length = static_cast<usb::u16>(setup_bytes[6] | (setup_bytes[7] << 8));
        const std::uint8_t idx = g_setup_hist_head;
        g_setup_hist_bm[idx] = setup.bm_request_type;
        g_setup_hist_b[idx] = setup.b_request;
        g_setup_hist_wv[idx] = setup.w_value;
        g_setup_hist_wl[idx] = setup.w_length;
        g_setup_hist_head = static_cast<std::uint8_t>((idx + 1u) % g_setup_hist_bm.size());
        if (g_setup_hist_count < g_setup_hist_bm.size()) {
            g_setup_hist_count++;
        }
        g_setup_calls++;
        g_last_bm = setup.bm_request_type;
        g_last_b = setup.b_request;
        g_last_w_value = setup.w_value;
        g_last_w_index = setup.w_index;
        g_last_w_length = setup.w_length;
        if (usb::request_type(setup.bm_request_type) == usb::RequestType::class_request) {
            g_class_setup_calls++;
            g_class_last_bm = setup.bm_request_type;
            g_class_last_b = setup.b_request;
            g_class_last_wv = setup.w_value;
            g_class_last_wl = setup.w_length;
        }
        g_usb_adapter.handle_setup(setup);
        if ((setup.bm_request_type & 0x80u) == 0u && setup.w_length > 0) {
            (void)HAL_PCD_EP_Receive(hpcd, 0x00,
                g_usb_out_bufs[0].data(), g_usb_out_mps[0]);
        }
        return;
    }
}

extern "C" CHARM_WEAK void HAL_PCD_DataOutStageCallback(PCD_HandleTypeDef* hpcd, uint8_t epnum) {
    if (!hpcd) return;
    if (!g_usb_hooks_enabled) return;
    if (epnum == 0) {
        const auto len = hpcd->OUT_ep[0].xfer_count;
        const auto* buf = hpcd->OUT_ep[0].xfer_buff;
        g_out0_calls++;
        if (buf && len > 0) {
            g_usb_adapter.handle_out_data(std::span<const usb::u8>(buf, len));
        } else {
            g_usb_adapter.handle_out_data(std::span<const usb::u8>{});
        }
        return;
    }
    if (epnum == 1) {
        g_out1_calls++;
    }
    const auto len = hpcd->OUT_ep[epnum].xfer_count;
    auto& cb = g_usb_out_cbs[epnum];
    if (cb.on_out && len > 0) {
        cb.on_out(g_usb_out_ctx[epnum],
            std::span<const usb::u8>(g_usb_out_bufs[epnum].data(), len));
        player::stm32h7::board::usb_poll_msc(hpcd);
    }
    const auto addr = static_cast<uint8_t>(epnum & 0x0F);
    (void)HAL_PCD_EP_Receive(hpcd, addr,
        g_usb_out_bufs[epnum].data(),
        g_usb_out_mps[epnum]);
}

extern "C" CHARM_WEAK void HAL_PCD_DataInStageCallback(PCD_HandleTypeDef* hpcd, uint8_t epnum) {
    if (!hpcd) return;
    if (!g_usb_hooks_enabled) return;
    if (epnum == 0) {
        const auto sent = (hpcd->IN_ep[0].xfer_len >= hpcd->IN_ep[0].xfer_count)
            ? static_cast<std::uint32_t>(hpcd->IN_ep[0].xfer_len - hpcd->IN_ep[0].xfer_count)
            : static_cast<std::uint32_t>(hpcd->IN_ep[0].xfer_len);
        const bool sent_zlp = (hpcd->IN_ep[0].xfer_len == 0);
        g_in0_calls++;
        (void)HAL_PCD_EP_Receive(hpcd, 0x00, g_usb_out_bufs[0].data(), g_usb_out_mps[0]);
        g_usb_adapter.handle_in_complete(sent, sent_zlp);
        return;
    }
    if (epnum == 1) {
        g_in1_calls++;
        player::stm32h7::board::usb_poll_msc(hpcd);
    }
    auto& cb = g_usb_in_cbs[epnum];
    if (cb.on_in_complete) {
        const auto sent = (hpcd->IN_ep[epnum].xfer_len >= hpcd->IN_ep[epnum].xfer_count)
            ? static_cast<std::uint32_t>(hpcd->IN_ep[epnum].xfer_len - hpcd->IN_ep[epnum].xfer_count)
            : static_cast<std::uint32_t>(hpcd->IN_ep[epnum].xfer_len);
        cb.on_in_complete(g_usb_in_ctx[epnum], sent, false);
    }
}

extern "C" CHARM_WEAK void HAL_PCD_ResetCallback(PCD_HandleTypeDef*) {
    if (!g_usb_hooks_enabled) return;
    g_usb_ep0_prepared = false;
    usb_prepare_ep0(&hpcd_USB_OTG_FS);
    g_reset_calls++;
    g_usb_adapter.handle_reset();
}

extern "C" CHARM_WEAK void HAL_PCD_SuspendCallback(PCD_HandleTypeDef*) {
    if (!g_usb_hooks_enabled) return;
    g_usb_adapter.handle_suspend();
}

extern "C" CHARM_WEAK void HAL_PCD_ResumeCallback(PCD_HandleTypeDef*) {
    if (!g_usb_hooks_enabled) return;
    g_usb_adapter.handle_resume();
}

extern "C" CHARM_WEAK void HAL_PCD_ConnectCallback(PCD_HandleTypeDef*) {
    if (!g_usb_hooks_enabled) return;
    g_connect_calls++;
    g_usb_adapter.handle_connect(true);
}

extern "C" CHARM_WEAK void HAL_PCD_DisconnectCallback(PCD_HandleTypeDef*) {
    if (!g_usb_hooks_enabled) return;
    g_usb_adapter.handle_connect(false);
}

extern "C" int charm_usb_setup_hook(PCD_HandleTypeDef* hpcd) {
    if (!g_usb_hooks_enabled || !hpcd) return 0;
    auto* setup_bytes = reinterpret_cast<std::uint8_t*>(hpcd->Setup);
    for (std::size_t i = 0; i < g_setup_raw.size(); ++i) {
        g_setup_raw[i] = setup_bytes[i];
    }
    usb::SetupPacket setup{};
    setup.bm_request_type = setup_bytes[0];
    setup.b_request = setup_bytes[1];
    setup.w_value = static_cast<usb::u16>(setup_bytes[2] | (setup_bytes[3] << 8));
    setup.w_index = static_cast<usb::u16>(setup_bytes[4] | (setup_bytes[5] << 8));
    setup.w_length = static_cast<usb::u16>(setup_bytes[6] | (setup_bytes[7] << 8));
    const std::uint8_t idx = g_setup_hist_head;
    g_setup_hist_bm[idx] = setup.bm_request_type;
    g_setup_hist_b[idx] = setup.b_request;
    g_setup_hist_wv[idx] = setup.w_value;
    g_setup_hist_wl[idx] = setup.w_length;
    g_setup_hist_head = static_cast<std::uint8_t>((idx + 1u) % g_setup_hist_bm.size());
    if (g_setup_hist_count < g_setup_hist_bm.size()) {
        g_setup_hist_count++;
    }
    g_setup_calls++;
    g_last_bm = setup.bm_request_type;
    g_last_b = setup.b_request;
    g_last_w_value = setup.w_value;
    g_last_w_index = setup.w_index;
    g_last_w_length = setup.w_length;
    if (usb::request_type(setup.bm_request_type) == usb::RequestType::class_request) {
        g_class_setup_calls++;
        g_class_last_bm = setup.bm_request_type;
        g_class_last_b = setup.b_request;
        g_class_last_wv = setup.w_value;
        g_class_last_wl = setup.w_length;
    }
    if (usb::request_type(setup.bm_request_type) == usb::RequestType::standard &&
            setup.b_request == 0x01 &&
            (setup.bm_request_type & 0x1Fu) == 0x02) {
        const auto ep = static_cast<std::uint8_t>(setup.w_index & 0xFFu);
        (void)HAL_PCD_EP_ClrStall(hpcd, ep);
        if (g_msc_bot) {
            g_msc_bot->on_clear_stall((ep & 0x80u) != 0u);
        }
    }
    g_usb_adapter.handle_setup(setup);
    if ((setup.bm_request_type & 0x80u) == 0u) {
        (void)HAL_PCD_EP_Receive(hpcd, 0x00,
            g_usb_out_bufs[0].data(),
            g_usb_out_mps[0]);
    }
    return 1;
}

extern "C" int charm_usb_data_out_hook(PCD_HandleTypeDef* hpcd, uint8_t epnum) {
    if (!g_usb_hooks_enabled || !hpcd) return 0;
    if (epnum == 0) {
        const auto len = hpcd->OUT_ep[0].xfer_count;
        const auto* buf = hpcd->OUT_ep[0].xfer_buff;
        g_out0_calls++;
        if (buf && len > 0) {
            g_usb_adapter.handle_out_data(std::span<const usb::u8>(buf, len));
        } else {
            g_usb_adapter.handle_out_data(std::span<const usb::u8>{});
        }
        return 1;
    }
    const auto len = hpcd->OUT_ep[epnum].xfer_count;
    auto& cb = g_usb_out_cbs[epnum];
    if (cb.on_out && len > 0) {
        cb.on_out(g_usb_out_ctx[epnum],
            std::span<const usb::u8>(g_usb_out_bufs[epnum].data(), len));
        player::stm32h7::board::usb_poll_msc(hpcd);
    }
    const auto addr = static_cast<uint8_t>(epnum & 0x0F);
    (void)HAL_PCD_EP_Receive(hpcd, addr,
        g_usb_out_bufs[epnum].data(),
        g_usb_out_mps[epnum]);
    return 1;
}

extern "C" int charm_usb_data_in_hook(PCD_HandleTypeDef* hpcd, uint8_t epnum) {
    if (!g_usb_hooks_enabled || !hpcd) return 0;
    if (epnum == 0) {
        const auto sent = (hpcd->IN_ep[0].xfer_len >= hpcd->IN_ep[0].xfer_count)
            ? static_cast<std::uint32_t>(hpcd->IN_ep[0].xfer_len - hpcd->IN_ep[0].xfer_count)
            : static_cast<std::uint32_t>(hpcd->IN_ep[0].xfer_len);
        const bool sent_zlp = (hpcd->IN_ep[0].xfer_len == 0);
        g_in0_calls++;
        (void)HAL_PCD_EP_Receive(hpcd, 0x00, g_usb_out_bufs[0].data(), g_usb_out_mps[0]);
        g_usb_adapter.handle_in_complete(sent, sent_zlp);
        return 1;
    }
    if (epnum == 1) {
        g_in1_calls++;
    }
    auto& cb = g_usb_in_cbs[epnum];
    if (cb.on_in_complete) {
        const auto sent = (hpcd->IN_ep[epnum].xfer_len >= hpcd->IN_ep[epnum].xfer_count)
            ? static_cast<std::uint32_t>(hpcd->IN_ep[epnum].xfer_len - hpcd->IN_ep[epnum].xfer_count)
            : static_cast<std::uint32_t>(hpcd->IN_ep[epnum].xfer_len);
        cb.on_in_complete(g_usb_in_ctx[epnum], sent, false);
    }
    return 1;
}

extern "C" int charm_usb_reset_hook(PCD_HandleTypeDef* hpcd) {
    if (!g_usb_hooks_enabled || !hpcd) return 0;
    g_reset_calls++;
    g_usb_ep0_prepared = false;
    usb_prepare_ep0(hpcd);
    g_usb_adapter.handle_reset();
    return 1;
}

extern "C" int charm_usb_suspend_hook(PCD_HandleTypeDef* hpcd) {
    if (!g_usb_hooks_enabled || !hpcd) return 0;
    g_usb_adapter.handle_suspend();
    return 1;
}

extern "C" int charm_usb_resume_hook(PCD_HandleTypeDef* hpcd) {
    if (!g_usb_hooks_enabled || !hpcd) return 0;
    g_usb_adapter.handle_resume();
    return 1;
}

extern "C" int charm_usb_connect_hook(PCD_HandleTypeDef* hpcd) {
    if (!g_usb_hooks_enabled || !hpcd) return 0;
    g_connect_calls++;
    g_usb_adapter.handle_connect(true);
    return 1;
}

extern "C" int charm_usb_disconnect_hook(PCD_HandleTypeDef* hpcd) {
    if (!g_usb_hooks_enabled || !hpcd) return 0;
    g_usb_adapter.handle_connect(false);
    return 1;
}
