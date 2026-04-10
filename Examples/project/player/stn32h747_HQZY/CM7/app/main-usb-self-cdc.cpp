#include <array>
#include <cstdint>
#include <span>
#include <cstring>
#include <cstdio>

#include "stm32h7xx_hal.h"

import charm.port;
import charm.system.clock;
import charm.system.time;
import out.api;
import player.stm32h7.board_usb;
import usb.class_cdc;
import usb.common;
import usb.device_driver;
import usb.model;
import usb.plan;
import usb.runtime;
import usb.spec;

extern "C" {
void Error_Handler(void);
extern PCD_HandleTypeDef hpcd_USB_OTG_FS;
}

namespace {
constexpr usb::u16 kLangs[] = { 0x0409 };
constexpr auto kLangDesc = usb::make_lang_id_descriptor(kLangs);
constexpr auto kVendorStr = usb::make_ascii_string_descriptor("Charm");
constexpr auto kProductStr = usb::make_ascii_string_descriptor("Charm Self CDC");
constexpr auto kSerialStr = usb::make_ascii_string_descriptor("0001");

static const usb::StringTable<4> kUsbStrings{
    std::array<std::span<const usb::u8>, 4>{
        std::span<const usb::u8>(kLangDesc.data(), kLangDesc.size()),
        std::span<const usb::u8>(kVendorStr.data(), kVendorStr.size()),
        std::span<const usb::u8>(kProductStr.data(), kProductStr.size()),
        std::span<const usb::u8>(kSerialStr.data(), kSerialStr.size()),
    }
};

struct CdcAppState {
    std::array<usb::u8, 512> tx_buf{};
    std::array<usb::u8, 512> rx_buf{};
    std::size_t tx_len{0};
    std::size_t rx_len{0};
    bool in_busy{false};
    bool banner_queued{false};
    bool dtr{false};
    usb::class_driver::CdcLineCoding line_coding{};
};

bool cdc_queue(CdcAppState& state, std::span<const usb::u8> data) noexcept {
    if (data.empty()) return true;
    const auto room = state.tx_buf.size() - state.tx_len;
    if (data.size() > room) return false;
    std::memcpy(state.tx_buf.data() + state.tx_len, data.data(), data.size());
    state.tx_len += data.size();
    return true;
}

bool cdc_queue_text(CdcAppState& state, const char* text) noexcept {
    return cdc_queue(state, std::span<const usb::u8>(reinterpret_cast<const usb::u8*>(text), std::strlen(text)));
}

void print_desc_hex(const char* prefix, std::span<const usb::u8> bytes) {
    if (!prefix || bytes.empty()) return;
    char buf[512];
    int offset = std::snprintf(buf, sizeof(buf), "%s size=%lu", prefix,
        static_cast<unsigned long>(bytes.size()));
    for (std::size_t i = 0; i < bytes.size() && offset > 0 && static_cast<std::size_t>(offset) < (sizeof(buf) - 4); ++i) {
        offset += std::snprintf(buf + offset, sizeof(buf) - static_cast<std::size_t>(offset),
            " %02X", static_cast<unsigned>(bytes[i]));
    }
    if (offset > 0 && static_cast<std::size_t>(offset) < sizeof(buf) - 2) {
        buf[offset++] = '\n';
        buf[offset] = '\0';
        out::print<"{}">(buf);
    }
}

void cdc_on_line_coding(void* ctx, const usb::class_driver::CdcLineCoding& coding) noexcept {
    auto* state = static_cast<CdcAppState*>(ctx);
    if (!state) return;
    state->line_coding = coding;
}

void cdc_on_control_line(void* ctx, usb::u16 value) noexcept {
    auto* state = static_cast<CdcAppState*>(ctx);
    if (!state) return;
    state->dtr = (value & 0x0001u) != 0u;
    if (state->dtr && !state->banner_queued) {
        state->banner_queued = cdc_queue_text(*state, "Charm Self CDC ready\r\n");
    }
}

std::span<usb::u8> cdc_tx_buffer(void* ctx) noexcept {
    auto* state = static_cast<CdcAppState*>(ctx);
    return state ? std::span<usb::u8>(state->tx_buf.data(), state->tx_buf.size()) : std::span<usb::u8>{};
}

std::span<usb::u8> cdc_rx_buffer(void* ctx) noexcept {
    auto* state = static_cast<CdcAppState*>(ctx);
    return state ? std::span<usb::u8>(state->rx_buf.data(), state->rx_buf.size()) : std::span<usb::u8>{};
}

std::size_t cdc_tx_length(void* ctx) noexcept {
    auto* state = static_cast<CdcAppState*>(ctx);
    return state ? state->tx_len : 0u;
}

void cdc_on_rx_done(void* ctx, std::size_t len) noexcept {
    auto* state = static_cast<CdcAppState*>(ctx);
    if (!state) return;
    state->rx_len = len;
    if (len > 0) {
        (void)cdc_queue(*state, std::span<const usb::u8>(state->rx_buf.data(), len));
    }
}

void cdc_on_tx_done(void* ctx, std::size_t len) noexcept {
    auto* state = static_cast<CdcAppState*>(ctx);
    if (!state) return;
    state->in_busy = false;
    if (len >= state->tx_len) {
        state->tx_len = 0;
        return;
    }
    const auto remain = state->tx_len - len;
    for (std::size_t index = 0; index < remain; ++index) {
        state->tx_buf[index] = state->tx_buf[index + len];
    }
    state->tx_len = remain;
}

void cdc_on_ready(void*, usb::class_driver::CdcAcm* cdc, const usb::class_driver::CdcConfig* cfg) noexcept {
    (void)cdc;
    (void)cfg;
}
} // namespace

