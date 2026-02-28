module;

#include "usb.h"

#include <cstddef>
#include <cstdint>

export module daplink.usb_minimal;
import daplink.swd_link;

namespace daplink::usb_minimal::detail {
    constexpr std::uint8_t kEp0Mps = 64;
    constexpr std::uint8_t kHidEpOut = 0x01;
    constexpr std::uint8_t kHidEpIn = 0x81;
    constexpr std::uint16_t kHidEpMps = 64;
    constexpr std::uint8_t kHidPacketSize = 64;

    constexpr std::uint8_t kReqGetStatus = 0x00;
    constexpr std::uint8_t kReqClearFeature = 0x01;
    constexpr std::uint8_t kReqSetFeature = 0x03;
    constexpr std::uint8_t kReqSetAddress = 0x05;
    constexpr std::uint8_t kReqGetDescriptor = 0x06;
    constexpr std::uint8_t kReqGetConfiguration = 0x08;
    constexpr std::uint8_t kReqSetConfiguration = 0x09;
    constexpr std::uint8_t kReqGetInterface = 0x0A;
    constexpr std::uint8_t kReqSetInterface = 0x0B;

    constexpr std::uint8_t kReqGetReport = 0x01;
    constexpr std::uint8_t kReqGetIdle = 0x02;
    constexpr std::uint8_t kReqGetProtocol = 0x03;
    constexpr std::uint8_t kReqSetReport = 0x09;
    constexpr std::uint8_t kReqSetIdle = 0x0A;
    constexpr std::uint8_t kReqSetProtocol = 0x0B;

    constexpr std::uint8_t kDescTypeDevice = 1;
    constexpr std::uint8_t kDescTypeConfiguration = 2;
    constexpr std::uint8_t kDescTypeString = 3;
    constexpr std::uint8_t kDescTypeHid = 0x21;
    constexpr std::uint8_t kDescTypeReport = 0x22;

    constexpr std::uint8_t kTypeStandard = 0x00;
    constexpr std::uint8_t kTypeClass = 0x20;
    constexpr std::uint8_t kDirIn = 0x80;
    constexpr std::uint8_t kRecipientInterface = 0x01;

    constexpr std::uint8_t kCmsisDapInfo = 0x00;
    constexpr std::uint8_t kCmsisDapHostStatus = 0x01;
    constexpr std::uint8_t kCmsisDapConnect = 0x02;
    constexpr std::uint8_t kCmsisDapDisconnect = 0x03;
    constexpr std::uint8_t kCmsisDapTransferConfigure = 0x04;
    constexpr std::uint8_t kCmsisDapTransfer = 0x05;
    constexpr std::uint8_t kCmsisDapTransferBlock = 0x06;
    constexpr std::uint8_t kCmsisDapWriteAbort = 0x08;
    constexpr std::uint8_t kCmsisDapDelay = 0x09;
    constexpr std::uint8_t kCmsisDapSwjPins = 0x10;
    constexpr std::uint8_t kCmsisDapSwjClock = 0x11;
    constexpr std::uint8_t kCmsisDapSwjSequence = 0x12;
    constexpr std::uint8_t kCmsisDapSwdConfigure = 0x13;
    constexpr std::uint8_t kCmsisDapInvalid = 0xFF;

    constexpr std::uint8_t kDapInfoVendor = 1;
    constexpr std::uint8_t kDapInfoProduct = 2;
    constexpr std::uint8_t kDapInfoSerial = 3;
    constexpr std::uint8_t kDapInfoFwVersion = 4;
    constexpr std::uint8_t kDapInfoCapabilities = 0xF0;
    constexpr std::uint8_t kDapInfoPacketCount = 0xFE;
    constexpr std::uint8_t kDapInfoPacketSize = 0xFF;

    constexpr std::uint8_t kDapOk = 0x00;
    constexpr std::uint8_t kDapPortDisabled = 0x00;
    constexpr std::uint8_t kDapPortSwd = 0x01;
    constexpr std::uint8_t kDapTransferOk = 0x01;
    constexpr std::uint8_t kDapTransferError = 0x08;
    constexpr std::uint8_t kReqApndp = 1U << 0;
    constexpr std::uint8_t kReqRnw = 1U << 1;
    constexpr std::uint8_t kReqMatchValue = 1U << 4;
    constexpr std::uint8_t kReqMatchMask = 1U << 5;
    constexpr std::uint8_t kReqDpRdbuff = kReqRnw | (1U << 2) | (1U << 3);

