module;

#include <cstdint>
#ifndef CHARM_DAP_ENABLE_SWO
#define CHARM_DAP_ENABLE_SWO 0
#endif
#ifndef CHARM_DAP_ENABLE_SWO_STREAM
#define CHARM_DAP_ENABLE_SWO_STREAM 0
#endif
#ifndef CHARM_DAP_ENABLE_DAP_UART
#define CHARM_DAP_ENABLE_DAP_UART 0
#endif

export module daplink.cmsis_dap:protocol;

export import :core;

namespace daplink::cmsis_dap::detail {
    constexpr std::uint8_t kCmsisDapInfo = 0x00;
    constexpr std::uint8_t kCmsisDapHostStatus = 0x01;
    constexpr std::uint8_t kCmsisDapConnect = 0x02;
    constexpr std::uint8_t kCmsisDapDisconnect = 0x03;
    constexpr std::uint8_t kCmsisDapTransferConfigure = 0x04;
    constexpr std::uint8_t kCmsisDapTransfer = 0x05;
    constexpr std::uint8_t kCmsisDapTransferBlock = 0x06;
    constexpr std::uint8_t kCmsisDapWriteAbort = 0x08;
    constexpr std::uint8_t kCmsisDapDelay = 0x09;
    constexpr std::uint8_t kCmsisDapResetTarget = 0x0A;
    constexpr std::uint8_t kCmsisDapSwjPins = 0x10;
    constexpr std::uint8_t kCmsisDapSwjClock = 0x11;
    constexpr std::uint8_t kCmsisDapSwjSequence = 0x12;
    constexpr std::uint8_t kCmsisDapSwdConfigure = 0x13;
    constexpr std::uint8_t kCmsisDapSwoTransport = 0x17;
    constexpr std::uint8_t kCmsisDapSwoMode = 0x18;
    constexpr std::uint8_t kCmsisDapSwoBaudrate = 0x19;
    constexpr std::uint8_t kCmsisDapSwoControl = 0x1A;
    constexpr std::uint8_t kCmsisDapSwoStatus = 0x1B;
    constexpr std::uint8_t kCmsisDapSwoData = 0x1C;
    constexpr std::uint8_t kCmsisDapSwoExtendedStatus = 0x1E;
    constexpr std::uint8_t kCmsisDapUartTransport = 0x1F;
    constexpr std::uint8_t kCmsisDapUartConfigure = 0x20;
    constexpr std::uint8_t kCmsisDapUartTransfer = 0x21;
    constexpr std::uint8_t kCmsisDapUartControl = 0x22;
    constexpr std::uint8_t kCmsisDapUartStatus = 0x23;
    constexpr std::uint8_t kCmsisDapQueueCommands = 0x7E;
    constexpr std::uint8_t kCmsisDapExecuteCommands = 0x7F;
    constexpr std::uint8_t kCmsisDapInvalid = 0xFF;

    constexpr std::uint8_t kDapInfoVendor = 1;
    constexpr std::uint8_t kDapInfoProduct = 2;
    constexpr std::uint8_t kDapInfoSerial = 3;
    constexpr std::uint8_t kDapInfoFwVersion = 4;
    constexpr std::uint8_t kDapInfoCapabilities = 0xF0;
    constexpr std::uint8_t kDapInfoUartRxBufferSize = 0xFB;
    constexpr std::uint8_t kDapInfoUartTxBufferSize = 0xFC;
    constexpr std::uint8_t kDapInfoSwoBufferSize = 0xFD;
    constexpr std::uint8_t kDapInfoPacketCount = 0xFE;
    constexpr std::uint8_t kDapInfoPacketSize = 0xFF;

    constexpr std::uint8_t kCapSwd = 1U << 0;
    constexpr std::uint8_t kCapJtag = 1U << 1;
    constexpr std::uint8_t kCapSwoUart = 1U << 2;
    constexpr std::uint8_t kCapSwoManchester = 1U << 3;
    constexpr std::uint8_t kCapAtomic = 1U << 4;
    constexpr std::uint8_t kCapTimestamp = 1U << 5;
    constexpr std::uint8_t kCapSwoStreaming = 1U << 6;
    constexpr std::uint8_t kCapDapUart = 1U << 7;

    constexpr std::uint8_t kCapabilities =
        kCapSwd |
        kCapAtomic |
        (CHARM_DAP_ENABLE_SWO ? kCapSwoUart : 0U) |
        (CHARM_DAP_ENABLE_SWO_STREAM ? kCapSwoStreaming : 0U) |
        (CHARM_DAP_ENABLE_DAP_UART ? kCapDapUart : 0U);

    constexpr std::uint8_t kDapOk = 0x00;
    constexpr std::uint8_t kDapError = 0xFF;
    constexpr std::uint8_t kDapPortDisabled = 0x00;
    constexpr std::uint8_t kDapPortSwd = 0x01;
    constexpr std::uint8_t kDapTransferOk = 0x01;
    constexpr std::uint8_t kDapTransferError = 0x08;
    constexpr std::uint8_t kDapTransferMismatch = 0x10;
    constexpr std::uint8_t kReqApndp = 1U << 0;
    constexpr std::uint8_t kReqRnw = 1U << 1;
    constexpr std::uint8_t kReqMatchValue = 1U << 4;
    constexpr std::uint8_t kReqMatchMask = 1U << 5;
    constexpr std::uint8_t kReqDpRdbuff = kReqRnw | (1U << 2) | (1U << 3);

    constexpr std::uint8_t kSwoModeOff = 0;
    constexpr std::uint8_t kSwoModeUart = 1;
    constexpr std::uint8_t kSwoModeManchester = 2;
    constexpr std::uint8_t kSwoCaptureActive = 1U << 0;
    constexpr std::uint16_t kSwoBufferSize = 256;
    constexpr std::uint16_t kUartBufferSize = 256;
    constexpr std::uint8_t kTransferErrorResetThreshold = 8;

    inline std::uint32_t read_le32(const std::uint8_t* p) noexcept {
        return static_cast<std::uint32_t>(p[0]) |
               (static_cast<std::uint32_t>(p[1]) << 8) |
               (static_cast<std::uint32_t>(p[2]) << 16) |
               (static_cast<std::uint32_t>(p[3]) << 24);
    }

    inline void write_le32(std::uint8_t* p, const std::uint32_t v) noexcept {
        p[0] = static_cast<std::uint8_t>(v & 0xFFU);
        p[1] = static_cast<std::uint8_t>((v >> 8) & 0xFFU);
        p[2] = static_cast<std::uint8_t>((v >> 16) & 0xFFU);
        p[3] = static_cast<std::uint8_t>((v >> 24) & 0xFFU);
    }

    inline std::uint8_t fill_info(const InfoField& field, std::uint8_t* out) noexcept {
        for (std::uint8_t i = 0; i < field.size; ++i) {
            out[i] = static_cast<std::uint8_t>(field.data[i]);
        }
        return field.size;
    }
}
