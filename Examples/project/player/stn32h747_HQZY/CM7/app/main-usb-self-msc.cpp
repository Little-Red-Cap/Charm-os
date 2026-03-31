#include <array>
#include <cstdint>
#include <span>
#include <cstring>

#include "stm32h7xx_hal.h"

import block.registry;
import charm.port;
import charm.system.clock;
import charm.system.time;
import fs_block;
import fs_errno;
import fs_stream;
import init.node;
import out.api;
import player.stm32h7.board_sdmmc;
import player.stm32h7.board_usb;
import player.stm32h7.fs_demo_mmc;
import usb.class_msc_block;
import usb.class_msc_block.node;
import usb.common;
import usb.device_driver;
import usb.dsl;
import util.core;

extern "C" {
void Error_Handler(void);
extern PCD_HandleTypeDef hpcd_USB_OTG_FS;
bool charm_usb_use_poll_irq(void);
void MX_USB_DEVICE_Init(void);
}

namespace {
constexpr usb::u16 kLangs[] = { 0x0409 };
constexpr auto kLangDesc = usb::make_lang_id_descriptor(kLangs);
constexpr auto kVendorStr = usb::make_ascii_string_descriptor("Charm");
constexpr auto kProductStr = usb::make_ascii_string_descriptor("Charm Self MSC");
constexpr auto kSerialStr = usb::make_ascii_string_descriptor("0001");

constexpr bool kDumpUsbDesc = true;
constexpr bool kDumpUsbRegs = false;
constexpr bool kDumpLba0 = true;
constexpr bool kDumpMscIo = true;
constexpr bool kDiagBypassEp0 = false;
constexpr bool kUseStEp0Diag = false;
constexpr bool kUsbMscRemovable = false;
constexpr bool kUsbMscUseBpbCapacity = true;
constexpr bool kUsbPollIrqInLoop = true;
constexpr std::uint32_t kUsbPollBurst = 8;

static const usb::StringTable<4> kUsbStrings{
    std::array<std::span<const usb::u8>, 4>{
        std::span<const usb::u8>(kLangDesc.data(), kLangDesc.size()),
        std::span<const usb::u8>(kVendorStr.data(), kVendorStr.size()),
        std::span<const usb::u8>(kProductStr.data(), kProductStr.size()),
        std::span<const usb::u8>(kSerialStr.data(), kSerialStr.size()),
    }
};

struct UsbBlockTrace {
    static constexpr std::size_t kCacheEntries = 4;

    fs::BlockDevice* dev{nullptr};
    std::array<std::array<util::u8, 512>, kCacheEntries> cache{};
    std::array<util::u64, kCacheEntries> cache_lba{};
    std::array<bool, kCacheEntries> cache_valid{};
    std::size_t cache_head{0};
    std::uint32_t read_calls{0};
    std::uint32_t cache_hits{0};
    std::uint32_t cache_misses{0};
    std::uint32_t last_read_ms{0};
    std::uint32_t max_read_ms{0};
    util::u64 last_read_lba{0};
    bool last_read_ok{false};

