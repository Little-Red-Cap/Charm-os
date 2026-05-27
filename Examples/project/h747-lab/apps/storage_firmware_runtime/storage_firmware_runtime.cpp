#include "storage_firmware_runtime.h"

#include "console_service.hpp"
#include "port.h"
#include "storage.h"
#include "stm32h7xx_hal.h"
#include "usb_device.h"
#include "usb_msc_service.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>
#include <string_view>
#include <utility>

import out.core;
import out.format;

namespace h747::apps::storage_firmware_runtime {
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

void emit_text(const char* text) noexcept {
    if (text == nullptr) {
        return;
    }
    (void)console_stream().write(std::string_view{text});
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

template <out::fixed_string Fmt, class... Args>
void emit(Args&&... args) noexcept {
    out::discard(out::vprint<Fmt>(out_sink(), std::forward<Args>(args)...));
}

h747::console::ConsoleLineSource line_source;
std::uint32_t last_tick_ms = 0U;
std::uint32_t alive_count = 0U;

void print_prompt() {
    emit<"\r\nh747-storage> ">();
}

void print_help() {
    emit<"Commands:\n">();
    emit<"  help            - Show help\n">();
    emit<"  status          - Print eMMC and USB MSC evidence\n">();
    emit<"  storage status  - Print raw storage evidence\n">();
    emit<"  reboot          - Reboot\n">();
}

void print_status() {
    const auto storage = h747_storage_state();
    const auto usb = h747::usb_msc::state();
    emit<"storage_fw: profile=storage_firmware_runtime tick={} alive={}\n">(
        h747::port::tick_ms(),
        alive_count);
    emit<"storage: ready={}/{} fat={} reads={} rfails={} writes={} wfails={} blk={} count={} part_lba={} hal={} err=0x{:08X} card={} wait_to={}\n">(
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
        storage.wait_timeout_count);
    emit<"usb_msc: init={} start={} ready={} writable={} blk={} count={} part_lba={} init_calls={} ready_calls={} cap_calls={} read_calls={} write_calls={} last_err={}\n">(
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
    emit<"usb_init: pcd_ready={} pcd_st={} dev_init={} usbd={}/{} class={}/{} storage={}/{} start={}/{} vbus={}\n">(
        usb.pcd_ready,
        usb.pcd_init_status,
        usb.device_init_called,
        usb.usbd_init_ok,
        usb.usbd_init_status,
        usb.class_ok,
        usb.register_class_status,
        usb.storage_ok,
        usb.register_storage_status,
        usb.usbd_start_ok,
        usb.usbd_start_status,
        usb.vbus_detector_enabled);
    emit<"usb_events: setup={} reset={} connect={} disconnect={} suspend={} resume={} ep0_out={} ep0_in={} last_setup={}\n">(
        usb.setup_count,
        usb.reset_count,
        usb.connect_count,
        usb.disconnect_count,
        usb.suspend_count,
        usb.resume_count,
        usb.out_ep0_hits,
        usb.in_ep0_hits,
        usb.last_setup_valid);
    emit<"usb_regs: gusbcfg=0x{:08X} gahbcfg=0x{:08X} gintsts=0x{:08X} gintmsk=0x{:08X} dctl=0x{:08X} dsts=0x{:08X} gotgctl=0x{:08X} gccfg=0x{:08X}\n">(
        usb.gusbcfg,
        usb.gahbcfg,
        usb.gintsts,
        usb.gintmsk,
        usb.dctl,
        usb.dsts,
        usb.gotgctl,
        usb.gccfg);
    emit<"usb_ep0: diepctl=0x{:08X} diepint=0x{:08X} doepctl=0x{:08X} doepint=0x{:08X}\n">(
        usb.diepctl0,
        usb.diepint0,
        usb.doepctl0,
        usb.doepint0);
    if (usb.last_setup_valid != 0U) {
        emit_hex_bytes("usb_setup:", usb.last_setup, sizeof(usb.last_setup));
    }
    emit<"usb_desc: dev_len={} cfg_len={} dev_prefix={} cfg_prefix={}\n">(
        usb.dev_desc_len,
        usb.cfg_desc_len,
        usb.dev_desc_prefix_len,
        usb.cfg_desc_prefix_len);
    if (usb.dev_desc_prefix_len != 0U) {
        emit_hex_bytes("usb_dev_desc:", usb.dev_desc_prefix, usb.dev_desc_prefix_len);
    }
    if (usb.cfg_desc_prefix_len != 0U) {
        emit_hex_bytes("usb_cfg_desc:", usb.cfg_desc_prefix, usb.cfg_desc_prefix_len);
    }
}

void handle_command(const std::string_view line) {
    if (line.empty()) {
        return;
    }
    if (line == "help"sv) {
        print_help();
    } else if (line == "status"sv || line == "storage status"sv) {
        print_status();
    } else if (line == "reboot"sv) {
        emit<"rebooting...\n">();
        HAL_Delay(20U);
        NVIC_SystemReset();
    } else {
        emit<"unknown command\n">();
    }
}

} // namespace

extern "C" void app_usb_setup_sniff(const uint8_t setup[8]) {
    (void)setup;
}

void init() {
    emit<"storage_fw: init role=emmc_usb_msc\n">();
    print_help();
    print_status();
    print_prompt();
    last_tick_ms = h747::port::tick_ms();
}

void loop_once() noexcept {
    h747::usb_msc::poll();

    if (const auto line = line_source.poll_line()) {
        handle_command(*line);
        print_prompt();
    }

    const std::uint32_t now = h747::port::tick_ms();
    if ((now - last_tick_ms) >= 1000U) {
        last_tick_ms = now;
        ++alive_count;
        emit<"storage_fw: alive tick={} usb_reads={} usb_writes={}\n">(
            alive_count,
            h747::usb_msc::state().read_calls,
            h747::usb_msc::state().write_calls);
        print_status();
        print_prompt();
    }
}

} // namespace h747::apps::storage_firmware_runtime
