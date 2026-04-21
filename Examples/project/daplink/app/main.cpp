#include "main.h"

#include <array>
#include <cstdint>

import daplink.board;
import daplink.usb_minimal;
import daplink.cmsis_dap;
import daplink.app_config;
import daplink.dap_init;
import daplink.dap_ops;
import daplink.ring_buffer;
import daplink.dap_transport;
import daplink.dap_policy;
import daplink.dap_strategy;
import io.channel;
import util.core;

namespace {
    constexpr auto kUsbProfile = daplink::app_config::kConfig.usb.profile;
    constexpr bool kEnableCdc =
        (kUsbProfile == daplink::app_config::UsbProfile::cdc) ||
        (kUsbProfile == daplink::app_config::UsbProfile::composite);
    constexpr bool kEnableHid =
        (kUsbProfile == daplink::app_config::UsbProfile::hid) ||
        (kUsbProfile == daplink::app_config::UsbProfile::composite);

    static_assert(!kEnableHid || (daplink::usb_minimal::hid_packet_size == daplink::cmsis_dap::kPacketSize));
}

extern "C" void SystemClock_Config(void);
extern "C" void MPU_Config(void);

namespace {
    constexpr std::size_t kUartBufSize = 256;
    using UartRing = daplink::ring_buffer::Buffer<kUartBufSize>;
    constexpr std::size_t kIoChunk = 64;
    constexpr std::size_t kUartTxBurstLimit = 8;

    io::result uart_read(void*, io::MutByteView buf) noexcept {
        std::size_t count = 0;
        while (count < buf.size() && daplink::board::cdc_uart_rx_ready()) {
            buf[count] = static_cast<util::u8>(daplink::board::cdc_uart_read());
            ++count;
        }
        if (count == 0) {
            return io::fail(io::errc::would_block);
        }
        return io::ok(count);
    }

    io::result uart_write(void*, io::ByteView buf) noexcept {
        const std::size_t limit = (buf.size() < kUartTxBurstLimit) ? buf.size() : kUartTxBurstLimit;
        if (limit == 0) {
            return io::fail(io::errc::would_block);
        }
        std::size_t count = 0;
        while (count < limit && daplink::board::cdc_uart_tx_ready()) {
            daplink::board::cdc_uart_write(static_cast<std::uint8_t>(buf[count]));
            ++count;
            if (daplink::board::cdc_uart_rx_pending()) {
                break;
            }
        }
        if (count == 0) {
            return io::fail(io::errc::would_block);
        }
        return io::ok(count);
    }

    io::result usb_cdc_read(void*, io::MutByteView buf) noexcept {
        if (!daplink::usb_minimal::cdc_out_ready()) {
            return io::fail(io::errc::would_block);
        }
        const auto payload = daplink::usb_minimal::cdc_out_packet();
        if (buf.size() < payload.size()) {
            return io::fail(io::errc::would_block);
        }
        const std::size_t len = payload.size();
        for (std::size_t i = 0; i < len; ++i) {
            buf[i] = static_cast<util::u8>(payload[i]);
        }
        daplink::usb_minimal::cdc_consume_out();
        return io::ok(len);
    }

    io::result usb_cdc_write(void*, io::ByteView buf) noexcept {
        if (!daplink::usb_minimal::cdc_send_in(
                reinterpret_cast<const std::uint8_t*>(buf.data()),
                static_cast<std::uint16_t>(buf.size()))) {
            return io::fail(io::errc::would_block);
        }
        return io::ok(buf.size());
    }

} // namespace

int main()
{
    HAL_Init();
    SystemClock_Config();

    if (!daplink::board::init_peripherals()) {
        Error_Handler();
    }
    daplink::board::configure_debug_pins_hi_z();

    auto dap_state = daplink::dap_init::make_dap_state();
    const daplink::cmsis_dap::DeviceInfo kInfo{
        daplink::cmsis_dap::make_info_field(daplink::app_config::kUsbManufacturer),
        daplink::cmsis_dap::make_info_field(daplink::app_config::kUsbProduct),
        daplink::cmsis_dap::make_info_field(daplink::app_config::kUsbSerial),
        daplink::cmsis_dap::make_info_field(daplink::app_config::kFwVersion)
    };

    UartRing uart_tx{};
    UartRing uart_rx{};
    daplink::dap_policy::UsbScheduler scheduler{};
    scheduler.cdc_policy = static_cast<daplink::dap_policy::CdcPolicy>(
        daplink::app_config::kConfig.cdc.policy);
    using DapOps = daplink::cmsis_dap::DefaultOps<daplink::board::SwdBackend>;
    using DapPolicy = daplink::dap_strategy::DefaultTransferPolicy<daplink::cmsis_dap::State>;
    daplink::dap_transport::HidTransport<
        daplink::board::SwdBackend,
        DapOps,
        DapPolicy> dap_transport{dap_state, kInfo};
    io::Channel usb_cdc{
        nullptr,
        io::ChannelOps{usb_cdc_read, usb_cdc_write, nullptr}
    };
    io::Channel uart{
        nullptr,
        io::ChannelOps{uart_read, uart_write, nullptr}
    };
    auto last_line = daplink::dap_policy::UsbScheduler::to_line(daplink::usb_minimal::cdc_line());
    if constexpr (kEnableCdc) {
        daplink::board::cdc_uart_apply_line(
            last_line.baud, last_line.stop_bits, last_line.parity, last_line.data_bits);
    }

    while (true) {
        scheduler.template tick<kIoChunk>(
            dap_transport,
            [&]() noexcept {
                dap_state = {};
                dap_transport.reset();
                if constexpr (kEnableCdc) {
                    uart_tx = {};
                    uart_rx = {};
                }
            },
            usb_cdc,
            uart,
            uart_tx,
            uart_rx,
            last_line,
            kEnableHid,
            kEnableCdc);
    }
}
