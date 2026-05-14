module;

#include "daplink_legacy_macro_compat.hpp"

#include <cstddef>
#include <cstdint>

export module daplink.app_config;

export namespace daplink::app_config {
    enum class UsbProfile : std::uint8_t {
        hid = 0,
        cdc = 1,
        composite = 2,
    };

    inline constexpr char kUsbManufacturer[] = "DAPLink";
    inline constexpr char kUsbProduct[] = "DAPLink CMSIS-DAP";
    inline constexpr char kUsbSerial[] = "0001";
    inline constexpr char kUsbHidInterface[] = "CMSIS-DAP v1";
    inline constexpr char kUsbCdcFunction[] = "DAPLink CDC Bridge";
    inline constexpr char kUsbCdcControlInterface[] = "DAPLink CDC Control";
    inline constexpr char kUsbCdcDataInterface[] = "DAPLink CDC Data";
    inline constexpr char kCmsisDapProtocolVersion[] = "1.3.0";
    inline constexpr char kProductFwVersion[] = "0.1.0";

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

    constexpr std::uint32_t kSwdDefaultHz = 1000000U;
    constexpr std::uint32_t kSwdMinHz = 200000U;
    constexpr std::uint8_t kSwdTurnaround = 1;
    constexpr std::uint8_t kSwdIdleCycles = 0;
    constexpr std::uint16_t kSwdRetryCount = 100;
    constexpr std::uint8_t kDapPacketCount = 1;
    constexpr std::uint8_t kDapBurstLimit = 1;

    constexpr std::uint32_t kCdcInTimeoutMs = 250;
    constexpr std::uint8_t kCdcPolicy = 0;

#if defined(DAPLINK_USB_PROFILE_VALUE)
    constexpr std::uint8_t kUsbProfileValue = static_cast<std::uint8_t>(DAPLINK_USB_PROFILE_VALUE);
#else
    constexpr std::uint8_t kUsbProfileValue = static_cast<std::uint8_t>(UsbProfile::composite);
#endif

#if defined(DAPLINK_CDC_UART_INDEX)
    constexpr std::uint8_t kCdcUartIndex = static_cast<std::uint8_t>(DAPLINK_CDC_UART_INDEX);
#else
    constexpr std::uint8_t kCdcUartIndex = 2;
#endif

#if defined(DAPLINK_DAP_BURST_LIMIT)
    constexpr std::uint8_t kDapBurstLimitValue = static_cast<std::uint8_t>(DAPLINK_DAP_BURST_LIMIT);
#else
    constexpr std::uint8_t kDapBurstLimitValue = kDapBurstLimit;
#endif

#if defined(DAPLINK_DAP_PACKET_COUNT)
    constexpr std::uint8_t kDapPacketCountValue = static_cast<std::uint8_t>(DAPLINK_DAP_PACKET_COUNT);
#else
    constexpr std::uint8_t kDapPacketCountValue = kDapPacketCount;
#endif

#if defined(DAPLINK_CDC_IN_TIMEOUT_MS)
    constexpr std::uint32_t kCdcInTimeoutMsValue =
        static_cast<std::uint32_t>(DAPLINK_CDC_IN_TIMEOUT_MS);
#else
    constexpr std::uint32_t kCdcInTimeoutMsValue = kCdcInTimeoutMs;
#endif

#if defined(DAPLINK_CDC_POLICY)
    constexpr std::uint8_t kCdcPolicyValue = static_cast<std::uint8_t>(DAPLINK_CDC_POLICY);
#else
    constexpr std::uint8_t kCdcPolicyValue = kCdcPolicy;
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
        std::uint32_t min_hz;
        std::uint8_t turnaround;
        std::uint8_t idle_cycles;
        std::uint16_t retry_count;
    };

    struct DapConfig {
        std::uint8_t packet_count;
        std::uint8_t burst_limit;
    };

    struct CdcConfig {
        std::uint8_t uart_index;
        std::uint32_t in_timeout_ms;
        std::uint8_t policy;
    };

    struct AppConfig {
        UsbConfig usb;
        SwdConfig swd;
        DapConfig dap;
        CdcConfig cdc;
    };

    static_assert(kUsbProfileValue <= static_cast<std::uint8_t>(UsbProfile::composite));
    static_assert(kCdcUartIndex == 1 || kCdcUartIndex == 2);
    static_assert(kDapPacketCountValue > 0);
    static_assert(kDapBurstLimitValue > 0);
    static_assert(kCdcPolicyValue <= 1);

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
            kSwdMinHz,
            kSwdTurnaround,
            kSwdIdleCycles,
            kSwdRetryCount,
        },
        DapConfig{kDapPacketCountValue, kDapBurstLimitValue},
        CdcConfig{kCdcUartIndex, kCdcInTimeoutMsValue, kCdcPolicyValue},
    };

}
