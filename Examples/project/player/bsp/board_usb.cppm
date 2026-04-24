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
import player.stm32h7.usb_glue_core;
import player.stm32h7.usb_msc_glue;

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
    namespace usb_core = player::stm32h7::usb_glue_core;

    usb_core::Core g_usb_core{};
    player::stm32h7::usb_msc_glue::State g_msc{};
    auto& g_usb_adapter = g_usb_core.adapter;
    auto& g_usb_dcd_ops = g_usb_core.dcd_ops;
    auto& g_usb_out_mps = g_usb_core.out_mps;
    bool g_usb_hooks_enabled = false;
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

    inline usb_core::Core* board_usb_core(PCD_HandleTypeDef* pcd) noexcept {
        if (!pcd) return nullptr;
        if (pcd == &hpcd_USB_OTG_FS && usb_core::from_pcd(pcd) != &g_usb_core) {
            usb_core::bind_pcd(g_usb_core, pcd);
        }
        return usb_core::from_pcd(pcd);
    }

    void usb_set_ready(void*, usb::class_driver::MscBot* bot,
                       const usb::class_driver::MscConfig* cfg) noexcept {
        player::stm32h7::usb_msc_glue::set_ready(g_msc, bot, cfg);
    }

    bool usb_ep_open(void* ctx, const usb::driver::EpConfig& cfg,
                     usb::driver::EpCallbacks cb) noexcept {
        auto* pcd = usb_pcd(ctx);
        auto* core = board_usb_core(pcd);
        return core && usb_core::ep_open(*core, pcd, cfg, cb);
    }

    bool usb_ep_close(void* ctx, usb::u8 address) noexcept {
        auto* pcd = usb_pcd(ctx);
        auto* core = board_usb_core(pcd);
        return core && usb_core::ep_close(*core, pcd, address);
    }

    bool usb_ep_send(void* ctx, usb::u8 address,
                     std::span<const usb::u8> data, bool zlp) noexcept {
        auto* pcd = usb_pcd(ctx);
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
        auto* core = board_usb_core(pcd);
        const auto ok = core && usb_core::ep_send(*core, pcd, address, data, zlp);
        if (!ok && is_ep0_in) {
            g_ep0_in_fail++;
        }
        return ok;
    }

    bool usb_ep_stall(void* ctx, usb::u8 address) noexcept {
        auto* pcd = usb_pcd(ctx);
        return usb_core::ep_stall(pcd, address);
    }

    bool usb_set_address(void* ctx, usb::u8 address) noexcept {
        auto* pcd = usb_pcd(ctx);
        const auto ok = usb_core::set_address(pcd, address);
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
        auto* core = board_usb_core(pcd);
        g_set_cfg_calls++;
        g_set_cfg_last = configured ? 1u : 0u;
        g_set_cfg_last_ok = 0;
        g_set_cfg_open_out_ok = 0;
        g_set_cfg_open_in_ok = 0;
        g_set_cfg_arm_out_ok = 0;
        const auto ok = core && usb_core::set_configured(*core, pcd, configured);
        g_set_cfg_last_ok = ok ? 1u : 0u;
        return ok;
    }

    bool usb_connect(void* ctx, bool enable) noexcept {
        auto* pcd = usb_pcd(ctx);
        auto* core = board_usb_core(pcd);
        return core && usb_core::connect(*core, pcd, enable,
            usb_core::ConnectMode::device_connect);
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
        usb_core::bind_pcd(g_usb_core, &hpcd_USB_OTG_FS);
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
        usb_core::bind_pcd(g_usb_core, &hpcd_USB_OTG_FS);
    }

    void usb_init_early(bool use_st_stack) noexcept {
        if (use_st_stack) {
            MX_USB_OTG_FS_PCD_Init();
            return;
        }
        usb_hw_init();
    }

    void usb_poll_msc(PCD_HandleTypeDef* pcd) noexcept {
        if (!g_usb_hooks_enabled || !pcd) return;
        (void)player::stm32h7::usb_msc_glue::poll(
            g_usb_core,
            g_msc,
            g_usb_dcd_ops,
            pcd);
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

extern "C" int charm_usb_setup_hook(PCD_HandleTypeDef* hpcd);
extern "C" int charm_usb_data_out_hook(PCD_HandleTypeDef* hpcd, uint8_t epnum);
extern "C" int charm_usb_data_in_hook(PCD_HandleTypeDef* hpcd, uint8_t epnum);
extern "C" int charm_usb_reset_hook(PCD_HandleTypeDef* hpcd);
extern "C" int charm_usb_suspend_hook(PCD_HandleTypeDef* hpcd);
extern "C" int charm_usb_resume_hook(PCD_HandleTypeDef* hpcd);
extern "C" int charm_usb_connect_hook(PCD_HandleTypeDef* hpcd);
extern "C" int charm_usb_disconnect_hook(PCD_HandleTypeDef* hpcd);

namespace {
    void record_setup_diag(const usb_core::SetupView& view) noexcept {
        g_setup_raw = view.raw;
        const auto& setup = view.packet;
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
    }

    void record_data_out_diag(const usb_core::DataOutEvent& event) noexcept {
        if (event.epnum == 0u) {
            g_out0_calls++;
            return;
        }
        if (event.epnum == 1u) {
            g_out1_calls++;
        }
    }

    void record_data_in_diag(const usb_core::DataInEvent& event) noexcept {
        if (event.epnum == 0u) {
            g_in0_calls++;
            return;
        }
        if (event.epnum == 1u) {
            g_in1_calls++;
        }
    }
}

extern "C" CHARM_WEAK void HAL_PCD_SetupStageCallback(PCD_HandleTypeDef* hpcd) {
    (void)charm_usb_setup_hook(hpcd);
}

extern "C" CHARM_WEAK void HAL_PCD_DataOutStageCallback(PCD_HandleTypeDef* hpcd, uint8_t epnum) {
    (void)charm_usb_data_out_hook(hpcd, epnum);
}

extern "C" CHARM_WEAK void HAL_PCD_DataInStageCallback(PCD_HandleTypeDef* hpcd, uint8_t epnum) {
    (void)charm_usb_data_in_hook(hpcd, epnum);
}

extern "C" CHARM_WEAK void HAL_PCD_ResetCallback(PCD_HandleTypeDef* hpcd) {
    (void)charm_usb_reset_hook(hpcd);
}

extern "C" CHARM_WEAK void HAL_PCD_SuspendCallback(PCD_HandleTypeDef* hpcd) {
    (void)charm_usb_suspend_hook(hpcd);
}

extern "C" CHARM_WEAK void HAL_PCD_ResumeCallback(PCD_HandleTypeDef* hpcd) {
    (void)charm_usb_resume_hook(hpcd);
}

extern "C" CHARM_WEAK void HAL_PCD_ConnectCallback(PCD_HandleTypeDef* hpcd) {
    (void)charm_usb_connect_hook(hpcd);
}

extern "C" CHARM_WEAK void HAL_PCD_DisconnectCallback(PCD_HandleTypeDef* hpcd) {
    (void)charm_usb_disconnect_hook(hpcd);
}

extern "C" int charm_usb_setup_hook(PCD_HandleTypeDef* hpcd) {
    if (!g_usb_hooks_enabled || !hpcd) return 0;
    auto* core = board_usb_core(hpcd);
    if (!core) return 0;
    const auto view = usb_core::decode_setup(hpcd);
    record_setup_diag(view);
    return usb_core::handle_setup(*core, hpcd, view);
}

extern "C" int charm_usb_data_out_hook(PCD_HandleTypeDef* hpcd, uint8_t epnum) {
    if (!g_usb_hooks_enabled || !hpcd) return 0;
    auto* core = board_usb_core(hpcd);
    if (!core) return 0;
    const auto event = usb_core::inspect_data_out(hpcd, epnum);
    record_data_out_diag(event);
    const auto handled = usb_core::handle_data_out(*core, hpcd, event);
    if (handled && g_msc.cfg &&
            epnum == static_cast<uint8_t>(g_msc.cfg->ep_out & 0x0F) &&
            event.len > 0u) {
        player::stm32h7::board::usb_poll_msc(hpcd);
    }
    return handled;
}

extern "C" int charm_usb_data_in_hook(PCD_HandleTypeDef* hpcd, uint8_t epnum) {
    if (!g_usb_hooks_enabled || !hpcd) return 0;
    auto* core = board_usb_core(hpcd);
    if (!core) return 0;
    const auto event = usb_core::inspect_data_in(hpcd, epnum);
    record_data_in_diag(event);
    const auto handled = usb_core::handle_data_in(*core, hpcd, event);
    if (handled && g_msc.cfg && epnum == static_cast<uint8_t>(g_msc.cfg->ep_in & 0x0F)) {
        player::stm32h7::board::usb_poll_msc(hpcd);
    }
    return handled;
}

extern "C" int charm_usb_reset_hook(PCD_HandleTypeDef* hpcd) {
    if (!g_usb_hooks_enabled || !hpcd) return 0;
    auto* core = board_usb_core(hpcd);
    if (!core) return 0;
    g_reset_calls++;
    return usb_core::handle_reset(*core, hpcd);
}

extern "C" int charm_usb_suspend_hook(PCD_HandleTypeDef* hpcd) {
    if (!g_usb_hooks_enabled || !hpcd) return 0;
    auto* core = board_usb_core(hpcd);
    return core ? usb_core::handle_suspend(*core) : 0;
}

extern "C" int charm_usb_resume_hook(PCD_HandleTypeDef* hpcd) {
    if (!g_usb_hooks_enabled || !hpcd) return 0;
    auto* core = board_usb_core(hpcd);
    return core ? usb_core::handle_resume(*core) : 0;
}

extern "C" int charm_usb_connect_hook(PCD_HandleTypeDef* hpcd) {
    if (!g_usb_hooks_enabled || !hpcd) return 0;
    auto* core = board_usb_core(hpcd);
    if (!core) return 0;
    g_connect_calls++;
    return usb_core::handle_connect(*core, true);
}

extern "C" int charm_usb_disconnect_hook(PCD_HandleTypeDef* hpcd) {
    if (!g_usb_hooks_enabled || !hpcd) return 0;
    auto* core = board_usb_core(hpcd);
    return core ? usb_core::handle_connect(*core, false) : 0;
}