    struct setup_packet {
        std::uint8_t bm_request_type;
        std::uint8_t b_request;
        std::uint16_t w_value;
        std::uint16_t w_index;
        std::uint16_t w_length;
    };

    struct usb_state {
        std::uint8_t config_value = 0;
        std::uint8_t idle_rate = 0;
        std::uint8_t protocol = 1;
        std::uint8_t dap_port = kDapPortDisabled;
        bool pending_ep0_out = false;
        std::uint16_t pending_out_length = 0;
        std::uint8_t ep0_out[kEp0Mps] = {};
        std::uint8_t ep0_in_zlp[1] = {};
        std::uint8_t ep0_in_data[2] = {};
        std::uint8_t hid_out[kHidPacketSize] = {};
        std::uint8_t hid_in[kHidPacketSize] = {};
    };

    inline usb_state g_state{};

    constexpr std::uint8_t device_descriptor[] = {
        0x12, 0x01, 0x00, 0x02, 0x00, 0x00, 0x00, kEp0Mps,
        0xFE, 0xCA, 0x01, 0x40, 0x00, 0x01, 0x01, 0x02,
        0x03, 0x01
    };

    constexpr std::uint8_t hid_report_descriptor[] = {
        0x06, 0x00, 0xFF,
        0x09, 0x01,
        0xA1, 0x01,
        0x09, 0x02,
        0x15, 0x00,
        0x26, 0xFF, 0x00,
        0x75, 0x08,
        0x95, 0x40,
        0x81, 0x02,
        0x09, 0x03,
        0x95, 0x40,
        0x91, 0x02,
        0xC0
    };

    constexpr std::uint8_t configuration_descriptor[] = {
        0x09, 0x02, 0x29, 0x00, 0x01, 0x01, 0x00, 0x80, 0x32,
        0x09, 0x04, 0x00, 0x00, 0x02, 0x03, 0x00, 0x00, 0x00,
        0x09, 0x21, 0x11, 0x01, 0x00, 0x01, 0x22, static_cast<std::uint8_t>(sizeof(hid_report_descriptor)), 0x00,
        0x07, 0x05, kHidEpIn, 0x03, 0x40, 0x00, 0x01,
        0x07, 0x05, kHidEpOut, 0x03, 0x40, 0x00, 0x01
    };

    constexpr std::uint8_t lang_id_descriptor[] = {0x04, 0x03, 0x09, 0x04};

    constexpr std::uint8_t manufacturer_string[] = {
        12, 0x03, 'C', 0, 'h', 0, 'a', 0, 'r', 0, 'm', 0
    };
    constexpr std::uint8_t product_string[] = {
        26, 0x03, 'C', 0, 'h', 0, 'a', 0, 'r', 0, 'm', 0, ' ', 0,
        'D', 0, 'A', 0, 'P', 0, 'L', 0, 'i', 0, 'n', 0, 'k', 0
    };
    constexpr std::uint8_t serial_string[] = {
        10, 0x03, '0', 0, '0', 0, '0', 0, '1', 0
    };
    constexpr char dap_vendor[] = "Charm";
    constexpr char dap_product[] = "Charm CMSIS-DAP";
    constexpr char dap_serial[] = "0001";
    constexpr char dap_fw[] = "0.1.0";

    inline std::uint16_t min_u16(const std::uint16_t a, const std::uint16_t b) noexcept {
        return (a < b) ? a : b;
    }

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

    inline std::uint8_t dap_transfer_once(const std::uint8_t request, std::uint32_t& data) noexcept {
        if ((request & kReqRnw) != 0U && (request & kReqApndp) != 0U) {
            std::uint32_t posted_dummy = 0;
            auto ack = daplink::swd::transfer(request, posted_dummy);
            if (ack != kDapTransferOk) {
                return ack;
            }
            return daplink::swd::transfer(kReqDpRdbuff, data);
        }
        return daplink::swd::transfer(request, data);
    }

    inline setup_packet parse_setup(const PCD_HandleTypeDef& hpcd) noexcept {
        const auto* b = reinterpret_cast<const std::uint8_t*>(hpcd.Setup);
        return {
            b[0],
            b[1],
            static_cast<std::uint16_t>(b[2] | (static_cast<std::uint16_t>(b[3]) << 8)),
            static_cast<std::uint16_t>(b[4] | (static_cast<std::uint16_t>(b[5]) << 8)),
            static_cast<std::uint16_t>(b[6] | (static_cast<std::uint16_t>(b[7]) << 8))
        };
    }

