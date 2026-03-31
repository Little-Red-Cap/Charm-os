module;

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

export module player.app_test_hqzy.app_system;

import charm.system.app_host;
import charm.system.caps;
import charm.system.clock;
import charm.system.init_block;
import charm.system.init_core;
import charm.system.init_usb;
import charm.system.time;
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

namespace player::app_test_hqzy::app_system {
    struct SdmmcContext {
        SD_HandleTypeDef* sd{nullptr};
        struct System* system{nullptr};
    };

    struct System {
        usb::driver::DcdDeviceAdapter usb_adapter{};
        usb::driver::DcdOps usb_dcd_ops{};
        usb::class_driver::MscBot* msc_bot{nullptr};
        const usb::class_driver::MscConfig* msc_cfg{nullptr};
        std::array<usb::driver::EpCallbacks, 16> usb_out_cbs{};
        std::array<usb::driver::EpCallbacks, 16> usb_in_cbs{};
        std::array<void*, 16> usb_out_ctxs{};
        std::array<void*, 16> usb_in_ctxs{};
        std::array<std::array<usb::u8, 64>, 16> usb_out_bufs{};
        std::array<usb::u16, 16> usb_out_mps{};
        util::u32 sd_block_size{512};
        PCD_HandleTypeDef* pcd{nullptr};
        SdmmcContext sdmmc_ctx{};
        util::Errc last_err{util::Errc::ok};
        util::u64 last_msc_ms{0};
    };

    namespace detail {
        inline void early_uart_print(const char* msg) noexcept {
            if (!msg) return;
            const std::size_t len = std::strlen(msg);
            if (len == 0) return;
            (void)HAL_UART_Transmit(&huart1,
                reinterpret_cast<uint8_t*>(const_cast<char*>(msg)),
                static_cast<uint16_t>(len),
                100);
        }

        inline System* system_from_pcd(PCD_HandleTypeDef* pcd) noexcept {
            return pcd ? static_cast<System*>(pcd->pData) : nullptr;
        }

        inline System* system_from_ctx(void* ctx) noexcept {
            return static_cast<System*>(ctx);
        }

        bool usb_ep_open(void* ctx, const usb::driver::EpConfig& cfg,
                         usb::driver::EpCallbacks cb) noexcept {
            auto* sys = system_from_ctx(ctx);
            if (!sys || !sys->pcd) return false;
            std::uint8_t type = EP_TYPE_BULK;
            switch (cfg.type) {
            case usb::driver::EpType::control: type = EP_TYPE_CTRL; break;
            case usb::driver::EpType::isochronous: type = EP_TYPE_ISOC; break;
            case usb::driver::EpType::bulk: type = EP_TYPE_BULK; break;
            case usb::driver::EpType::interrupt: type = EP_TYPE_INTR; break;
            }
            if (HAL_PCD_EP_Open(sys->pcd, cfg.address, cfg.max_packet_size, type) != HAL_OK) {
                return false;
            }
            const std::uint8_t ep_num = static_cast<std::uint8_t>(cfg.address & 0x0F);
            if (cfg.direction == usb::driver::EpDirection::out) {
                sys->usb_out_cbs[ep_num] = cb;
                sys->usb_out_mps[ep_num] = cfg.max_packet_size;
                (void)HAL_PCD_EP_Receive(sys->pcd, cfg.address,
                    sys->usb_out_bufs[ep_num].data(),
                    sys->usb_out_mps[ep_num]);
            } else {
                sys->usb_in_cbs[ep_num] = cb;
            }
            return true;
        }

        bool usb_ep_close(void* ctx, usb::u8 address) noexcept {
            auto* sys = system_from_ctx(ctx);
            if (!sys || !sys->pcd) return false;
            if (HAL_PCD_EP_Close(sys->pcd, address) != HAL_OK) return false;
            const std::uint8_t ep_num = static_cast<std::uint8_t>(address & 0x0F);
            if ((address & 0x80) != 0) {
                sys->usb_in_cbs[ep_num] = {};
            } else {
                sys->usb_out_cbs[ep_num] = {};
            }
            return true;
        }

        bool usb_ep_send(void* ctx, usb::u8 address,
                         std::span<const usb::u8> data, bool) noexcept {
            auto* sys = system_from_ctx(ctx);
            if (!sys || !sys->pcd) return false;
            auto* ptr = const_cast<usb::u8*>(data.data());
            return HAL_PCD_EP_Transmit(sys->pcd, address, ptr,
                static_cast<uint16_t>(data.size())) == HAL_OK;
        }

        bool usb_ep_stall(void* ctx, usb::u8 address) noexcept {
            auto* sys = system_from_ctx(ctx);
            if (!sys || !sys->pcd) return false;
            return HAL_PCD_EP_SetStall(sys->pcd, address) == HAL_OK;
        }

