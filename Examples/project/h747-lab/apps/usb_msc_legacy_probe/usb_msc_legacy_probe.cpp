#include "usb_msc_legacy_probe.h"

#include "console_service.hpp"
#include "port.h"
#include "storage.h"
#include "stm32h7xx_hal.h"
#include "usb_msc_legacy_service.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string_view>
#include <utility>

import out.core;
import out.format;

namespace h747::apps::usb_msc_legacy_probe {
namespace {

using namespace std::literals::string_view_literals;

template <charm::cap::ByteSink Sink>
class OutSinkAdapter {
public:
    explicit OutSinkAdapter(Sink& sink) : sink_(&sink) {}

    out::result<std::size_t> write(const out::bytes bytes) noexcept {
        if (sink_ == nullptr) {
            return out::ok<std::size_t>(0U);
        }
        const auto transfer = sink_->write(bytes);
        return out::ok(static_cast<std::size_t>(transfer.bytes));
    }

    out::result<std::size_t> flush() noexcept {
        if (sink_ != nullptr) {
            (void)sink_->flush();
        }
        return out::ok<std::size_t>(0U);
    }

private:
    Sink* sink_{nullptr};
};

h747::console::ConsoleStream& console_stream() noexcept {
    static h747::console::ConsoleStream stream{};
    return stream;
}

OutSinkAdapter<h747::console::ConsoleStream>& out_sink() noexcept {
    static OutSinkAdapter adapter{console_stream()};
    return adapter;
}

template <out::fixed_string Fmt, class... Args>
void emit(Args&&... args) noexcept {
    out::discard(out::vprint<Fmt>(out_sink(), std::forward<Args>(args)...));
}

h747::console::ConsoleLineSource line_source;
std::uint32_t last_tick_ms = 0U;
std::uint32_t alive_count = 0U;
std::uint32_t last_alive_setup_count = 0U;
std::uint32_t last_alive_reset_count = 0U;
std::uint32_t last_alive_read_count = 0U;
std::uint32_t last_alive_write_count = 0U;
alignas(32) std::uint8_t write_smoke_before[512]{};
alignas(32) std::uint8_t write_smoke_after[512]{};
alignas(32) std::uint8_t lba_dump[512]{};
alignas(32) std::uint8_t block_zero[512]{};
alignas(32) std::uint8_t mbr_block[512]{};
alignas(32) std::uint8_t fat32_boot_block[512]{};
alignas(32) std::uint8_t fat32_fsinfo_block[512]{};
alignas(32) std::uint8_t fat32_fat_block[512]{};
alignas(32) std::uint8_t zero_chunk[512 * 8]{};

void emit_text(const char* text) noexcept {
    if (text == nullptr) {
        return;
    }
    h747::console::write(text);
}

void emit_hex_bytes(const char* label, const std::uint8_t* bytes, const std::size_t len) noexcept {
    if (label != nullptr) {
        emit_text(label);
    }
    static constexpr char kHex[] = "0123456789ABCDEF";
    char buf[4] = {' ', '0', '0', '\0'};
    for (std::size_t i = 0; i < len; ++i) {
        buf[1] = kHex[(bytes[i] >> 4U) & 0x0FU];
        buf[2] = kHex[bytes[i] & 0x0FU];
        emit_text(buf);
    }
    emit_text("\n");
}

void emit_setup_packet(const char* label, const std::uint8_t* bytes) noexcept {
    emit_hex_bytes(label, bytes, 8U);
}

std::uint16_t u16le(const std::uint8_t* p) noexcept {
    return static_cast<std::uint16_t>(p[0])
         | static_cast<std::uint16_t>(static_cast<std::uint16_t>(p[1]) << 8U);
}

std::uint32_t u32le(const std::uint8_t* p) noexcept {
    return static_cast<std::uint32_t>(p[0])
         | (static_cast<std::uint32_t>(p[1]) << 8U)
         | (static_cast<std::uint32_t>(p[2]) << 16U)
         | (static_cast<std::uint32_t>(p[3]) << 24U);
}

void put_u16le(std::uint8_t* p, const std::uint16_t value) noexcept {
    p[0] = static_cast<std::uint8_t>(value & 0xFFU);
    p[1] = static_cast<std::uint8_t>((value >> 8U) & 0xFFU);
}

void put_u32le(std::uint8_t* p, const std::uint32_t value) noexcept {
    p[0] = static_cast<std::uint8_t>(value & 0xFFU);
    p[1] = static_cast<std::uint8_t>((value >> 8U) & 0xFFU);
    p[2] = static_cast<std::uint8_t>((value >> 16U) & 0xFFU);
    p[3] = static_cast<std::uint8_t>((value >> 24U) & 0xFFU);
}

void print_prompt() {
    emit<"\r\nh747-legacy-msc> ">();
}

void print_help() {
    emit<"Commands:\n">();
    emit<"  help            - Show help\n">();
    emit<"  status          - Print legacy USB MSC evidence\n">();
    emit<"  storage status  - Print eMMC evidence\n">();
    emit<"  usb status      - Print short USB link evidence\n">();
    emit<"  write smoke     - Read/write/read-back LBA0 without changing content\n">();
    emit<"  dump lba0       - Print LBA0 BPB/MBR evidence\n">();
    emit<"  dump part       - Print partition boot sector evidence at LBA 2048\n">();
    emit<"  wipe lba0       - Detach USB and clear LBA0\n">();
    emit<"  init mbr        - Detach USB and write a standard FAT32 MBR shell\n">();
    emit<"  init fat32      - Detach USB and write a minimal empty FAT32 volume\n">();
    emit<"  usb detach      - Stop USB MSC before direct eMMC diagnostics\n">();
    emit<"  usb attach      - Start USB MSC after storage is idle\n">();
    emit<"  usb attach window - Attach for 20s, print link evidence, then detach\n">();
    emit<"  usb readonly    - Report USB MSC as write protected\n">();
    emit<"  usb writable    - Allow host writes for explicit write tests\n">();
    emit<"  reboot          - Reboot\n">();
}

void print_storage_status() {
    const auto storage = h747_storage_state();
    emit<"storage: ready={}/{} fat={} reads={} rfails={} writes={} wfails={} blk={} count={} part_lba={} hal={} err=0x{:08X} card={} wait_to={} last={}/{} clkcr=0x{:08X} sta=0x{:08X} resp1=0x{:08X} bus={} w8={} w4={} w1={}\n">(
        storage.initialized,
        storage.ready,
        storage.fat_probe_ok,
        storage.read_count,
        storage.read_fail_count,
        storage.write_count,
        storage.write_fail_count,
        storage.block_size,
        storage.exposed_block_count,
        storage.partition_lba,
        storage.last_hal_status,
        storage.last_error,
        storage.card_state,
        storage.wait_timeout_count,
        storage.last_lba,
        storage.last_count,
        storage.clkcr,
        storage.sta,
        storage.resp1,
        storage.selected_bus_width,
        storage.wide_status_8,
        storage.wide_status_4,
        storage.wide_status_1);
}

void print_usb_link_summary() {
    const auto usb = h747::usb_msc_legacy::state();
    emit<"usb_link: start={} dev={}/{} addr={} cfg={} conf={} setup={} reset={} reads={} writes={} scsi={} bad_cbw={} last_op=0x{:02X} lba={} blocks={} dm={} dp={} dctl=0x{:08X} dsts=0x{:08X} sdis={}/{} pkt={} ra={} window={}/{} ep1={}/{} stall={}/{} clr={}/{} open={} close={} addr_set={} setcfg={} getlun={} botreset={}\n">(
        usb.started,
        static_cast<unsigned>(usb.dev_state),
        static_cast<unsigned>(usb.dev_old_state),
        static_cast<unsigned>(usb.dev_address),
        usb.dev_config,
        static_cast<unsigned>(usb.conf_idx),
        usb.setup_count,
        usb.reset_count,
        usb.read_calls,
        usb.write_calls,
        usb.scsi_cbw_count,
        usb.scsi_bad_cbw_count,
        static_cast<unsigned>(usb.scsi_last_opcode),
        usb.scsi_last_lba,
        usb.scsi_last_blocks,
        usb.gpio_dm,
        usb.gpio_dp,
        usb.dctl,
        usb.dsts,
        usb.soft_disconnect_before_start,
        usb.soft_disconnect_after_start,
        usb.packet_bytes,
        usb.read_ahead_blocks,
        usb.cache_window_lba,
        usb.cache_window_blocks,
        usb.out_ep1_hits,
        usb.in_ep1_hits,
        usb.stall_ep1_out_hits,
        usb.stall_ep1_in_hits,
        usb.clear_stall_ep1_out_hits,
        usb.clear_stall_ep1_in_hits,
        usb.open_ep_count,
        usb.close_ep_count,
        usb.set_address_count,
        usb.setup_std_set_configuration,
        usb.setup_class_get_max_lun,
        usb.setup_class_bot_reset);
}

void run_usb_attach_window() {
    const auto attach_status = h747::usb_msc_legacy::attach();
    emit<"usb_attach_window: attach_status={}\n">(attach_status);
    print_usb_link_summary();
    for (std::uint32_t i = 0U; i < 4U; ++i) {
        HAL_Delay(5000U);
        const auto storage = h747_storage_state();
        const auto usb = h747::usb_msc_legacy::state();
        emit<"usb_attach_window: sample={} dev={}/{} addr={} cfg={} conf={} setup={} reset={} conn={}/{} sus={}/{} reads={} writes={} rfails={} wfails={} scsi={} bad_cbw={} tur={} inq={} cap={} read10={} reqsense={} modes6={} modes10={} write10={} other={} last_op=0x{:02X} lba={} blocks={} pkt={} ra={} window={}/{} ep1={}/{} stall={}/{} clr={}/{} open={} close={} addr_set={} setcfg={} getlun={} botreset={}\n">(
            i + 1U,
            static_cast<unsigned>(usb.dev_state),
            static_cast<unsigned>(usb.dev_old_state),
            static_cast<unsigned>(usb.dev_address),
            usb.dev_config,
            static_cast<unsigned>(usb.conf_idx),
            usb.setup_count,
            usb.reset_count,
            usb.connect_count,
            usb.disconnect_count,
            usb.suspend_count,
            usb.resume_count,
            usb.read_calls,
            usb.write_calls,
            storage.read_fail_count,
            storage.write_fail_count,
            usb.scsi_cbw_count,
            usb.scsi_bad_cbw_count,
            usb.scsi_test_unit_ready_count,
            usb.scsi_inquiry_count,
            usb.scsi_read_capacity10_count,
            usb.scsi_read10_count,
            usb.scsi_request_sense_count,
            usb.scsi_mode_sense6_count,
            usb.scsi_mode_sense10_count,
            usb.scsi_write10_count,
            usb.scsi_other_count,
            static_cast<unsigned>(usb.scsi_last_opcode),
            usb.scsi_last_lba,
            usb.scsi_last_blocks,
            usb.packet_bytes,
            usb.read_ahead_blocks,
            usb.cache_window_lba,
            usb.cache_window_blocks,
            usb.out_ep1_hits,
            usb.in_ep1_hits,
            usb.stall_ep1_out_hits,
            usb.stall_ep1_in_hits,
            usb.clear_stall_ep1_out_hits,
            usb.clear_stall_ep1_in_hits,
            usb.open_ep_count,
            usb.close_ep_count,
            usb.set_address_count,
            usb.setup_std_set_configuration,
            usb.setup_class_get_max_lun,
            usb.setup_class_bot_reset);
    }
    h747::usb_msc_legacy::detach();
    emit<"usb_attach_window: detached=1\n">();
    print_usb_link_summary();
}

void print_status() {
    const auto usb = h747::usb_msc_legacy::state();
    emit<"legacy_msc_probe: profile=usb_msc_legacy_probe tick={} alive={}\n">(
        h747::port::tick_ms(),
        alive_count);
    print_storage_status();
    emit<"legacy_usb_msc: init={} start={} ready={} writable={} blk={} count={} part_lba={} init_calls={} ready_calls={} cap_calls={} read_calls={} write_calls={} last_err={}\n">(
        usb.initialized,
        usb.started,
        usb.ready,
        usb.write_enabled,
        usb.block_size,
        usb.block_count,
        usb.partition_lba,
        usb.init_calls,
        usb.ready_calls,
        usb.capacity_calls,
        usb.read_calls,
        usb.write_calls,
        usb.last_error);
    emit<"legacy_usb_io: cache={}/{} stores={} inv={} blocks={}/{} max={}/{} last_read={}/{} last_write={}/{} pkt={} ra={} window={}/{}\n">(
        usb.cache_hits,
        usb.cache_misses,
        usb.cache_stores,
        usb.cache_invalidations,
        usb.read_blocks,
        usb.write_blocks,
        usb.max_read_len,
        usb.max_write_len,
        usb.last_read_lba,
        usb.last_read_len,
        usb.last_write_lba,
        usb.last_write_len,
        usb.packet_bytes,
        usb.read_ahead_blocks,
        usb.cache_window_lba,
        usb.cache_window_blocks);
    emit<"legacy_usb_init: pcd_ready={} pcd_st={} dev_init={} usbd={} class={} storage={} start={} vbus={} sdis={}/{}\n">(
        usb.pcd_ready,
        usb.pcd_init_status,
        usb.device_init_called,
        usb.usbd_init_status,
        usb.register_class_status,
        usb.register_storage_status,
        usb.usbd_start_status,
        usb.vbus_detector_enabled,
        usb.soft_disconnect_before_start,
        usb.soft_disconnect_after_start);
    emit<"legacy_usb_state: dev={}/{} addr={} conn={} speed={} cfg={}/{} cfg_status={} conf_idx={} ep0={}/{} class={}/{} ep1_used={}/{}\n">(
        static_cast<unsigned>(usb.dev_state),
        static_cast<unsigned>(usb.dev_old_state),
        static_cast<unsigned>(usb.dev_address),
        static_cast<unsigned>(usb.dev_connection_status),
        static_cast<unsigned>(usb.dev_speed),
        usb.dev_config,
        usb.dev_default_config,
        usb.dev_config_status,
        static_cast<unsigned>(usb.conf_idx),
        usb.ep0_state,
        usb.ep0_data_len,
        usb.class_id,
        usb.num_classes,
        static_cast<unsigned>(usb.ep1_out_used),
        static_cast<unsigned>(usb.ep1_in_used));
    emit<"legacy_usb_events: setup={} reset={} connect={} disconnect={} suspend={} resume={} ep0={}/{} ep1={}/{} stall={}/{} clr={}/{} open={} close={} addr={} last_setup={}\n">(
        usb.setup_count,
        usb.reset_count,
        usb.connect_count,
        usb.disconnect_count,
        usb.suspend_count,
        usb.resume_count,
        usb.out_ep0_hits,
        usb.in_ep0_hits,
        usb.out_ep1_hits,
        usb.in_ep1_hits,
        usb.stall_ep1_out_hits,
        usb.stall_ep1_in_hits,
        usb.clear_stall_ep1_out_hits,
        usb.clear_stall_ep1_in_hits,
        usb.open_ep_count,
        usb.close_ep_count,
        usb.set_address_count,
        usb.last_setup_valid);
    emit<"legacy_usb_setup_counts: std_status={} clrfeat={} setfeat={} setaddr={} getdesc={}/{}/{}/{}/{} getcfg={} setcfg={} getitf={} setitf={} std_other={} getlun={} botreset={} class_other={} vendor={} type_other={}\n">(
        usb.setup_std_get_status,
        usb.setup_std_clear_feature,
        usb.setup_std_set_feature,
        usb.setup_std_set_address,
        usb.setup_std_get_descriptor_device,
        usb.setup_std_get_descriptor_config,
        usb.setup_std_get_descriptor_string,
        usb.setup_std_get_descriptor_qualifier,
        usb.setup_std_get_descriptor_other,
        usb.setup_std_get_configuration,
        usb.setup_std_set_configuration,
        usb.setup_std_get_interface,
        usb.setup_std_set_interface,
        usb.setup_std_other,
        usb.setup_class_get_max_lun,
        usb.setup_class_bot_reset,
        usb.setup_class_other,
        usb.setup_vendor,
        usb.setup_type_other);
    emit<"legacy_usb_scsi: cbw={} bad={} tur={} sense={} inquiry={} startstop={} allow={} modes6={} modes10={} fmtcap={} cap10={} cap16={} read10={} read12={} write10={} write12={} verify10={} other={} last={}/0x{:02X}/lun{}/cb{}/flags0x{:02X}/tag{}/len{}/lba{}/blocks{}\n">(
        usb.scsi_cbw_count,
        usb.scsi_bad_cbw_count,
        usb.scsi_test_unit_ready_count,
        usb.scsi_request_sense_count,
        usb.scsi_inquiry_count,
        usb.scsi_start_stop_count,
        usb.scsi_allow_removal_count,
        usb.scsi_mode_sense6_count,
        usb.scsi_mode_sense10_count,
        usb.scsi_read_format_capacity_count,
        usb.scsi_read_capacity10_count,
        usb.scsi_read_capacity16_count,
        usb.scsi_read10_count,
        usb.scsi_read12_count,
        usb.scsi_write10_count,
        usb.scsi_write12_count,
        usb.scsi_verify10_count,
        usb.scsi_other_count,
        usb.scsi_last_valid,
        static_cast<unsigned>(usb.scsi_last_opcode),
        static_cast<unsigned>(usb.scsi_last_lun),
        static_cast<unsigned>(usb.scsi_last_cb_len),
        static_cast<unsigned>(usb.scsi_last_flags),
        usb.scsi_last_tag,
        usb.scsi_last_data_len,
        usb.scsi_last_lba,
        usb.scsi_last_blocks);
    emit<"legacy_usb_regs: gusbcfg=0x{:08X} gahbcfg=0x{:08X} gintsts=0x{:08X} gintmsk=0x{:08X} masked=0x{:08X} gotgint=0x{:08X} dctl=0x{:08X} dsts=0x{:08X} gotgctl=0x{:08X} gccfg=0x{:08X}\n">(
        usb.gusbcfg,
        usb.gahbcfg,
        usb.gintsts,
        usb.gintmsk,
        usb.gint_masked,
        usb.gotgint,
        usb.dctl,
        usb.dsts,
        usb.gotgctl,
        usb.gccfg);
    emit<"legacy_usb_dev: daint=0x{:08X} daintmsk=0x{:08X} doepmsk=0x{:08X} diepmsk=0x{:08X} diepctl0=0x{:08X} diepint0=0x{:08X} doepctl0=0x{:08X} doepint0=0x{:08X}\n">(
        usb.daint,
        usb.daintmsk,
        usb.doepmsk,
        usb.diepmsk,
        usb.diepctl0,
        usb.diepint0,
        usb.doepctl0,
        usb.doepint0);
    emit<"legacy_usb_hw: gpio_moder=0x{:08X} pupdr=0x{:08X} idr=0x{:08X} afrh=0x{:08X} dm={} dp={} rcc_usbsel=0x{:08X} ahb1enr=0x{:08X} nvic={}/{}/{}\n">(
        usb.gpioa_moder,
        usb.gpioa_pupdr,
        usb.gpioa_idr,
        usb.gpioa_afrh,
        usb.gpio_dm,
        usb.gpio_dp,
        usb.rcc_usbsel,
        usb.rcc_ahb1enr,
        usb.nvic_enable_otg_fs,
        usb.nvic_pending_otg_fs,
        usb.nvic_active_otg_fs);
    if (usb.last_setup_valid != 0U) {
        emit_hex_bytes("legacy_usb_setup:", usb.last_setup, sizeof(usb.last_setup));
    }
    for (std::uint8_t slot = 0U; slot < usb.setup_history_count && slot < 4U; ++slot) {
        emit_setup_packet("legacy_usb_setup_hist:", usb.setup_history[slot]);
    }
    emit<"legacy_usb_desc: dev_len={} cfg_len={} dev_prefix={} cfg_prefix={}\n">(
        usb.dev_desc_len,
        usb.cfg_desc_len,
        usb.dev_desc_prefix_len,
        usb.cfg_desc_prefix_len);
    if (usb.dev_desc_prefix_len != 0U) {
        emit_hex_bytes("legacy_usb_dev_desc:", usb.dev_desc_prefix, usb.dev_desc_prefix_len);
    }
    if (usb.cfg_desc_prefix_len != 0U) {
        emit_hex_bytes("legacy_usb_cfg_desc:", usb.cfg_desc_prefix, usb.cfg_desc_prefix_len);
    }
}

void run_write_smoke() {
    const auto size = h747_storage_block_size();
    if (size != sizeof(write_smoke_before)) {
        emit<"write_smoke: ok=0 lba=0 read1=0 write=0 read2=0 same=0 reason=block_size size={}\n">(size);
        return;
    }

    std::memset(write_smoke_before, 0, sizeof(write_smoke_before));
    std::memset(write_smoke_after, 0, sizeof(write_smoke_after));

    const std::uint32_t lba = 0U;
    const auto read1 = h747_storage_read_raw_blocks(lba, write_smoke_before, sizeof(write_smoke_before));
    const auto write = (read1 != 0U)
                           ? h747_storage_write_raw_blocks(lba, write_smoke_before, sizeof(write_smoke_before))
                           : 0U;
    const auto read2 = (write != 0U)
                           ? h747_storage_read_raw_blocks(lba, write_smoke_after, sizeof(write_smoke_after))
                           : 0U;
    const auto same = (read2 != 0U && std::memcmp(write_smoke_before, write_smoke_after, sizeof(write_smoke_before)) == 0)
                          ? 1U
                          : 0U;
    const auto storage = h747_storage_state();
    const auto ok = (read1 != 0U && write != 0U && read2 != 0U && same != 0U) ? 1U : 0U;
    emit<"write_smoke: ok={} lba={} read1={} write={} read2={} same={} reads={} rfails={} writes={} wfails={} hal={} err=0x{:08X} card={} wait_to={} last={}/{} clkcr=0x{:08X} sta=0x{:08X} resp1=0x{:08X}\n">(
        ok,
        lba,
        read1,
        write,
        read2,
        same,
        storage.read_count,
        storage.read_fail_count,
        storage.write_count,
        storage.write_fail_count,
        storage.last_hal_status,
        storage.last_error,
        storage.card_state,
        storage.wait_timeout_count,
        storage.last_lba,
        storage.last_count,
        storage.clkcr,
        storage.sta,
        storage.resp1);
}

void dump_lba(const std::uint32_t lba) {
    h747::usb_msc_legacy::detach();
    HAL_Delay(50U);
    std::memset(lba_dump, 0, sizeof(lba_dump));
    const auto ok = h747_storage_read_raw_blocks(lba, lba_dump, sizeof(lba_dump));
    const auto storage = h747_storage_state();
    emit<"lba{}: read={} sig=0x{:02X}{:02X} bps={} spc={} reserved={} fats={} root_entries={} total16={} media=0x{:02X} fatsz16={} hidden={} total32={} fatsz32={} part0_type=0x{:02X} part0_lba={} part0_sectors={} hal={} err=0x{:08X} last={}/{}\n">(
        lba,
        ok,
        static_cast<unsigned>(lba_dump[511]),
        static_cast<unsigned>(lba_dump[510]),
        u16le(lba_dump + 11),
        static_cast<unsigned>(lba_dump[13]),
        u16le(lba_dump + 14),
        static_cast<unsigned>(lba_dump[16]),
        u16le(lba_dump + 17),
        u16le(lba_dump + 19),
        static_cast<unsigned>(lba_dump[21]),
        u16le(lba_dump + 22),
        u32le(lba_dump + 28),
        u32le(lba_dump + 32),
        u32le(lba_dump + 36),
        static_cast<unsigned>(lba_dump[0x1BE + 4]),
        u32le(lba_dump + 0x1BE + 8),
        u32le(lba_dump + 0x1BE + 12),
        storage.last_hal_status,
        storage.last_error,
        storage.last_lba,
        storage.last_count);
    emit_hex_bytes("lba0_hex_000:", lba_dump, 64U);
    emit_hex_bytes("lba0_hex_1BE:", lba_dump + 0x1BE, 66U);
}

void dump_lba0() {
    dump_lba(0U);
}

void dump_part() {
    dump_lba(2048U);
}

void wipe_lba0() {
    h747::usb_msc_legacy::detach();
    HAL_Delay(50U);
    std::memset(block_zero, 0, sizeof(block_zero));
    const auto write = h747_storage_write_raw_blocks(0U, block_zero, sizeof(block_zero));
    const auto read = h747_storage_read_raw_blocks(0U, lba_dump, sizeof(lba_dump));
    std::uint32_t nonzero = 0U;
    if (read != 0U) {
        for (const auto value : lba_dump) {
            if (value != 0U) {
                ++nonzero;
            }
        }
    }
    const auto storage = h747_storage_state();
    emit<"wipe_lba0: write={} read={} nonzero={} hal={} err=0x{:08X} last={}/{}\n">(
        write,
        read,
        nonzero,
        storage.last_hal_status,
        storage.last_error,
        storage.last_lba,
        storage.last_count);
}

void init_mbr() {
    static constexpr std::uint32_t kPartitionStartLba = 2048U;
    static constexpr std::uint32_t kMbrDiskSignature = 0x48474731U;

    h747::usb_msc_legacy::detach();
    HAL_Delay(50U);

    const auto raw_blocks = h747_storage_raw_block_count();
    if (raw_blocks <= (kPartitionStartLba + 4096U)) {
        emit<"init_mbr: ok=0 reason=too_small raw_blocks={}\n">(raw_blocks);
        return;
    }

    std::memset(mbr_block, 0, sizeof(mbr_block));
    std::memset(block_zero, 0, sizeof(block_zero));

    const auto partition_blocks = raw_blocks - kPartitionStartLba;
    auto* part0 = mbr_block + 0x1BE;
    part0[0] = 0x00U;
    part0[1] = 0x00U;
    part0[2] = 0x20U;
    part0[3] = 0x21U;
    part0[4] = 0x0CU;
    part0[5] = 0xFEU;
    part0[6] = 0xFFU;
    part0[7] = 0xFFU;
    put_u32le(mbr_block + 0x1B8, kMbrDiskSignature);
    put_u32le(part0 + 8, kPartitionStartLba);
    put_u32le(part0 + 12, partition_blocks);
    mbr_block[510] = 0x55U;
    mbr_block[511] = 0xAAU;

    const auto write_mbr = h747_storage_write_raw_blocks(0U, mbr_block, sizeof(mbr_block));
    const auto zero_boot = h747_storage_write_raw_blocks(kPartitionStartLba, block_zero, sizeof(block_zero));
    const auto read = h747_storage_read_raw_blocks(0U, lba_dump, sizeof(lba_dump));
    const auto verified = (read != 0U
                           && lba_dump[510] == 0x55U
                           && lba_dump[511] == 0xAAU
                           && lba_dump[0x1BE + 4] == 0x0CU
                           && u32le(lba_dump + 0x1BE + 8) == kPartitionStartLba
                           && u32le(lba_dump + 0x1BE + 12) == partition_blocks)
                              ? 1U
                              : 0U;
    const auto storage = h747_storage_state();
    const auto ok = (write_mbr != 0U && zero_boot != 0U && verified != 0U) ? 1U : 0U;
    emit<"init_mbr: ok={} write_mbr={} zero_boot={} read={} verified={} part_lba={} part_blocks={} raw_blocks={} hal={} err=0x{:08X} last={}/{}\n">(
        ok,
        write_mbr,
        zero_boot,
        read,
        verified,
        kPartitionStartLba,
        partition_blocks,
        raw_blocks,
        storage.last_hal_status,
        storage.last_error,
        storage.last_lba,
        storage.last_count);
}

std::uint32_t ceil_div_u32(const std::uint64_t value, const std::uint32_t divisor) noexcept {
    return static_cast<std::uint32_t>((value + divisor - 1U) / divisor);
}

std::uint32_t fat32_sectors_for_clusters(const std::uint32_t clusters) noexcept {
    return ceil_div_u32((static_cast<std::uint64_t>(clusters) + 2ULL) * 4ULL, 512U);
}

struct Fat32Layout {
    std::uint32_t partition_lba;
    std::uint32_t partition_blocks;
    std::uint32_t reserved_sectors;
    std::uint32_t sectors_per_fat;
    std::uint32_t data_start_rel;
    std::uint32_t data_start_lba;
    std::uint32_t cluster_count;
    std::uint8_t sectors_per_cluster;
};

bool make_fat32_layout(const std::uint32_t raw_blocks, Fat32Layout& out) noexcept {
    static constexpr std::uint32_t kPartitionStartLba = 2048U;
    static constexpr std::uint32_t kPreferredDataStartRel = 32768U;
    static constexpr std::uint32_t kMinFat32Clusters = 65525U;
    static constexpr std::uint8_t kSectorsPerCluster = 16U;
    if (raw_blocks <= (kPartitionStartLba + kPreferredDataStartRel + (kMinFat32Clusters * kSectorsPerCluster))) {
        return false;
    }

    const auto partition_blocks = raw_blocks - kPartitionStartLba;
    auto cluster_count = (partition_blocks - kPreferredDataStartRel) / kSectorsPerCluster;
    auto sectors_per_fat = fat32_sectors_for_clusters(cluster_count);
    if ((sectors_per_fat * 2U) >= kPreferredDataStartRel) {
        return false;
    }
    const auto reserved = kPreferredDataStartRel - (sectors_per_fat * 2U);
    if (reserved < 32U || reserved > 0xFFFFU) {
        return false;
    }
    cluster_count = (partition_blocks - kPreferredDataStartRel) / kSectorsPerCluster;
    if (cluster_count < kMinFat32Clusters) {
        return false;
    }

    out = Fat32Layout{
        .partition_lba = kPartitionStartLba,
        .partition_blocks = partition_blocks,
        .reserved_sectors = reserved,
        .sectors_per_fat = sectors_per_fat,
        .data_start_rel = kPreferredDataStartRel,
        .data_start_lba = kPartitionStartLba + kPreferredDataStartRel,
        .cluster_count = cluster_count,
        .sectors_per_cluster = kSectorsPerCluster,
    };
    return true;
}

void fill_mbr(const Fat32Layout& layout) noexcept {
    static constexpr std::uint32_t kMbrDiskSignature = 0x48474731U;

    std::memset(mbr_block, 0, sizeof(mbr_block));
    auto* part0 = mbr_block + 0x1BE;
    part0[0] = 0x00U;
    part0[1] = 0x00U;
    part0[2] = 0x20U;
    part0[3] = 0x21U;
    part0[4] = 0x0CU;
    part0[5] = 0xFEU;
    part0[6] = 0xFFU;
    part0[7] = 0xFFU;
    put_u32le(mbr_block + 0x1B8, kMbrDiskSignature);
    put_u32le(part0 + 8, layout.partition_lba);
    put_u32le(part0 + 12, layout.partition_blocks);
    mbr_block[510] = 0x55U;
    mbr_block[511] = 0xAAU;
}

void fill_fat32_boot(const Fat32Layout& layout) noexcept {
    std::memset(fat32_boot_block, 0, sizeof(fat32_boot_block));
    fat32_boot_block[0] = 0xEBU;
    fat32_boot_block[1] = 0x58U;
    fat32_boot_block[2] = 0x90U;
    std::memcpy(fat32_boot_block + 3, "MSDOS5.0", 8U);
    put_u16le(fat32_boot_block + 11, 512U);
    fat32_boot_block[13] = layout.sectors_per_cluster;
    put_u16le(fat32_boot_block + 14, static_cast<std::uint16_t>(layout.reserved_sectors));
    fat32_boot_block[16] = 2U;
    put_u16le(fat32_boot_block + 17, 0U);
    put_u16le(fat32_boot_block + 19, 0U);
    fat32_boot_block[21] = 0xF8U;
    put_u16le(fat32_boot_block + 22, 0U);
    put_u16le(fat32_boot_block + 24, 63U);
    put_u16le(fat32_boot_block + 26, 255U);
    put_u32le(fat32_boot_block + 28, layout.partition_lba);
    put_u32le(fat32_boot_block + 32, layout.partition_blocks);
    put_u32le(fat32_boot_block + 36, layout.sectors_per_fat);
    put_u16le(fat32_boot_block + 40, 0U);
    put_u16le(fat32_boot_block + 42, 0U);
    put_u32le(fat32_boot_block + 44, 2U);
    put_u16le(fat32_boot_block + 48, 1U);
    put_u16le(fat32_boot_block + 50, 6U);
    fat32_boot_block[64] = 0x80U;
    fat32_boot_block[66] = 0x29U;
    put_u32le(fat32_boot_block + 67, 0x74774701U);
    std::memcpy(fat32_boot_block + 71, "CHARMEMMC  ", 11U);
    std::memcpy(fat32_boot_block + 82, "FAT32   ", 8U);
    std::memcpy(fat32_boot_block + 90, "Charm H747 FAT32 volume\r\n", 25U);
    fat32_boot_block[510] = 0x55U;
    fat32_boot_block[511] = 0xAAU;
}

void fill_fat32_fsinfo(const Fat32Layout& layout) noexcept {
    std::memset(fat32_fsinfo_block, 0, sizeof(fat32_fsinfo_block));
    put_u32le(fat32_fsinfo_block + 0, 0x41615252U);
    put_u32le(fat32_fsinfo_block + 484, 0x61417272U);
    put_u32le(fat32_fsinfo_block + 488, layout.cluster_count - 1U);
    put_u32le(fat32_fsinfo_block + 492, 3U);
    put_u32le(fat32_fsinfo_block + 508, 0xAA550000U);
}

void fill_fat32_first_fat_sector() noexcept {
    std::memset(fat32_fat_block, 0, sizeof(fat32_fat_block));
    put_u32le(fat32_fat_block + 0, 0x0FFFFFF8U);
    put_u32le(fat32_fat_block + 4, 0xFFFFFFFFU);
    put_u32le(fat32_fat_block + 8, 0x0FFFFFFFU);
}

bool zero_range(std::uint32_t start_lba, std::uint32_t sectors) noexcept {
    static constexpr std::uint32_t kChunkSectors = sizeof(zero_chunk) / 512U;
    std::memset(zero_chunk, 0, sizeof(zero_chunk));
    while (sectors != 0U) {
        const auto chunk = (sectors > kChunkSectors) ? kChunkSectors : sectors;
        if (h747_storage_write_raw_blocks(start_lba, zero_chunk, chunk * 512U) == 0U) {
            return false;
        }
        start_lba += chunk;
        sectors -= chunk;
    }
    return true;
}

void init_fat32() {
    Fat32Layout layout{};
    h747::usb_msc_legacy::detach();
    HAL_Delay(50U);

    const auto raw_blocks = h747_storage_raw_block_count();
    if (!make_fat32_layout(raw_blocks, layout)) {
        emit<"init_fat32: ok=0 reason=layout raw_blocks={}\n">(raw_blocks);
        return;
    }

    fill_mbr(layout);
    fill_fat32_boot(layout);
    fill_fat32_fsinfo(layout);
    fill_fat32_first_fat_sector();

    const auto fat0_lba = layout.partition_lba + layout.reserved_sectors;
    const auto fat1_lba = fat0_lba + layout.sectors_per_fat;
    const auto root_lba = layout.data_start_lba;

    const auto write_mbr = h747_storage_write_raw_blocks(0U, mbr_block, sizeof(mbr_block));
    const auto write_boot = h747_storage_write_raw_blocks(layout.partition_lba, fat32_boot_block, sizeof(fat32_boot_block));
    const auto write_fsinfo = h747_storage_write_raw_blocks(layout.partition_lba + 1U, fat32_fsinfo_block, sizeof(fat32_fsinfo_block));
    const auto write_backup_boot = h747_storage_write_raw_blocks(layout.partition_lba + 6U, fat32_boot_block, sizeof(fat32_boot_block));
    const auto write_backup_fsinfo = h747_storage_write_raw_blocks(layout.partition_lba + 7U, fat32_fsinfo_block, sizeof(fat32_fsinfo_block));

    emit<"init_fat32: zero fat0 start={} sectors={}\n">(fat0_lba, layout.sectors_per_fat);
    const auto zero_fat0 = zero_range(fat0_lba, layout.sectors_per_fat) ? 1U : 0U;
    emit<"init_fat32: zero fat1 start={} sectors={} ok={}\n">(fat1_lba, layout.sectors_per_fat, zero_fat0);
    const auto zero_fat1 = (zero_fat0 != 0U && zero_range(fat1_lba, layout.sectors_per_fat)) ? 1U : 0U;
    const auto zero_root = (zero_fat1 != 0U && zero_range(root_lba, layout.sectors_per_cluster)) ? 1U : 0U;

    const auto write_fat0 = h747_storage_write_raw_blocks(fat0_lba, fat32_fat_block, sizeof(fat32_fat_block));
    const auto write_fat1 = h747_storage_write_raw_blocks(fat1_lba, fat32_fat_block, sizeof(fat32_fat_block));
    const auto read_boot = h747_storage_read_raw_blocks(layout.partition_lba, lba_dump, sizeof(lba_dump));
    const auto verify = (read_boot != 0U
                         && lba_dump[510] == 0x55U
                         && lba_dump[511] == 0xAAU
                         && u16le(lba_dump + 11) == 512U
                         && lba_dump[13] == layout.sectors_per_cluster
                         && u16le(lba_dump + 14) == layout.reserved_sectors
                         && u32le(lba_dump + 36) == layout.sectors_per_fat)
                            ? 1U
                            : 0U;
    const auto storage = h747_storage_state();
    const auto ok = (write_mbr != 0U
                     && write_boot != 0U
                     && write_fsinfo != 0U
                     && write_backup_boot != 0U
                     && write_backup_fsinfo != 0U
                     && zero_fat0 != 0U
                     && zero_fat1 != 0U
                     && zero_root != 0U
                     && write_fat0 != 0U
                     && write_fat1 != 0U
                     && verify != 0U)
                        ? 1U
                        : 0U;
    emit<"init_fat32: ok={} mbr={} boot={} fsinfo={} backup={}/{} zero={}/{}/{} fat={}/{} verify={} part_lba={} part_blocks={} reserved={} fatsz={} data_lba={} clusters={} spc={} hal={} err=0x{:08X} last={}/{}\n">(
        ok,
        write_mbr,
        write_boot,
        write_fsinfo,
        write_backup_boot,
        write_backup_fsinfo,
        zero_fat0,
        zero_fat1,
        zero_root,
        write_fat0,
        write_fat1,
        verify,
        layout.partition_lba,
        layout.partition_blocks,
        layout.reserved_sectors,
        layout.sectors_per_fat,
        layout.data_start_lba,
        layout.cluster_count,
        static_cast<unsigned>(layout.sectors_per_cluster),
        storage.last_hal_status,
        storage.last_error,
        storage.last_lba,
        storage.last_count);
}

void handle_command(const std::string_view line) {
    if (line.empty()) {
        return;
    }
    if (line == "help"sv) {
        print_help();
    } else if (line == "status"sv) {
        print_status();
    } else if (line == "storage status"sv) {
        print_storage_status();
    } else if (line == "usb status"sv) {
        print_usb_link_summary();
    } else if (line == "write smoke"sv) {
        run_write_smoke();
    } else if (line == "dump lba0"sv) {
        dump_lba0();
    } else if (line == "dump part"sv) {
        dump_part();
    } else if (line == "wipe lba0"sv) {
        wipe_lba0();
    } else if (line == "init mbr"sv) {
        init_mbr();
    } else if (line == "init fat32"sv) {
        init_fat32();
    } else if (line == "usb detach"sv) {
        h747::usb_msc_legacy::detach();
        emit<"usb_detach: ok=1\n">();
        print_usb_link_summary();
    } else if (line == "usb attach"sv) {
        const auto status = h747::usb_msc_legacy::attach();
        emit<"usb_attach: status={}\n">(status);
        print_usb_link_summary();
    } else if (line == "usb attach window"sv) {
        run_usb_attach_window();
    } else if (line == "usb readonly"sv) {
        h747::usb_msc_legacy::set_write_enabled(false);
        emit<"usb_readonly: ok=1\n">();
        print_usb_link_summary();
    } else if (line == "usb writable"sv) {
        h747::usb_msc_legacy::set_write_enabled(true);
        emit<"usb_writable: ok=1\n">();
        print_usb_link_summary();
    } else if (line == "reboot"sv) {
        emit<"rebooting...\n">();
        HAL_Delay(20U);
        NVIC_SystemReset();
    } else {
        emit<"unknown command\n">();
    }
}

} // namespace

extern "C" void app_usb_setup_sniff(const std::uint8_t setup[8]) {
    // Setup packets are already counted and sampled in usbd_conf.c.
    // Avoid blocking UART writes from the USB interrupt path.
    (void)setup;
}

void init() {
    emit<"legacy_msc_probe: init role=st_usb_stack_storage_only usb=manual_attach\n">();
    print_help();
    print_status();
    print_prompt();
    last_tick_ms = h747::port::tick_ms();
    const auto usb = h747::usb_msc_legacy::state();
    last_alive_setup_count = usb.setup_count;
    last_alive_reset_count = usb.reset_count;
    last_alive_read_count = usb.read_calls;
    last_alive_write_count = usb.write_calls;
}

void loop_once() noexcept {
    h747::usb_msc_legacy::poll();

    if (const auto line = line_source.poll_line()) {
        handle_command(*line);
        print_prompt();
    }

    const std::uint32_t now = h747::port::tick_ms();
    if ((now - last_tick_ms) >= 5000U) {
        last_tick_ms = now;
        ++alive_count;
        const auto storage = h747_storage_state();
        const auto usb = h747::usb_msc_legacy::state();
        const auto setup_delta = usb.setup_count - last_alive_setup_count;
        const auto reset_delta = usb.reset_count - last_alive_reset_count;
        const auto read_delta = usb.read_calls - last_alive_read_count;
        const auto write_delta = usb.write_calls - last_alive_write_count;
        last_alive_setup_count = usb.setup_count;
        last_alive_reset_count = usb.reset_count;
        last_alive_read_count = usb.read_calls;
        last_alive_write_count = usb.write_calls;
        emit<"legacy_msc_probe: alive tick={} usb_reads={} usb_writes={} rfails={} wfails={} setup={} reset={} setup_delta={} reset_delta={} read_delta={} write_delta={}\n">(
            alive_count,
            usb.read_calls,
            usb.write_calls,
            storage.read_fail_count,
            storage.write_fail_count,
            usb.setup_count,
            usb.reset_count,
            setup_delta,
            reset_delta,
            read_delta,
            write_delta);
    }
}

} // namespace h747::apps::usb_msc_legacy_probe