    inline void ep0_stall(PCD_HandleTypeDef& hpcd) noexcept {
        (void)HAL_PCD_EP_SetStall(&hpcd, 0x00);
        (void)HAL_PCD_EP_SetStall(&hpcd, 0x80);
    }

    inline void ep0_send(PCD_HandleTypeDef& hpcd, const std::uint8_t* data, std::uint16_t len) noexcept {
        (void)HAL_PCD_EP_Transmit(&hpcd, 0x80, const_cast<std::uint8_t*>(data), len);
    }

    inline void ep0_status_in(PCD_HandleTypeDef& hpcd) noexcept {
        (void)HAL_PCD_EP_Transmit(&hpcd, 0x80, g_state.ep0_in_zlp, 0);
    }

    inline void ep0_prepare_out(PCD_HandleTypeDef& hpcd, std::uint16_t len) noexcept {
        g_state.pending_ep0_out = true;
        g_state.pending_out_length = min_u16(len, kEp0Mps);
        (void)HAL_PCD_EP_Receive(&hpcd, 0x00, g_state.ep0_out, g_state.pending_out_length);
    }

    inline void hid_out_rearm(PCD_HandleTypeDef& hpcd) noexcept {
        (void)HAL_PCD_EP_Receive(&hpcd, kHidEpOut, g_state.hid_out, kHidEpMps);
    }

    inline void hid_in_send(PCD_HandleTypeDef& hpcd, const std::uint8_t len) noexcept {
        (void)len;
        (void)HAL_PCD_EP_Transmit(&hpcd, kHidEpIn, g_state.hid_in, kHidEpMps);
    }

    inline std::uint8_t fill_dap_info_payload(const std::uint8_t info_id, std::uint8_t* out) noexcept {
        switch (info_id) {
            case kDapInfoVendor: {
                constexpr std::uint8_t n = static_cast<std::uint8_t>(sizeof(dap_vendor));
                for (std::uint8_t i = 0; i < n; ++i) out[i] = static_cast<std::uint8_t>(dap_vendor[i]);
                return n;
            }
            case kDapInfoProduct: {
                constexpr std::uint8_t n = static_cast<std::uint8_t>(sizeof(dap_product));
                for (std::uint8_t i = 0; i < n; ++i) out[i] = static_cast<std::uint8_t>(dap_product[i]);
                return n;
            }
            case kDapInfoSerial: {
                constexpr std::uint8_t n = static_cast<std::uint8_t>(sizeof(dap_serial));
                for (std::uint8_t i = 0; i < n; ++i) out[i] = static_cast<std::uint8_t>(dap_serial[i]);
                return n;
            }
            case kDapInfoFwVersion: {
                constexpr std::uint8_t n = static_cast<std::uint8_t>(sizeof(dap_fw));
                for (std::uint8_t i = 0; i < n; ++i) out[i] = static_cast<std::uint8_t>(dap_fw[i]);
                return n;
            }
            case kDapInfoCapabilities:
                out[0] = 0x11; // SWD + Atomic commands
                return 1;
            case kDapInfoPacketCount:
                out[0] = 1;
                return 1;
            case kDapInfoPacketSize:
                out[0] = static_cast<std::uint8_t>(kHidPacketSize & 0xFF);
                out[1] = static_cast<std::uint8_t>((kHidPacketSize >> 8) & 0xFF);
                return 2;
            default:
                return 0;
        }
    }