        bool usb_set_address(void* ctx, usb::u8 address) noexcept {
            auto* sys = system_from_ctx(ctx);
            if (!sys || !sys->pcd) return false;
            return HAL_PCD_SetAddress(sys->pcd, address) == HAL_OK;
        }

        bool usb_set_configured(void* ctx, bool configured) noexcept {
            auto* sys = system_from_ctx(ctx);
            if (!sys || !sys->pcd) return false;
            return configured ? (HAL_PCD_Start(sys->pcd) == HAL_OK)
                              : (HAL_PCD_Stop(sys->pcd) == HAL_OK);
        }

        bool usb_connect(void* ctx, bool enable) noexcept {
            auto* sys = system_from_ctx(ctx);
            if (!sys || !sys->pcd) return false;
            return enable ? (HAL_PCD_Start(sys->pcd) == HAL_OK)
                          : (HAL_PCD_Stop(sys->pcd) == HAL_OK);
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

        util::Errc project_block_err(block::Errc err) noexcept {
            switch (err) {
            case block::Errc::ok: return util::Errc::ok;
            case block::Errc::busy: return util::Errc::busy;
            case block::Errc::timeout: return util::Errc::timeout;
            case block::Errc::invalid_arg: return util::Errc::invalid_arg;
            default: return util::Errc::io;
            }
        }

        bool sdmmc_wait_ready(SD_HandleTypeDef* sd, util::u64 deadline_ms) noexcept {
            if (!sd || !charm::system::time::bound()) return false;
            while (HAL_SD_GetCardState(sd) != HAL_SD_CARD_TRANSFER) {
                if (charm::system::time::now_ms() >= deadline_ms) {
                    return false;
                }
            }
            return true;
        }

        block::Status sdmmc_init(void* ctx, const block::SdmmcConfig& cfg,
                                 block::SdmmcInfo& out) noexcept {
            auto* sd_ctx = static_cast<SdmmcContext*>(ctx);
            if (!sd_ctx || !sd_ctx->sd || !sd_ctx->system) {
                return block::Status{block::Errc::invalid_arg};
            }
            auto* sd = sd_ctx->sd;

            const auto init_status = HAL_SD_Init(sd);
            if (init_status != HAL_OK) {
                auto st = sdmmc_status(init_status);
                sd_ctx->system->last_err = project_block_err(st.err);
                return st;
            }

            if (cfg.bus_width == 4) {
                (void)HAL_SD_ConfigWideBusOperation(sd, SDMMC_BUS_WIDE_4B);
            } else if (cfg.bus_width == 1) {
                (void)HAL_SD_ConfigWideBusOperation(sd, SDMMC_BUS_WIDE_1B);
            }

            HAL_SD_CardInfoTypeDef info{};
            if (HAL_SD_GetCardInfo(sd, &info) != HAL_OK) {
                sd_ctx->system->last_err = util::Errc::io;
                return block::Status{block::Errc::io};
            }

            const util::u32 block_size = info.LogBlockSize ? info.LogBlockSize : info.BlockSize;
            const util::u32 block_count = info.LogBlockNbr ? info.LogBlockNbr : info.BlockNbr;
            if (block_size == 0 || block_count == 0) {
                sd_ctx->system->last_err = util::Errc::invalid_arg;
                return block::Status{block::Errc::invalid_arg};
            }

            sd_ctx->system->sd_block_size = block_size;
            sd_ctx->system->last_err = util::Errc::ok;
            out.block_size = block_size;
            out.block_count = block_count;
            return block::Status{block::Errc::ok};
        }

        block::Status sdmmc_read(void* ctx, util::u64 lba,
                                 std::span<util::u8> data) noexcept {
            auto* sd_ctx = static_cast<SdmmcContext*>(ctx);
            if (!sd_ctx || !sd_ctx->sd || !sd_ctx->system) {
                return block::Status{block::Errc::invalid_arg};
            }
            auto* sd = sd_ctx->sd;
            if (data.empty() || sd_ctx->system->sd_block_size == 0) {
                sd_ctx->system->last_err = util::Errc::invalid_arg;
                return block::Status{block::Errc::invalid_arg};
            }

            const util::u32 blocks =
                static_cast<util::u32>(data.size() / sd_ctx->system->sd_block_size);
            if (blocks == 0) {
                sd_ctx->system->last_err = util::Errc::invalid_arg;
                return block::Status{block::Errc::invalid_arg};
            }

            const auto st = HAL_SD_ReadBlocks(sd, data.data(),
                static_cast<uint32_t>(lba), blocks, 1000);
            if (st != HAL_OK) {
                auto status = sdmmc_status(st);
                sd_ctx->system->last_err = project_block_err(status.err);
                return status;
            }
            if (!charm::system::time::bound()) {
                sd_ctx->system->last_err = util::Errc::bad_state;
                return block::Status{block::Errc::io};
            }
            const auto deadline = charm::system::time::now_ms() + 1000u;
            if (!sdmmc_wait_ready(sd, deadline)) {
                sd_ctx->system->last_err = util::Errc::timeout;
                return block::Status{block::Errc::timeout};
            }
            sd_ctx->system->last_err = util::Errc::ok;
            return block::Status{block::Errc::ok};
        }

        const block::SdmmcOps kSdmmcOps{
            &sdmmc_init,
            &sdmmc_read,
            nullptr,
            nullptr,
            nullptr
        };

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

        inline void init_platform(System& sys) noexcept {
            HAL_Init();
            SystemClock_Config();

            MX_GPIO_Init();
            MX_DMA_Init();
            MX_USART1_UART_Init();
            MX_SDMMC1_SD_Init();
            MX_USB_OTG_FS_PCD_Init();
            sys.pcd = &hpcd_USB_OTG_FS;
            sys.pcd->pData = &sys;
            sys.sdmmc_ctx = SdmmcContext{&hsd1, &sys};

            early_uart_print("boot: init ok\n");
        }

        inline void init_usb_ops(System& sys) noexcept {
            sys.usb_dcd_ops.ep.open = &usb_ep_open;
            sys.usb_dcd_ops.ep.close = &usb_ep_close;
            sys.usb_dcd_ops.ep.send = &usb_ep_send;
            sys.usb_dcd_ops.ep.stall = &usb_ep_stall;
            sys.usb_dcd_ops.set_address = &usb_set_address;
            sys.usb_dcd_ops.set_configured = &usb_set_configured;
            sys.usb_dcd_ops.connect = &usb_connect;
        }

        util::u64 platform_now_ms(void*) noexcept {
            return static_cast<util::u64>(HAL_GetTick());
        }
    } // namespace detail