    fs::Status read(util::u64 lba, std::span<util::u8> data) noexcept {
        if (!dev || !dev->read) {
            last_read_ok = false;
            return fs::Status{fs::Errc::io};
        }
        read_calls++;
        last_read_lba = lba;
        if (dev->block_size == 512 && data.size() == 512) {
            for (std::size_t i = 0; i < kCacheEntries; ++i) {
                if (cache_valid[i] && cache_lba[i] == lba) {
                    std::memcpy(data.data(), cache[i].data(), 512);
                    cache_hits++;
                    last_read_ok = true;
                    return fs::Status{fs::Errc::ok};
                }
            }
            cache_misses++;
        }
        const auto start = HAL_GetTick();
        const auto st = dev->read(dev->ctx, lba, data);
        const auto cost = HAL_GetTick() - start;
        last_read_ms = cost;
        if (cost > max_read_ms) max_read_ms = cost;
        last_read_ok = static_cast<bool>(st);
        if (last_read_ok && dev->block_size == 512 && data.size() == 512) {
            std::memcpy(cache[cache_head].data(), data.data(), 512);
            cache_lba[cache_head] = lba;
            cache_valid[cache_head] = true;
            cache_head = (cache_head + 1u) % kCacheEntries;
        }
        return st;
    }
};

fs::Status usb_block_read(void* ctx, util::u64 lba, std::span<util::u8> data) noexcept {
    auto* trace = static_cast<UsbBlockTrace*>(ctx);
    if (!trace) return fs::Status{fs::Errc::io};
    return trace->read(lba, data);
}

util::u32 bpb_total_sectors(const std::array<usb::u8, 512>& sector) noexcept {
    if (sector[510] != 0x55 || sector[511] != 0xAA) return 0;
    const auto jump = sector[0];
    if (jump != 0xEB && jump != 0xE9) return 0;
    const auto tot16 = static_cast<util::u32>(sector[19] | (sector[20] << 8));
    const auto tot32 = static_cast<util::u32>(sector[32] |
        (sector[33] << 8) | (sector[34] << 16) | (sector[35] << 24));
    return tot16 != 0 ? tot16 : tot32;
}
} // namespace

