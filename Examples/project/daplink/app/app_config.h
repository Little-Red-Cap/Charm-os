#ifndef DAPLINK_APP_CONFIG_H
#define DAPLINK_APP_CONFIG_H

#include <cstddef>
#include <cstdint>

namespace daplink::app_config {
    enum class UsbProfile : std::uint8_t {
        hid = 0,
        cdc = 1,
        composite = 2,
    };

    inline constexpr char kUsbManufacturer[] = "Charm";
    inline constexpr char kUsbProduct[] = "Charm CMSIS-DAP";
    inline constexpr char kUsbSerial[] = "0001";
    inline constexpr char kFwVersion[] = "0.1.0";

    constexpr std::uint16_t kUsbVid = 0xCAFE;
    constexpr std::uint16_t kUsbPid = 0x4001;

    constexpr std::uint8_t kUsbEp0Mps = 64;
    constexpr std::uint8_t kUsbHidEpOut = 0x01;
    constexpr std::uint8_t kUsbHidEpIn = 0x81;
    constexpr std::uint16_t kUsbHidEpMps = 64;
    constexpr std::size_t kUsbHidPacketSize = 64;

    constexpr std::uint8_t kUsbCdcEpCmd = 0x83;
    constexpr std::uint8_t kUsbCdcEpOut = 0x04;
    constexpr std::uint8_t kUsbCdcEpIn = 0x84;
    constexpr std::uint16_t kUsbCdcEpCmdMps = 8;
    constexpr std::uint16_t kUsbCdcEpMps = 64;
    constexpr bool kUsbCdcHasCmdEp = true;

    constexpr std::uint32_t kSwdDefaultHz = 5000000U;
    constexpr std::uint8_t kSwdTurnaround = 1;
    constexpr std::uint8_t kSwdIdleCycles = 0;
    constexpr std::uint16_t kSwdRetryCount = 100;

#ifdef CHARM_DAP_USB_PROFILE
    constexpr std::uint8_t kUsbProfileValue = static_cast<std::uint8_t>(CHARM_DAP_USB_PROFILE);
#else
    constexpr std::uint8_t kUsbProfileValue = static_cast<std::uint8_t>(UsbProfile::composite);
#endif

#ifdef CHARM_DAP_CDC_UART
    constexpr std::uint8_t kCdcUartIndex = static_cast<std::uint8_t>(CHARM_DAP_CDC_UART);
#else
    constexpr std::uint8_t kCdcUartIndex = 2;
#endif

    struct UsbStrings {
        const char* manufacturer;
        const char* product;
        const char* serial;
    };

    struct UsbConfig {
        UsbProfile profile;
        std::uint16_t vid;
        std::uint16_t pid;
        UsbStrings strings;
        std::uint8_t ep0_mps;
        std::uint8_t hid_ep_out;
        std::uint8_t hid_ep_in;
        std::uint16_t hid_ep_mps;
        std::size_t hid_packet_size;
        std::uint8_t cdc_ep_cmd;
        std::uint8_t cdc_ep_out;
        std::uint8_t cdc_ep_in;
        std::uint16_t cdc_ep_cmd_mps;
        std::uint16_t cdc_ep_mps;
        bool cdc_has_cmd_ep;
    };

    struct SwdConfig {
        std::uint32_t default_hz;
        std::uint8_t turnaround;
        std::uint8_t idle_cycles;
        std::uint16_t retry_count;
    };

    struct CdcConfig {
        std::uint8_t uart_index;
    };

    struct AppConfig {
        UsbConfig usb;
        SwdConfig swd;
        CdcConfig cdc;
        const char* fw_version;
    };

    static_assert(kUsbProfileValue <= static_cast<std::uint8_t>(UsbProfile::composite));
    static_assert(kCdcUartIndex == 1 || kCdcUartIndex == 2);

    inline constexpr AppConfig kConfig{
        UsbConfig{
            static_cast<UsbProfile>(kUsbProfileValue),
            kUsbVid,
            kUsbPid,
            UsbStrings{kUsbManufacturer, kUsbProduct, kUsbSerial},
            kUsbEp0Mps,
            kUsbHidEpOut,
            kUsbHidEpIn,
            kUsbHidEpMps,
            kUsbHidPacketSize,
            kUsbCdcEpCmd,
            kUsbCdcEpOut,
            kUsbCdcEpIn,
            kUsbCdcEpCmdMps,
            kUsbCdcEpMps,
            kUsbCdcHasCmdEp,
        },
        SwdConfig{
            kSwdDefaultHz,
            kSwdTurnaround,
            kSwdIdleCycles,
            kSwdRetryCount,
        },
        CdcConfig{kCdcUartIndex},
        kFwVersion,
    };
}

#endif