    extern "C" void OTG_FS_IRQHandler(void) {
        HAL_PCD_IRQHandler(&hpcd_USB_OTG_FS);
    }

    extern "C" void HAL_PCD_SetupStageCallback(PCD_HandleTypeDef* hpcd) {
        if (!hpcd) return;
        auto* sys = detail::system_from_pcd(hpcd);
        if (!sys) return;
        usb::SetupPacket setup{};
        setup.bm_request_type = hpcd->Setup[0];
        setup.b_request = hpcd->Setup[1];
        setup.w_value = static_cast<usb::u16>(hpcd->Setup[2] | (hpcd->Setup[3] << 8));
        setup.w_index = static_cast<usb::u16>(hpcd->Setup[4] | (hpcd->Setup[5] << 8));
        setup.w_length = static_cast<usb::u16>(hpcd->Setup[6] | (hpcd->Setup[7] << 8));
        sys->usb_adapter.handle_setup(setup);
    }

    extern "C" void HAL_PCD_DataOutStageCallback(PCD_HandleTypeDef* hpcd, uint8_t epnum) {
        if (!hpcd) return;
        auto* sys = detail::system_from_pcd(hpcd);
        if (!sys) return;
        const auto len = hpcd->OUT_ep[epnum].xfer_count;
        auto& cb = sys->usb_out_cbs[epnum];
        if (cb.on_out && len > 0) {
            cb.on_out(sys->usb_out_ctxs[epnum],
                std::span<const usb::u8>(sys->usb_out_bufs[epnum].data(), len));
        }
        const auto addr = static_cast<uint8_t>(epnum & 0x0F);
        (void)HAL_PCD_EP_Receive(hpcd, addr,
            sys->usb_out_bufs[epnum].data(),
            sys->usb_out_mps[epnum]);
    }

    extern "C" void HAL_PCD_DataInStageCallback(PCD_HandleTypeDef* hpcd, uint8_t epnum) {
        if (!hpcd) return;
        auto* sys = detail::system_from_pcd(hpcd);
        if (!sys) return;
        auto& cb = sys->usb_in_cbs[epnum];
        if (cb.on_in_complete) {
            const auto sent = hpcd->IN_ep[epnum].xfer_count;
            cb.on_in_complete(sys->usb_in_ctxs[epnum], sent, false);
        }
    }

    extern "C" void HAL_PCD_ResetCallback(PCD_HandleTypeDef*) {
        if (hpcd_USB_OTG_FS.pData) {
            detail::system_from_pcd(&hpcd_USB_OTG_FS)->usb_adapter.handle_reset();
        }
    }