int main() {
    auto kit = charm::port::init();
    charm::system::Clock clock{nullptr, charm::system::ClockOps{&charm::port::now_ms, nullptr}};
    charm::system::time::bind(clock);
    out::Scope scope{kit.console};

    out::println<"boot: uart ok">();
    player::stm32h7::board::sdmmc_hw_init();
    out::println<"boot: sdmmc hw ok">();

    auto* dev = fs_sd_block_device();
    if (!dev) {
        out::println<"boot: sdmmc init failed">();
        Error_Handler();
    }

    std::array<usb::u8, 512> lba0{};
    util::u32 bpb_sectors = 0;
    if ((kDumpLba0 || kUsbMscUseBpbCapacity) && dev->read && dev->block_size <= 512 && dev->block_size != 0) {
        const auto st = dev->read(dev->ctx, 0,
            std::span<usb::u8>(lba0.data(), dev->block_size));
        if (st) {
            bpb_sectors = bpb_total_sectors(lba0);
            if (kDumpLba0) {
                out::println<"mbr: lba0 head {:02X} {:02X} {:02X} {:02X} {:02X} {:02X} {:02X} {:02X} {:02X} {:02X} {:02X} {:02X} {:02X} {:02X} {:02X} {:02X}">(
                    lba0[0], lba0[1], lba0[2], lba0[3], lba0[4], lba0[5], lba0[6], lba0[7],
                    lba0[8], lba0[9], lba0[10], lba0[11], lba0[12], lba0[13], lba0[14], lba0[15]);
                out::println<"mbr: lba0 tail {:02X} {:02X}">(
                    lba0[dev->block_size - 2], lba0[dev->block_size - 1]);
            }
        } else if (kDumpLba0) {
            out::println<"mbr: lba0 read failed">();
        }
    }

    UsbBlockTrace usb_trace{};
    block::Registry<4> registry{};
    registry.init();
    block::DeviceDesc sd_desc{"block.sd0", block::cap_id("block.sd0")};
    usb_trace.dev = dev;
    fs::BlockDevice usb_dev = *dev;
    usb_dev.ctx = &usb_trace;
    usb_dev.read = &usb_block_read;
    if (kUsbMscUseBpbCapacity && bpb_sectors > 0 && bpb_sectors <= usb_dev.block_count) {
        usb_dev.block_count = bpb_sectors;
    }
    if (!registry.register_device(sd_desc, usb_dev)) {
        out::println<"boot: block registry failed">();
        Error_Handler();
    }

    player::stm32h7::board::usb_hw_init();
    player::stm32h7::board::usb_enable_hooks(true);
    {
        const auto hw = player::stm32h7::board::usb_hw_diag_snapshot();
        out::println<"usb: hw rcc_src=0x{:08X} fs_clk={} gpioa_moder=0x{:08X} afr0=0x{:08X} afr1=0x{:08X} pupd=0x{:08X} dm={} dp={}">(
            hw.rcc_usb_src,
            hw.usb_fs_clk_en,
            hw.gpioa_moder,
            hw.gpioa_afr0,
            hw.gpioa_afr1,
            hw.gpioa_pupd,
            hw.pin_dm,
            hw.pin_dp);
        out::println<"usb: ep0 mps init={} in0_mps={} out0_mps={}">(
            static_cast<unsigned>(hpcd_USB_OTG_FS.Init.ep0_mps),
            static_cast<unsigned>(hpcd_USB_OTG_FS.IN_ep[0].maxpacket),
            static_cast<unsigned>(hpcd_USB_OTG_FS.OUT_ep[0].maxpacket));
    }
    if (kUseStEp0Diag) {
        player::stm32h7::board::usb_enable_hooks(false);
        MX_USB_DEVICE_Init();
        out::println<"boot: usb st ep0 enabled">();
        while (true) {
            static std::uint32_t last_ms = 0;
            static std::uint32_t irq_poll_calls = 0;
            const auto now = static_cast<std::uint32_t>(charm::port::now_ms(nullptr));
            if (kUsbPollIrqInLoop) {
                HAL_PCD_IRQHandler(&hpcd_USB_OTG_FS);
                irq_poll_calls++;
            }
            if ((now - last_ms) >= 1000u) {
                last_ms = now;
                out::println<"usb: st ep0 tick irq_poll={}">(
                    static_cast<unsigned long>(irq_poll_calls));
            }
        }
    }

    usb::device::MscBlockDesc msc_desc{};
    auto& dcd_ops = player::stm32h7::board::usb_dcd_ops();
    msc_desc.cap_name = "usb.msc0";
    msc_desc.block_cap = "block.sd0";
    msc_desc.dcd = dcd_ops;
    msc_desc.dcd_ctx = &hpcd_USB_OTG_FS;
    msc_desc.adapter = &player::stm32h7::board::usb_adapter();
    msc_desc.dev_info.vendor_id = 0x1209;
    msc_desc.dev_info.product_id = 0x0002;
    msc_desc.dev_info.i_manufacturer = 1;
    msc_desc.dev_info.i_product = 2;
    msc_desc.dev_info.i_serial = 3;
    msc_desc.msc_cfg.ep_out = 0x01;
    msc_desc.msc_cfg.ep_in = 0x81;
    msc_desc.msc_cfg.ep_mps = 64;
    msc_desc.strings = std::span<const std::span<const usb::u8>>(
        kUsbStrings.entries.data(), kUsbStrings.entries.size());
    msc_desc.storage_cfg.removable = kUsbMscRemovable;
    msc_desc.storage_cfg.read_only = true;
    msc_desc.on_ready = &player::stm32h7::board::usb_set_ready;
    msc_desc.on_ready_ctx = nullptr;

    usb::device::MscBlockBinding<block::Registry<4>> binding{
        registry, msc_desc, init::Phase::app, static_cast<util::u32>(init::Runlevel::all)
    };
    auto init_st = decltype(binding)::init_trampoline(&binding);
    if (!init_st) {
        out::println<"boot: usb msc init failed {}">(static_cast<int>(init_st.error()));
        Error_Handler();
    }
    const auto dev_desc = std::span<const usb::u8>(
        binding.dev_desc.data(), binding.dev_desc.size());
    const auto cfg_desc = binding.tree.view;
    if (kDumpUsbDesc) {
        out::println<"usb: dev_desc size={}">(static_cast<unsigned>(sizeof(usb::DeviceDescriptor)));
        out::println<"usb: dev_desc bytes={} {} {} {} {} {} {} {} {} {} {} {} {} {} {} {} {} {}">(
            dev_desc[0], dev_desc[1], dev_desc[2], dev_desc[3],
            dev_desc[4], dev_desc[5], dev_desc[6], dev_desc[7],
            dev_desc[8], dev_desc[9], dev_desc[10], dev_desc[11],
            dev_desc[12], dev_desc[13], dev_desc[14], dev_desc[15],
            dev_desc[16], dev_desc[17]);
        if (cfg_desc.size() >= 9) {
            const std::uint16_t total =
                static_cast<std::uint16_t>(cfg_desc[2] | (cfg_desc[3] << 8));
            out::println<"usb: cfg_desc size={} total={} head={} {} {} {} {} {} {} {} {}">(
                static_cast<unsigned>(cfg_desc.size()),
                total,
                cfg_desc[0], cfg_desc[1], cfg_desc[2], cfg_desc[3],
                cfg_desc[4], cfg_desc[5], cfg_desc[6], cfg_desc[7], cfg_desc[8]);
            const auto max_dump = static_cast<std::size_t>(
                (cfg_desc.size() > 35) ? 35 : cfg_desc.size());
            out::println<"usb: cfg_desc bytes0={} {} {} {} {} {} {} {} {} {} {} {} {} {} {} {}">(
                cfg_desc[0], cfg_desc[1], cfg_desc[2], cfg_desc[3],
                cfg_desc[4], cfg_desc[5], cfg_desc[6], cfg_desc[7],
                cfg_desc[8], cfg_desc[9], cfg_desc[10], cfg_desc[11],
                cfg_desc[12], cfg_desc[13], cfg_desc[14], cfg_desc[15]);
            if (max_dump > 16) {
                out::println<"usb: cfg_desc bytes1={} {} {} {} {} {} {} {} {} {} {} {} {} {} {} {}">(
                    cfg_desc[16], cfg_desc[17], cfg_desc[18], cfg_desc[19],
                    cfg_desc[20], cfg_desc[21], cfg_desc[22], cfg_desc[23],
                    cfg_desc[24], cfg_desc[25], cfg_desc[26], cfg_desc[27],
                    cfg_desc[28], cfg_desc[29], cfg_desc[30], cfg_desc[31]);
            }
            if (max_dump > 32) {
                out::println<"usb: cfg_desc bytes2={} {} {}">(
                    cfg_desc[32], cfg_desc[33], cfg_desc[34]);
            }
        } else {
            out::println<"usb: cfg_desc size={}">(
                static_cast<unsigned>(cfg_desc.size()));
        }
    }
    if (kDiagBypassEp0) {
        player::stm32h7::board::usb_set_diag_descriptors(true, dev_desc, cfg_desc);
        out::println<"usb: diag ep0 enabled">();
    } else {
        player::stm32h7::board::usb_set_diag_descriptors(
            false, std::span<const usb::u8>{}, std::span<const usb::u8>{});
    }
    if (HAL_PCD_Start(&hpcd_USB_OTG_FS) != HAL_OK) {
        out::println<"boot: usb start failed">();
    }
    (void)HAL_PCD_EP_Open(&hpcd_USB_OTG_FS, 0x00, 64, EP_TYPE_CTRL);
    (void)HAL_PCD_EP_Open(&hpcd_USB_OTG_FS, 0x80, 64, EP_TYPE_CTRL);
    out::println<"usb: ep0 open in_mps={} out_mps={}">(
        static_cast<unsigned>(hpcd_USB_OTG_FS.IN_ep[0].maxpacket),
        static_cast<unsigned>(hpcd_USB_OTG_FS.OUT_ep[0].maxpacket));
    player::stm32h7::board::usb_set_started(true);
    (void)player::stm32h7::board::usb_dcd_ops().connect(&hpcd_USB_OTG_FS, true);
    out::println<"boot: usb start ok">();
    out::println<"boot: usb self msc ok">();
    if (kDumpUsbRegs) {
        const auto* usb = USB_OTG_FS;
        const auto* usb_dev = reinterpret_cast<USB_OTG_DeviceTypeDef*>(
            USB_OTG_FS_PERIPH_BASE + USB_OTG_DEVICE_BASE);
        const auto* in0 = reinterpret_cast<USB_OTG_INEndpointTypeDef*>(
            USB_OTG_FS_PERIPH_BASE + USB_OTG_IN_ENDPOINT_BASE);
        const auto* out0 = reinterpret_cast<USB_OTG_OUTEndpointTypeDef*>(
            USB_OTG_FS_PERIPH_BASE + USB_OTG_OUT_ENDPOINT_BASE);
        out::println<"usb: reg gusbcfg=0x{:08X} gahbcfg=0x{:08X} gintsts=0x{:08X} gintmsk=0x{:08X} dctl=0x{:08X} dsts=0x{:08X} gotgctl=0x{:08X} gccfg=0x{:08X}">(
            static_cast<std::uint32_t>(usb->GUSBCFG),
            static_cast<std::uint32_t>(usb->GAHBCFG),
            static_cast<std::uint32_t>(usb->GINTSTS),
            static_cast<std::uint32_t>(usb->GINTMSK),
            static_cast<std::uint32_t>(usb_dev->DCTL),
            static_cast<std::uint32_t>(usb_dev->DSTS),
            static_cast<std::uint32_t>(usb->GOTGCTL),
            static_cast<std::uint32_t>(usb->GCCFG));
        out::println<"usb: ep0 diepctl=0x{:08X} diepint=0x{:08X} doepctl=0x{:08X} doepint=0x{:08X}">(
            static_cast<std::uint32_t>(in0->DIEPCTL),
            static_cast<std::uint32_t>(in0->DIEPINT),
            static_cast<std::uint32_t>(out0->DOEPCTL),
            static_cast<std::uint32_t>(out0->DOEPINT));
    }

    while (true) {
        static std::uint32_t last_ms = 0;
        static std::uint32_t irq_poll_calls = 0;
        const auto now = static_cast<std::uint32_t>(charm::port::now_ms(nullptr));
        if (kUsbPollIrqInLoop) {
            HAL_PCD_IRQHandler(&hpcd_USB_OTG_FS);
            irq_poll_calls++;
        }
        if ((now - last_ms) >= 1000u) {
            const auto diag = player::stm32h7::board::usb_diag_snapshot();
            out::println<"usb: setup={} out0={} in0={} out1={} in1={} reset={} conn={} set_cfg={} last_cfg={} cfg_ok={} cfg_out_ok={} cfg_in_ok={} cfg_arm_ok={} set_addr={} last_addr={} addr_ok={} set_addr_nz={} last_addr_nz={} class_setup={} class_bm=0x{:02X} class_b=0x{:02X} class_wv=0x{:04X} class_wl=0x{:04X} ep0_in={} ep0_fail={} ep0_bytes={} ep0_zlp={} ep0_last_len={} ep0_last_zlp={} diag_hits={} diag_tx={} diag_bytes={} diag_zlp={} diag_out_zlp={} diag_out_len={} irq_poll={} bm=0x{:02X} b=0x{:02X} wv=0x{:04X} wi=0x{:04X} wl=0x{:04X} raw={:02X} {:02X} {:02X} {:02X} {:02X} {:02X} {:02X} {:02X}">(
                diag.setup_calls,
                diag.out0_calls,
                diag.in0_calls,
                diag.out1_calls,
                diag.in1_calls,
                diag.reset_calls,
                diag.connect_calls,
                diag.set_cfg_calls,
                diag.set_cfg_last,
                diag.set_cfg_last_ok,
                diag.set_cfg_open_out_ok,
                diag.set_cfg_open_in_ok,
                diag.set_cfg_arm_out_ok,
                diag.set_addr_calls,
                diag.set_addr_last,
                diag.set_addr_last_ok,
                diag.set_addr_nonzero_calls,
                diag.set_addr_last_nonzero,
                diag.class_setup_calls,
                diag.class_last_bm,
                diag.class_last_b,
                diag.class_last_wv,
                diag.class_last_wl,
                diag.ep0_in_calls,
                diag.ep0_in_fail,
                diag.ep0_in_bytes,
                diag.ep0_in_zlp,
                diag.ep0_last_len,
                diag.ep0_last_zlp,
                diag.diag_ep0_hits,
                diag.diag_ep0_tx,
                diag.diag_ep0_bytes,
                diag.diag_ep0_zlp,
                diag.diag_ep0_out_zlp,
                diag.diag_ep0_out_last_len,
                irq_poll_calls,
                diag.bm_request_type,
                diag.b_request,
                diag.w_value,
                diag.w_index,
                diag.w_length,
                diag.setup_raw[0],
                diag.setup_raw[1],
                diag.setup_raw[2],
                diag.setup_raw[3],
                diag.setup_raw[4],
                diag.setup_raw[5],
                diag.setup_raw[6],
                diag.setup_raw[7]);
            if (binding.bot) {
                const auto& bot = *binding.bot;
                out::println<"msc: cbw={} cmd=0x{:02X} lun={} xfer={} flags=0x{:02X} tag=0x{:08X} scsi=0x{:02X} st={} lba={} blocks={} bsize={} sense={:02X}/{:02X}/{:02X} rc={} rf={} rc_lba={} rc_blocks={} rc_bsize={} rf_blocks={} rf_bsize={} csw={} csw_st={} csw_res={} csw_tag=0x{:08X}">(
                    bot.cbw_count(),
                    bot.last_cbw_cmd(),
                    bot.last_cbw_lun(),
                    static_cast<unsigned long>(bot.last_cbw_xfer()),
                    bot.last_cbw_flags(),
                    static_cast<unsigned long>(bot.last_cbw_tag()),
                    bot.last_scsi_cmd(),
                    bot.last_scsi_status(),
                    static_cast<unsigned long>(bot.last_scsi_lba()),
                    static_cast<unsigned long>(bot.last_scsi_blocks()),
                    static_cast<unsigned long>(bot.last_scsi_block_size()),
                    bot.last_sense_key(),
                    bot.last_sense_asc(),
                    bot.last_sense_ascq(),
                    bot.read_capacity_calls(),
                    bot.read_format_calls(),
                    static_cast<unsigned long>(bot.last_rc_lba()),
                    static_cast<unsigned long>(bot.last_rc_blocks()),
                    static_cast<unsigned long>(bot.last_rc_bsize()),
                    static_cast<unsigned long>(bot.last_rf_blocks()),
                    static_cast<unsigned long>(bot.last_rf_bsize()),
                    bot.csw_count(),
                    bot.last_csw_status(),
                    static_cast<unsigned long>(bot.last_csw_residue()),
                    static_cast<unsigned long>(bot.last_csw_tag()));
            }
            if (kDumpMscIo) {
                out::println<"msc: io reads={} hits={} miss={} last_lba={} last_ms={} max_ms={} last_ok={}">(
                    usb_trace.read_calls,
                    usb_trace.cache_hits,
                    usb_trace.cache_misses,
                    static_cast<unsigned long>(usb_trace.last_read_lba),
                    usb_trace.last_read_ms,
                    usb_trace.max_read_ms,
                    usb_trace.last_read_ok ? 1u : 0u);
            }
            if (diag.setup_hist_count > 0) {
                const std::uint8_t count = diag.setup_hist_count;
                const std::uint8_t head = diag.setup_hist_head;
                out::println<"usb: hist count={} head={} [0]=bm{:02X} b{:02X} wv{:04X} wl{:04X} [1]=bm{:02X} b{:02X} wv{:04X} wl{:04X} [2]=bm{:02X} b{:02X} wv{:04X} wl{:04X} [3]=bm{:02X} b{:02X} wv{:04X} wl{:04X}">(
                    count,
                    head,
                    diag.setup_bm[0], diag.setup_b[0], diag.setup_wv[0], diag.setup_wl[0],
                    diag.setup_bm[1], diag.setup_b[1], diag.setup_wv[1], diag.setup_wl[1],
                    diag.setup_bm[2], diag.setup_b[2], diag.setup_wv[2], diag.setup_wl[2],
                    diag.setup_bm[3], diag.setup_b[3], diag.setup_wv[3], diag.setup_wl[3]);
            }
            last_ms = now;
        }
        for (std::uint32_t i = 0; i < kUsbPollBurst; ++i) {
            player::stm32h7::board::usb_poll_msc(&hpcd_USB_OTG_FS);
        }
        charm::system::time::sleep_ms(1);
    }
}
extern "C" bool charm_usb_use_poll_irq(void) {
    return kUsbPollIrqInLoop;
}