    inline void build_dap_response() noexcept {
        for (std::size_t i = 0; i < kHidPacketSize; ++i) {
            g_state.hid_in[i] = 0;
        }

        const std::uint8_t cmd = g_state.hid_out[0];
        g_state.hid_in[0] = cmd;

        switch (cmd) {
            case kCmsisDapInfo: {
                const auto info_len = fill_dap_info_payload(g_state.hid_out[1], &g_state.hid_in[2]);
                g_state.hid_in[1] = info_len;
                break;
            }
            case kCmsisDapHostStatus:
                g_state.hid_in[1] = kDapOk;
                break;
            case kCmsisDapConnect:
                if (g_state.hid_out[1] == 0 || g_state.hid_out[1] == kDapPortSwd) {
                    if (daplink::swd::connect_swd()) {
                        g_state.dap_port = kDapPortSwd;
                        g_state.hid_in[1] = kDapPortSwd;
                    } else {
                        g_state.dap_port = kDapPortDisabled;
                        g_state.hid_in[1] = kDapPortDisabled;
                    }
                } else {
                    g_state.hid_in[1] = kDapPortDisabled;
                }
                break;
            case kCmsisDapDisconnect:
                daplink::swd::disconnect();
                g_state.dap_port = kDapPortDisabled;
                g_state.hid_in[1] = kDapOk;
                break;
            case kCmsisDapTransferConfigure: {
                const auto idle = g_state.hid_out[1];
                const auto retry = static_cast<std::uint16_t>(g_state.hid_out[2] | (g_state.hid_out[3] << 8));
                daplink::swd::set_transfer_config(idle, retry);
                g_state.hid_in[1] = kDapOk;
                break;
            }
            case kCmsisDapWriteAbort:
            case kCmsisDapDelay:
            case kCmsisDapSwjClock:
                g_state.hid_in[1] = kDapOk;
                break;
            case kCmsisDapSwjPins: {
                const auto value = g_state.hid_out[1];
                const auto select = g_state.hid_out[2];
                g_state.hid_in[1] = daplink::swd::swj_pins(value, select);
                break;
            }
            case kCmsisDapSwjSequence: {
                std::uint32_t bits = g_state.hid_out[1];
                if (bits == 0) {
                    bits = 256;
                }
                daplink::swd::swj_sequence_bits(&g_state.hid_out[2], bits);
                g_state.hid_in[1] = kDapOk;
                break;
            }
            case kCmsisDapSwdConfigure:
                daplink::swd::set_swd_config(g_state.hid_out[1]);
                g_state.hid_in[1] = kDapOk;
                break;
            case kCmsisDapTransfer: {
                std::uint8_t response_count = 0;
                std::uint8_t response_value = kDapTransferOk;
                std::uint8_t in_idx = 3;
                std::uint8_t out_idx = 3;
                const auto transfer_count = g_state.hid_out[2];

                if (g_state.dap_port != kDapPortSwd) {
                    response_value = kDapTransferError;
                } else {
                    for (std::uint8_t i = 0; i < transfer_count; ++i) {
                        if (in_idx >= kHidPacketSize) {
                            response_value = kDapTransferError;
                            break;
                        }
                        const auto request = g_state.hid_out[in_idx++];
                        std::uint32_t data = 0;
                        const bool is_read = (request & kReqRnw) != 0U;
                        const bool is_match_value = (request & kReqMatchValue) != 0U;
                        const bool is_match_mask = (request & kReqMatchMask) != 0U;

                        if (!is_read || is_match_value || is_match_mask) {
                            if ((in_idx + 3) >= kHidPacketSize) {
                                response_value = kDapTransferError;
                                break;
                            }
                            data = read_le32(&g_state.hid_out[in_idx]);
                            in_idx = static_cast<std::uint8_t>(in_idx + 4);
                        }

                        if (is_match_value || is_match_mask) {
                            // TODO(daplink): implement CMSIS-DAP match-value/mask flow.
                            response_value = kDapTransferError;
                            break;
                        }

                        const auto ack = dap_transfer_once(request, data);
                        if (ack != kDapTransferOk) {
                            response_value = ack;
                            break;
                        }

                        response_count = static_cast<std::uint8_t>(response_count + 1);
                        if (is_read) {
                            if ((out_idx + 3) >= kHidPacketSize) {
                                response_value = kDapTransferError;
                                break;
                            }
                            write_le32(&g_state.hid_in[out_idx], data);
                            out_idx = static_cast<std::uint8_t>(out_idx + 4);
                        }
                    }
                }

                g_state.hid_in[1] = response_count;
                g_state.hid_in[2] = response_value;
                break;
            }
            case kCmsisDapTransferBlock: {
                std::uint16_t response_count = 0;
                std::uint8_t response_value = kDapTransferOk;
                std::uint8_t in_idx = 4;
                std::uint8_t out_idx = 4;
                const auto transfer_count = static_cast<std::uint16_t>(g_state.hid_out[1] | (g_state.hid_out[2] << 8));
                const auto request = g_state.hid_out[3];
                const bool is_read = (request & kReqRnw) != 0U;

                if (g_state.dap_port != kDapPortSwd) {
                    response_value = kDapTransferError;
                } else if ((request & (kReqMatchValue | kReqMatchMask)) != 0U) {
                    response_value = kDapTransferError;
                } else {
                    for (std::uint16_t i = 0; i < transfer_count; ++i) {
                        std::uint32_t data = 0;
                        if (!is_read) {
                            if ((in_idx + 3) >= kHidPacketSize) {
                                response_value = kDapTransferError;
                                break;
                            }
                            data = read_le32(&g_state.hid_out[in_idx]);
                            in_idx = static_cast<std::uint8_t>(in_idx + 4);
                        }

                        const auto ack = dap_transfer_once(request, data);
                        if (ack != kDapTransferOk) {
                            response_value = ack;
                            break;
                        }

                        ++response_count;
                        if (is_read) {
                            if ((out_idx + 3) >= kHidPacketSize) {
                                response_value = kDapTransferError;
                                break;
                            }
                            write_le32(&g_state.hid_in[out_idx], data);
                            out_idx = static_cast<std::uint8_t>(out_idx + 4);
                        }
                    }
                }

                g_state.hid_in[1] = static_cast<std::uint8_t>(response_count & 0xFFU);
                g_state.hid_in[2] = static_cast<std::uint8_t>((response_count >> 8) & 0xFFU);
                g_state.hid_in[3] = response_value;
                break;
            }
            default:
                g_state.hid_in[0] = kCmsisDapInvalid;
                break;
        }
    }

