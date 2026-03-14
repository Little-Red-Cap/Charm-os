#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <span>

#include "stm32h7xx_hal.h"
#include "dma.h"
#include "gpio.h"
#include "sdmmc.h"
#include "usart.h"
#include "usb_otg.h"

import charm.system.app_host;
import charm.system.caps;
import charm.system.clock;
import charm.system.init_block;
import charm.system.init_core;
import charm.system.init_usb;
import block.device;
import block.registry;
import block.sdmmc;
import init.graph;
import init.node;
import kernel.capabilities;
import usb.class_msc;
import usb.class_msc_block;
import usb.class_msc_block.node;
import usb.common;
import usb.device_driver;
import usb.driver;
import usb.dsl;
import util.core;
import util.error;

extern "C" {
    void SystemClock_Config(void);
    void MX_GPIO_Init(void);
    void MX_DMA_Init(void);
    void MX_SDMMC1_SD_Init(void);
    void MX_USB_OTG_FS_PCD_Init(void);
    void MX_USART1_UART_Init(void);
    void Error_Handler(void);
    extern UART_HandleTypeDef huart1;
    extern PCD_HandleTypeDef hpcd_USB_OTG_FS;
    extern SD_HandleTypeDef hsd1;
}

namespace {
    usb::driver::DcdDeviceAdapter g_usb_adapter{};
    usb::driver::DcdOps g_usb_dcd_ops{};
    usb::class_driver::MscBot* g_msc_bot = nullptr;
    const usb::class_driver::MscConfig* g_msc_cfg = nullptr;
    std::array<usb::driver::EpCallbacks, 16> g_usb_out_cbs{};
    std::array<usb::driver::EpCallbacks, 16> g_usb_in_cbs{};
    std::array<void*, 16> g_usb_out_ctxs{};
    std::array<void*, 16> g_usb_in_ctxs{};
    std::array<std::array<usb::u8, 64>, 16> g_usb_out_bufs{};
    std::array<usb::u16, 16> g_usb_out_mps{};

    constexpr usb::u16 kLangs[] = { 0x0409 };
    constexpr auto kLangDesc = usb::make_lang_id_descriptor(kLangs);
    constexpr auto kVendorStr = usb::make_ascii_string_descriptor("Charm");
    constexpr auto kProductStr = usb::make_ascii_string_descriptor("Charm MSC");
    constexpr auto kSerialStr = usb::make_ascii_string_descriptor("0001");

    static const usb::StringTable<4> kUsbStrings{
        std::array<std::span<const usb::u8>, 4>{
            std::span<const usb::u8>(kLangDesc.data(), kLangDesc.size()),
            std::span<const usb::u8>(kVendorStr.data(), kVendorStr.size()),
            std::span<const usb::u8>(kProductStr.data(), kProductStr.size()),
            std::span<const usb::u8>(kSerialStr.data(), kSerialStr.size()),
        }
    };

    util::u32 g_sd_block_size = 512;

    inline void early_uart_print(const char* msg) noexcept {
        if (!msg) return;
        const std::size_t len = std::strlen(msg);
        if (len == 0) return;
        (void)HAL_UART_Transmit(&huart1,
            reinterpret_cast<uint8_t*>(const_cast<char*>(msg)),
            static_cast<uint16_t>(len),
            100);
    }

    inline PCD_HandleTypeDef* usb_pcd(void* ctx) noexcept {
        return static_cast<PCD_HandleTypeDef*>(ctx);
    }

    void usb_set_ready(void*, usb::class_driver::MscBot* bot,
                       const usb::class_driver::MscConfig* cfg) noexcept {
        g_msc_bot = bot;
        g_msc_cfg = cfg;
        for (auto& ctx : g_usb_out_ctxs) ctx = bot;
        for (auto& ctx : g_usb_in_ctxs) ctx = bot;
    }