    extern "C" void HAL_PCD_SuspendCallback(PCD_HandleTypeDef*) {
        if (hpcd_USB_OTG_FS.pData) {
            detail::system_from_pcd(&hpcd_USB_OTG_FS)->usb_adapter.handle_suspend();
        }
    }

    extern "C" void HAL_PCD_ResumeCallback(PCD_HandleTypeDef*) {
        if (hpcd_USB_OTG_FS.pData) {
            detail::system_from_pcd(&hpcd_USB_OTG_FS)->usb_adapter.handle_resume();
        }
    }

    extern "C" void HAL_PCD_ConnectCallback(PCD_HandleTypeDef*) {
        if (hpcd_USB_OTG_FS.pData) {
            detail::system_from_pcd(&hpcd_USB_OTG_FS)->usb_adapter.handle_connect(true);
        }
    }

    extern "C" void HAL_PCD_DisconnectCallback(PCD_HandleTypeDef*) {
        if (hpcd_USB_OTG_FS.pData) {
            detail::system_from_pcd(&hpcd_USB_OTG_FS)->usb_adapter.handle_connect(false);
        }
    }

    int run() {
        System sys{};
        detail::init_usb_ops(sys);

        using PumpCaps = charm::system::SystemCaps<
            kernel::NoopIrqGuard,
            kernel::NoopWakeup>;

        PumpCaps pump_caps{};
        charm::system::AppHost<PumpCaps> host{pump_caps};

        charm::system::ClockOps clock_ops{
            &detail::platform_now_ms,
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

        block::SdmmcHandle sdmmc_handle{&sys.sdmmc_ctx, &detail::kSdmmcOps};
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
        usb_desc.dcd = sys.usb_dcd_ops;
        usb_desc.dcd_ctx = &sys;
        usb_desc.adapter = &sys.usb_adapter;
        usb_desc.dev_info.vendor_id = 0x1209;
        usb_desc.dev_info.product_id = 0x0002;
        usb_desc.dev_info.i_manufacturer = 1;
        usb_desc.dev_info.i_product = 2;
        usb_desc.dev_info.i_serial = 3;
        usb_desc.msc_cfg.ep_out = 0x01;
        usb_desc.msc_cfg.ep_in = 0x81;
        usb_desc.msc_cfg.ep_mps = 64;
        usb_desc.strings = std::span<const std::span<const usb::u8>>(
            detail::kUsbStrings.entries.data(), detail::kUsbStrings.entries.size());
        usb_desc.storage_cfg.read_only = true;
        usb_desc.on_ready = [](void* ctx,
                               usb::class_driver::MscBot* bot,
                               const usb::class_driver::MscConfig* cfg) noexcept {
            auto* sys = static_cast<System*>(ctx);
            if (!sys) return;
            sys->msc_bot = bot;
            sys->msc_cfg = cfg;
            for (auto& c : sys->usb_out_ctxs) c = bot;
            for (auto& c : sys->usb_in_ctxs) c = bot;
        };
        usb_desc.on_ready_ctx = &sys;

        charm::system::UsbMscBlockInitChain<block::Registry<kMaxEndpoints>> usb_chain{
            core.block_registry, usb_desc
        };

        const init::Node platform_node{
            "platform.init",
            init::Phase::early,
            static_cast<util::u32>(init::Runlevel::all),
            {},
            {},
            [](void* ctx) noexcept -> util::Result<void> {
                auto* sys_ctx = static_cast<System*>(ctx);
                if (!sys_ctx) return util::unexpected(util::Errc::invalid_arg);
                detail::init_platform(*sys_ctx);
                return {};
            },
            nullptr,
            &sys
        };

        init::Graph<kMaxNodes, kMaxCaps> graph{};
        std::array<const init::Node*, kMaxNodes> nodes{};
        util::usize idx = 0;
        nodes[idx++] = &platform_node;
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
            detail::early_uart_print("boot: graph build failed\n");
            Error_Handler();
        }
        auto r_start = graph.start();
        if (!r_start) {
            detail::early_uart_print("boot: graph start failed\n");
            Error_Handler();
        }

        detail::early_uart_print("boot: usb msc ready\n");

        while (true) {
            (void)host.run_once();
            if (sys.msc_bot && sys.msc_cfg) {
                if (charm::system::time::bound()) {
                    const auto now = charm::system::time::now_ms();
                    if ((now - sys.last_msc_ms) >= 1u) {
                        sys.last_msc_ms = now;
                        (void)usb::device::examples::send_msc_in_packet(
                            sys.usb_dcd_ops,
                            sys.pcd,
                            *sys.msc_bot,
                            *sys.msc_cfg);
                    }
                }
            }
        }
    }
}