    inline void handle_get_descriptor(PCD_HandleTypeDef& hpcd, const setup_packet& s) noexcept {
        const std::uint8_t desc_type = static_cast<std::uint8_t>(s.w_value >> 8);
        const std::uint8_t desc_index = static_cast<std::uint8_t>(s.w_value & 0xFF);
        const std::uint8_t* data = nullptr;
        std::uint16_t len = 0;

        switch (desc_type) {
            case kDescTypeDevice:
                data = device_descriptor;
                len = static_cast<std::uint16_t>(sizeof(device_descriptor));
                break;
            case kDescTypeConfiguration:
                data = configuration_descriptor;
                len = static_cast<std::uint16_t>(sizeof(configuration_descriptor));
                break;
            case kDescTypeString:
                if (desc_index == 0) {
                    data = lang_id_descriptor;
                    len = static_cast<std::uint16_t>(sizeof(lang_id_descriptor));
                } else if (desc_index == 1) {
                    data = manufacturer_string;
                    len = static_cast<std::uint16_t>(sizeof(manufacturer_string));
                } else if (desc_index == 2) {
                    data = product_string;
                    len = static_cast<std::uint16_t>(sizeof(product_string));
                } else if (desc_index == 3) {
                    data = serial_string;
                    len = static_cast<std::uint16_t>(sizeof(serial_string));
                }
                break;
            case kDescTypeReport:
                data = hid_report_descriptor;
                len = static_cast<std::uint16_t>(sizeof(hid_report_descriptor));
                break;
            case kDescTypeHid:
                data = &configuration_descriptor[18];
                len = 9;
                break;
            default:
                break;
        }

        if (data == nullptr) {
            ep0_stall(hpcd);
            return;
        }
        if (len > s.w_length) {
            len = s.w_length;
        }
        ep0_send(hpcd, data, len);
    }
} // namespace daplink::usb_minimal::detail

export namespace daplink::usb_minimal {
    using namespace detail;

    inline bool attach(PCD_HandleTypeDef& hpcd) noexcept {
        const auto r0 = HAL_PCDEx_PMAConfig(&hpcd, 0x00, PCD_SNG_BUF, 0x18);
        const auto r1 = HAL_PCDEx_PMAConfig(&hpcd, 0x80, PCD_SNG_BUF, 0x58);
        const auto r2 = HAL_PCDEx_PMAConfig(&hpcd, kHidEpIn, PCD_SNG_BUF, 0xC0);
        const auto r3 = HAL_PCDEx_PMAConfig(&hpcd, kHidEpOut, PCD_SNG_BUF, 0x100);
        return (r0 == HAL_OK) && (r1 == HAL_OK) && (r2 == HAL_OK) && (r3 == HAL_OK);
    }

    inline void on_reset(PCD_HandleTypeDef& hpcd) noexcept {
        g_state = {};
        (void)HAL_PCD_SetAddress(&hpcd, 0);
        (void)HAL_PCD_EP_Open(&hpcd, 0x00, kEp0Mps, EP_TYPE_CTRL);
        (void)HAL_PCD_EP_Open(&hpcd, 0x80, kEp0Mps, EP_TYPE_CTRL);
        (void)HAL_PCD_EP_Open(&hpcd, kHidEpIn, kHidEpMps, EP_TYPE_INTR);
        (void)HAL_PCD_EP_Open(&hpcd, kHidEpOut, kHidEpMps, EP_TYPE_INTR);
        (void)HAL_PCD_EP_Receive(&hpcd, 0x00, g_state.ep0_out, kEp0Mps);
        hid_out_rearm(hpcd);
    }