    bool usb_ep_open(void* ctx, const usb::driver::EpConfig& cfg,
                     usb::driver::EpCallbacks cb) noexcept {
        auto* pcd = usb_pcd(ctx);
        if (!pcd) return false;
        std::uint8_t type = EP_TYPE_BULK;
        switch (cfg.type) {
        case usb::driver::EpType::control: type = EP_TYPE_CTRL; break;
        case usb::driver::EpType::isochronous: type = EP_TYPE_ISOC; break;
        case usb::driver::EpType::bulk: type = EP_TYPE_BULK; break;
        case usb::driver::EpType::interrupt: type = EP_TYPE_INTR; break;
        }
        if (HAL_PCD_EP_Open(pcd, cfg.address, cfg.max_packet_size, type) != HAL_OK) {
            return false;
        }
        const std::uint8_t ep_num = static_cast<std::uint8_t>(cfg.address & 0x0F);
        if (cfg.direction == usb::driver::EpDirection::out) {
            g_usb_out_cbs[ep_num] = cb;
            g_usb_out_mps[ep_num] = cfg.max_packet_size;
            (void)HAL_PCD_EP_Receive(pcd, cfg.address,
                g_usb_out_bufs[ep_num].data(),
                g_usb_out_mps[ep_num]);
        } else {
            g_usb_in_cbs[ep_num] = cb;
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
        } else {
            g_usb_out_cbs[ep_num] = {};
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

    block::Status sdmmc_status(HAL_StatusTypeDef st) noexcept {
        switch (st) {
        case HAL_OK:
            return block::Status{block::Errc::ok};
        case HAL_BUSY:
            return block::Status{block::Errc::busy};
        case HAL_TIMEOUT:
            return block::Status{block::Errc::timeout};
        default:
            return block::Status{block::Errc::io};
        }
    }

    bool sdmmc_wait_ready(SD_HandleTypeDef* sd, util::u32 timeout_ms) noexcept {
        const util::u32 start = HAL_GetTick();
        while (HAL_SD_GetCardState(sd) != HAL_SD_CARD_TRANSFER) {
            if ((HAL_GetTick() - start) > timeout_ms) {
                return false;
            }
        }
        return true;
    }

    block::Status sdmmc_init(void* ctx, const block::SdmmcConfig& cfg,
                             block::SdmmcInfo& out) noexcept {
        auto* sd = static_cast<SD_HandleTypeDef*>(ctx);
        if (!sd) return block::Status{block::Errc::invalid_arg};

        const auto init_status = HAL_SD_Init(sd);
        if (init_status != HAL_OK) {
            return sdmmc_status(init_status);
        }

        if (cfg.bus_width == 4) {
            (void)HAL_SD_ConfigWideBusOperation(sd, SDMMC_BUS_WIDE_4B);
        } else if (cfg.bus_width == 1) {
            (void)HAL_SD_ConfigWideBusOperation(sd, SDMMC_BUS_WIDE_1B);
        }

        HAL_SD_CardInfoTypeDef info{};
        if (HAL_SD_GetCardInfo(sd, &info) != HAL_OK) {
            return block::Status{block::Errc::io};
        }

        const util::u32 block_size = info.LogBlockSize ? info.LogBlockSize : info.BlockSize;
        const util::u32 block_count = info.LogBlockNbr ? info.LogBlockNbr : info.BlockNbr;
        if (block_size == 0 || block_count == 0) {
            return block::Status{block::Errc::invalid_arg};
        }

        g_sd_block_size = block_size;
        out.block_size = block_size;
        out.block_count = block_count;
        return block::Status{block::Errc::ok};
    }

    block::Status sdmmc_read(void* ctx, util::u64 lba,
                             std::span<util::u8> data) noexcept {
        auto* sd = static_cast<SD_HandleTypeDef*>(ctx);
        if (!sd) return block::Status{block::Errc::invalid_arg};
        if (data.empty() || g_sd_block_size == 0) {
            return block::Status{block::Errc::invalid_arg};
        }

        const util::u32 blocks = static_cast<util::u32>(data.size() / g_sd_block_size);
        if (blocks == 0) return block::Status{block::Errc::invalid_arg};

        const auto st = HAL_SD_ReadBlocks(sd, data.data(),
            static_cast<uint32_t>(lba), blocks, 1000);
        if (st != HAL_OK) {
            return sdmmc_status(st);
        }
        if (!sdmmc_wait_ready(sd, 1000)) {
            return block::Status{block::Errc::timeout};
        }
        return block::Status{block::Errc::ok};
    }

    const block::SdmmcOps kSdmmcOps{
        &sdmmc_init,
        &sdmmc_read,
        nullptr,
        nullptr,
        nullptr
    };
}

extern "C" void OTG_FS_IRQHandler(void) {
    HAL_PCD_IRQHandler(&hpcd_USB_OTG_FS);
}

extern "C" void HAL_PCD_SetupStageCallback(PCD_HandleTypeDef* hpcd) {
    if (!hpcd) return;
    usb::SetupPacket setup{};
    setup.bm_request_type = hpcd->Setup[0];
    setup.b_request = hpcd->Setup[1];
    setup.w_value = static_cast<usb::u16>(hpcd->Setup[2] | (hpcd->Setup[3] << 8));
    setup.w_index = static_cast<usb::u16>(hpcd->Setup[4] | (hpcd->Setup[5] << 8));
    setup.w_length = static_cast<usb::u16>(hpcd->Setup[6] | (hpcd->Setup[7] << 8));
    g_usb_adapter.handle_setup(setup);
}

extern "C" void HAL_PCD_DataOutStageCallback(PCD_HandleTypeDef* hpcd, uint8_t epnum) {
    if (!hpcd) return;
    const auto len = hpcd->OUT_ep[epnum].xfer_count;
    auto& cb = g_usb_out_cbs[epnum];
    if (cb.on_out && len > 0) {
        cb.on_out(g_usb_out_ctxs[epnum],
            std::span<const usb::u8>(g_usb_out_bufs[epnum].data(), len));
    }
    const auto addr = static_cast<uint8_t>(epnum & 0x0F);
    (void)HAL_PCD_EP_Receive(hpcd, addr,
        g_usb_out_bufs[epnum].data(),
        g_usb_out_mps[epnum]);
}

extern "C" void HAL_PCD_DataInStageCallback(PCD_HandleTypeDef* hpcd, uint8_t epnum) {
    if (!hpcd) return;
    auto& cb = g_usb_in_cbs[epnum];
    if (cb.on_in_complete) {
        const auto sent = hpcd->IN_ep[epnum].xfer_count;
        cb.on_in_complete(g_usb_in_ctxs[epnum], sent, false);
    }
}

extern "C" void HAL_PCD_ResetCallback(PCD_HandleTypeDef*) {
    g_usb_adapter.handle_reset();
}

extern "C" void HAL_PCD_SuspendCallback(PCD_HandleTypeDef*) {
    g_usb_adapter.handle_suspend();
}

extern "C" void HAL_PCD_ResumeCallback(PCD_HandleTypeDef*) {
    g_usb_adapter.handle_resume();
}

extern "C" void HAL_PCD_ConnectCallback(PCD_HandleTypeDef*) {
    g_usb_adapter.handle_connect(true);
}

extern "C" void HAL_PCD_DisconnectCallback(PCD_HandleTypeDef*) {
    g_usb_adapter.handle_connect(false);
}

int main() {
    HAL_Init();
    SystemClock_Config();

    MX_GPIO_Init();
    MX_DMA_Init();
    MX_USART1_UART_Init();
    MX_SDMMC1_SD_Init();
    MX_USB_OTG_FS_PCD_Init();

    early_uart_print("boot: init ok\n");

    g_usb_dcd_ops.ep.open = &usb_ep_open;
    g_usb_dcd_ops.ep.close = &usb_ep_close;
    g_usb_dcd_ops.ep.send = &usb_ep_send;
    g_usb_dcd_ops.ep.stall = &usb_ep_stall;
    g_usb_dcd_ops.set_address = &usb_set_address;
    g_usb_dcd_ops.set_configured = &usb_set_configured;
    g_usb_dcd_ops.connect = &usb_connect;

    using PumpCaps = charm::system::SystemCaps<
        kernel::NoopIrqGuard,
        kernel::NoopWakeup>;

    PumpCaps pump_caps{};
    charm::system::AppHost<PumpCaps> host{pump_caps};

    charm::system::ClockOps clock_ops{
        [](void*) noexcept { return static_cast<util::u64>(HAL_GetTick()); },
        nullptr
    };

    constexpr util::usize kMaxNodes = 16;
    constexpr util::usize kMaxCaps = 32;
    constexpr util::usize kMaxEndpoints = 16;

    charm::system::CoreSystemChain<kMaxEndpoints> core{
        clock_ops,
        nullptr,
        host.pump(),
        host.post_fn(),
        host.post_ctx(),
        host.pump_id(),
        8
    };

    block::SdmmcHandle sdmmc_handle{&hsd1, &kSdmmcOps};
    block::SdmmcConfig sdmmc_cfg{};
    sdmmc_cfg.clock_hz = 0;
    sdmmc_cfg.bus_width = 4;
    sdmmc_cfg.use_dma = false;

    charm::system::SdmmcInitChain<block::Registry<kMaxEndpoints>> sdmmc_chain{
        core.block_registry,
        sdmmc_handle,
        sdmmc_cfg,
        "block.sd0"
    };

    usb::device::MscBlockDesc usb_desc{};
    usb_desc.cap_name = "usb.msc0";
    usb_desc.block_cap = "block.sd0";
    usb_desc.dcd = g_usb_dcd_ops;
    usb_desc.dcd_ctx = &hpcd_USB_OTG_FS;
    usb_desc.adapter = &g_usb_adapter;
    usb_desc.dev_info.vendor_id = 0x1209;
    usb_desc.dev_info.product_id = 0x0002;
    usb_desc.dev_info.i_manufacturer = 1;
    usb_desc.dev_info.i_product = 2;
    usb_desc.dev_info.i_serial = 3;
    usb_desc.msc_cfg.ep_out = 0x01;
    usb_desc.msc_cfg.ep_in = 0x81;
    usb_desc.msc_cfg.ep_mps = 64;
    usb_desc.strings = std::span<const std::span<const usb::u8>>(
        kUsbStrings.entries.data(), kUsbStrings.entries.size());
    usb_desc.storage_cfg.read_only = true;
    usb_desc.on_ready = &usb_set_ready;
    usb_desc.on_ready_ctx = nullptr;

    charm::system::UsbMscBlockInitChain<block::Registry<kMaxEndpoints>> usb_chain{
        core.block_registry, usb_desc
    };

    init::Graph<kMaxNodes, kMaxCaps> graph{};
    std::array<const init::Node*, kMaxNodes> nodes{};
    util::usize idx = 0;
    const auto core_nodes = core.node_span();
    for (util::usize i = 0; i < core_nodes.size(); ++i) {
        nodes[idx++] = core_nodes[i];
    }
    const auto sd_nodes = sdmmc_chain.node_span();
    for (util::usize i = 0; i < sd_nodes.size(); ++i) {
        nodes[idx++] = sd_nodes[i];
    }
    const auto usb_nodes = usb_chain.node_span();
    for (util::usize i = 0; i < usb_nodes.size(); ++i) {
        nodes[idx++] = usb_nodes[i];
    }

    auto r = graph.build(std::span<const init::Node* const>(nodes.data(), idx),
                         static_cast<util::u32>(init::Runlevel::all),
                         init::Phase::app);
    if (!r) {
        early_uart_print("boot: graph build failed\n");
        Error_Handler();
    }
    auto r_start = graph.start();
    if (!r_start) {
        early_uart_print("boot: graph start failed\n");
        Error_Handler();
    }

    early_uart_print("boot: usb msc ready\n");

    while (true) {
        (void)host.run_once();
        if (g_msc_bot && g_msc_cfg) {
            (void)usb::device::examples::send_msc_in_packet(
                g_usb_dcd_ops, &hpcd_USB_OTG_FS, *g_msc_bot, *g_msc_cfg);
        }
    }
}