int charm_player_selected_profile_main() {
    auto kit = charm::port::init();
    charm::system::Clock clock{nullptr, charm::system::ClockOps{&charm::port::now_ms, nullptr}};
    charm::system::time::bind(clock);
    out::Scope scope{kit.console};

    out::println<"boot: uart ok">();

    player::stm32h7::board::usb_hw_init();
    player::stm32h7::board::usb_enable_hooks(true);
    reinterpret_cast<USB_OTG_DeviceTypeDef*>(
        USB_OTG_FS_PERIPH_BASE + USB_OTG_DEVICE_BASE)->DCTL |= USB_OTG_DCTL_SDIS;

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

    CdcAppState cdc_state{};
    auto& dcd_ops = player::stm32h7::board::usb_dcd_ops();
    const auto runtime = usb::runtime::stm32_fs(
        dcd_ops,
        &hpcd_USB_OTG_FS,
        &player::stm32h7::board::usb_adapter(),
        {},
        usb::runtime::CdcRuntimeConfig{
            .ctx = &cdc_state,
            .ops = usb::class_driver::CdcOps{
                .on_line_coding = &cdc_on_line_coding,
                .on_control_line = &cdc_on_control_line,
                .tx_buffer = &cdc_tx_buffer,
                .rx_buffer = &cdc_rx_buffer,
                .tx_length = &cdc_tx_length,
                .on_rx_done = &cdc_on_rx_done,
                .on_tx_done = &cdc_on_tx_done,
            },
            .ready = usb::runtime::CdcReadyHook{&cdc_on_ready, nullptr},
        });

    const auto spec = usb::spec::cdc_device(
        usb::spec::DeviceSpec{
            .vendor_id = 0x1209,
            .product_id = 0x0003,
            .i_manufacturer = 1,
            .i_product = 2,
            .i_serial = 3,
            .strings = std::span<const std::span<const usb::u8>>(
                kUsbStrings.entries.data(), kUsbStrings.entries.size()),
        },
        usb::spec::CdcFunctionSpec{
            .cap_name = "usb.cdc0",
            .ctrl_ifc = 0,
            .data_ifc = 1,
            .ep_notify = 0x82,
            .ep_out = 0x01,
            .ep_in = 0x81,
            .ep_mps = 64,
        });

    const auto model = usb::build(spec);
    const auto plan = usb::plan::build(model);
    if (!plan) {
        out::println<"boot: usb cdc plan failed {}">(static_cast<int>(plan.error()));
        Error_Handler();
    }

    auto binding = usb::runtime::make(plan.value(), runtime);
    auto init_st = decltype(binding)::init_trampoline(&binding);
    if (!init_st) {
        out::println<"boot: usb cdc init failed {}">(static_cast<int>(init_st.error()));
        Error_Handler();
    }

    print_desc_hex("usb: dev_desc", binding.table.device);
    print_desc_hex("usb: cfg_desc", binding.table.configuration);

    if (HAL_PCD_Start(&hpcd_USB_OTG_FS) != HAL_OK) {
        out::println<"boot: usb start failed">();
        Error_Handler();
    }
    reinterpret_cast<USB_OTG_DeviceTypeDef*>(
        USB_OTG_FS_PERIPH_BASE + USB_OTG_DEVICE_BASE)->DCTL &= ~USB_OTG_DCTL_SDIS;
    out::println<"boot: usb start ok">();
    out::println<"boot: usb self cdc ok">();

    std::uint32_t last_ms = 0;
    while (true) {
        if (binding.cdc && cdc_state.tx_len > 0 && !cdc_state.in_busy) {
            const auto sent = usb::device::examples::send_cdc_in_packet(
                dcd_ops,
                &hpcd_USB_OTG_FS,
                *binding.cdc,
                binding.cdc->config().ep_mps);
            if (sent) {
                cdc_state.in_busy = true;
            }
        }

        const auto now = HAL_GetTick();
        if ((now - last_ms) >= 1000u) {
            const auto diag = player::stm32h7::board::usb_diag_snapshot();
            out::println<"usb: setup={} out0={} in0={} out1={} in1={} reset={} conn={} set_cfg={} last_cfg={} set_addr={} last_addr={} class_setup={} bm=0x{:02X} b=0x{:02X} wv=0x{:04X} wl=0x{:04X}">(
                diag.setup_calls,
                diag.out0_calls,
                diag.in0_calls,
                diag.out1_calls,
                diag.in1_calls,
                diag.reset_calls,
                diag.connect_calls,
                diag.set_cfg_calls,
                diag.set_cfg_last,
                diag.set_addr_calls,
                diag.set_addr_last,
                diag.class_setup_calls,
                diag.bm_request_type,
                diag.b_request,
                diag.w_value,
                diag.w_length);
            out::println<"cdc: tx_len={} rx_len={} in_busy={} dtr={} baud={} data_bits={} parity={} stop_bits={}">(
                static_cast<unsigned long>(cdc_state.tx_len),
                static_cast<unsigned long>(cdc_state.rx_len),
                cdc_state.in_busy ? 1u : 0u,
                cdc_state.dtr ? 1u : 0u,
                static_cast<unsigned long>(cdc_state.line_coding.baud),
                cdc_state.line_coding.data_bits,
                cdc_state.line_coding.parity,
                cdc_state.line_coding.stop_bits);
            last_ms = now;
        }

        charm::system::time::sleep_ms(1);
    }
}