    inline void on_setup_stage(PCD_HandleTypeDef& hpcd) noexcept {
        const auto s = parse_setup(hpcd);
        const auto req_type = static_cast<std::uint8_t>(s.bm_request_type & 0x60);
        const bool dir_in = (s.bm_request_type & kDirIn) != 0;

        if (req_type == kTypeStandard) {
            switch (s.b_request) {
                case kReqGetDescriptor:
                    if (dir_in) {
                        handle_get_descriptor(hpcd, s);
                    } else {
                        ep0_stall(hpcd);
                    }
                    return;
                case kReqSetAddress:
                    if (!dir_in && s.w_index == 0 && s.w_length == 0 && (s.w_value < 128)) {
                        (void)HAL_PCD_SetAddress(&hpcd, static_cast<std::uint8_t>(s.w_value & 0x7F));
                        ep0_status_in(hpcd);
                    } else {
                        ep0_stall(hpcd);
                    }
                    return;
                case kReqSetConfiguration:
                    if (!dir_in && s.w_length == 0) {
                        g_state.config_value = static_cast<std::uint8_t>(s.w_value & 0xFF);
                        ep0_status_in(hpcd);
                    } else {
                        ep0_stall(hpcd);
                    }
                    return;
                case kReqGetConfiguration:
                    g_state.ep0_in_data[0] = g_state.config_value;
                    ep0_send(hpcd, g_state.ep0_in_data, 1);
                    return;
                case kReqGetStatus:
                    g_state.ep0_in_data[0] = 0;
                    g_state.ep0_in_data[1] = 0;
                    ep0_send(hpcd, g_state.ep0_in_data, 2);
                    return;
                case kReqGetInterface:
                    g_state.ep0_in_data[0] = 0;
                    ep0_send(hpcd, g_state.ep0_in_data, 1);
                    return;
                case kReqSetInterface:
                case kReqClearFeature:
                case kReqSetFeature:
                    ep0_status_in(hpcd);
                    return;
                default:
                    ep0_stall(hpcd);
                    return;
            }
        }

        if (req_type == kTypeClass && ((s.bm_request_type & 0x1F) == kRecipientInterface)) {
            switch (s.b_request) {
                case kReqGetReport:
                    g_state.ep0_in_data[0] = 0;
                    ep0_send(hpcd, g_state.ep0_in_data, 1);
                    return;
                case kReqGetIdle:
                    g_state.ep0_in_data[0] = g_state.idle_rate;
                    ep0_send(hpcd, g_state.ep0_in_data, 1);
                    return;
                case kReqGetProtocol:
                    g_state.ep0_in_data[0] = g_state.protocol;
                    ep0_send(hpcd, g_state.ep0_in_data, 1);
                    return;
                case kReqSetIdle:
                    if (!dir_in) {
                        g_state.idle_rate = static_cast<std::uint8_t>((s.w_value >> 8) & 0xFF);
                        ep0_status_in(hpcd);
                    } else {
                        ep0_stall(hpcd);
                    }
                    return;
                case kReqSetProtocol:
                    if (!dir_in) {
                        g_state.protocol = static_cast<std::uint8_t>(s.w_value & 0xFF);
                        ep0_status_in(hpcd);
                    } else {
                        ep0_stall(hpcd);
                    }
                    return;
                case kReqSetReport:
                    if (!dir_in && s.w_length > 0) {
                        ep0_prepare_out(hpcd, s.w_length);
                    } else if (!dir_in) {
                        ep0_status_in(hpcd);
                    } else {
                        ep0_stall(hpcd);
                    }
                    return;
                default:
                    ep0_stall(hpcd);
                    return;
            }
        }

        ep0_stall(hpcd);
    }

    inline void on_data_out_stage(PCD_HandleTypeDef& hpcd, std::uint8_t epnum) noexcept {
        if (epnum == 0 && g_state.pending_ep0_out) {
            g_state.pending_ep0_out = false;
            g_state.pending_out_length = 0;
            ep0_status_in(hpcd);
            return;
        }
        if (epnum == 1) {
            build_dap_response();
            hid_in_send(hpcd, static_cast<std::uint8_t>(kHidPacketSize));
            hid_out_rearm(hpcd);
        }
    }

    inline void on_data_in_stage(PCD_HandleTypeDef&, std::uint8_t) noexcept {}
}
