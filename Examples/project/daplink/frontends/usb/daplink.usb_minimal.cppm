module;

#include "daplink_port_api.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
export module daplink.usb_minimal;

import daplink.usb_device_model;

namespace daplink::usb_minimal::detail {
    namespace model = daplink::usb_device_model;
    using UsbPcdHandle = daplink::port::UsbPcdHandle;
    using UsbEndpointType = daplink::port::UsbEndpointType;
    using UsbLayout = daplink::port::UsbLayout;
    constexpr bool kEnableHid = model::kEnableHid;
    constexpr bool kEnableCdc = model::kEnableCdc;
    constexpr bool kCdcHasCmdEp = model::kCdcHasCmdEp;

    constexpr std::uint8_t kEp0Mps = model::kEp0Mps;
    constexpr std::uint8_t kHidEpOut = model::kHidEpOut;
    constexpr std::uint8_t kHidEpIn = model::kHidEpIn;
    constexpr std::uint16_t kHidEpMps = model::kHidEpMps;
    constexpr std::size_t kHidPacketSize = model::kHidPacketSize;
    constexpr std::uint8_t kCdcEpCmd = model::kCdcEpCmd;
    constexpr std::uint8_t kCdcEpOut = model::kCdcEpOut;
    constexpr std::uint8_t kCdcEpIn = model::kCdcEpIn;
    constexpr std::uint16_t kCdcEpCmdMps = model::kCdcEpCmdMps;
    constexpr std::uint16_t kCdcEpMps = model::kCdcEpMps;

    constexpr std::uint16_t kPmaEp0Out = UsbLayout::kUsbPmaEp0Out;
    constexpr std::uint16_t kPmaEp0In = UsbLayout::kUsbPmaEp0In;
    constexpr std::uint16_t kPmaHidIn = UsbLayout::kUsbPmaHidIn;
    constexpr std::uint16_t kPmaHidOut = UsbLayout::kUsbPmaHidOut;
    constexpr std::uint16_t kPmaCdcCmd = UsbLayout::kUsbPmaCdcCmd;
    constexpr std::uint16_t kPmaCdcOut = UsbLayout::kUsbPmaCdcOut;
    constexpr std::uint16_t kPmaCdcIn = UsbLayout::kUsbPmaCdcIn;
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
    constexpr std::uint8_t kCdcCommInterface = model::kCdcCommInterface;
    constexpr std::uint8_t kHidInterface = model::kHidInterface;

    struct setup_packet {
        std::uint8_t bm_request_type;
        std::uint8_t b_request;
        std::uint16_t w_value;
        std::uint16_t w_index;
        std::uint16_t w_length;
    };

    enum class ep0_out_kind : std::uint8_t {
        none = 0,
        hid_set_report,
        cdc_line_coding,
        cdc_encapsulated,
        cdc_comm_feature,
    };

    struct cdc_line_coding {
        std::uint32_t dwDTERate = 115200;
        std::uint8_t bCharFormat = 0;
        std::uint8_t bParityType = 0;
        std::uint8_t bDataBits = 8;
    };

    struct hid_state {
        std::uint8_t hid_out[2][kHidPacketSize] = {};
        std::uint16_t hid_out_len[2] = {};
        std::uint8_t hid_in[kHidPacketSize] = {};
        volatile std::uint8_t hid_out_full_mask = 0;
        volatile std::uint8_t hid_out_active_index = 0;
        volatile std::uint8_t hid_out_read_index = 0;
        volatile bool hid_out_armed = false;
        volatile bool hid_in_busy = false;
    };

    struct cdc_state {
        std::uint8_t cdc_out[2][kCdcEpMps] = {};
        std::uint16_t cdc_out_len[2] = {};
        std::uint8_t cdc_in[kCdcEpMps] = {};
        volatile std::uint8_t cdc_out_full_mask = 0;
        volatile std::uint8_t cdc_out_active_index = 0;
        volatile std::uint8_t cdc_out_read_index = 0;
        volatile bool cdc_out_armed = false;
        volatile bool cdc_in_busy = false;
        std::uint32_t cdc_in_last_ms = 0;
        std::uint16_t cdc_control_line_state = 0;
        cdc_line_coding cdc_line = {};
    };

    struct usb_state {
        std::uint8_t config_value = 0;
        std::uint8_t idle_rate = 0;
        std::uint8_t protocol = 1;
        std::uint8_t pending_address = 0;
        bool address_pending = false;
        const std::uint8_t* ep0_in_ptr = nullptr;
        std::uint16_t ep0_in_remaining = 0;
        std::uint16_t ep0_in_req_len = 0;
        bool ep0_in_active = false;
        bool ep0_in_need_zlp = false;
        bool pending_ep0_out = false;
        ep0_out_kind pending_ep0_kind = ep0_out_kind::none;
        std::uint16_t pending_out_length = 0;
        std::uint8_t ep0_out[kEp0Mps] = {};
        std::uint8_t ep0_in_zlp[1] = {};
        std::uint8_t ep0_in_data[2] = {};
        hid_state hid{};
        cdc_state cdc{};
        volatile bool reset_pending = false;
        UsbPcdHandle* hpcd = nullptr;
    };

    inline usb_state g_state{};
    constexpr std::uint32_t kCdcInTimeoutMs = model::kCdcInTimeoutMs;
    inline constexpr std::array<std::uint8_t, kHidPacketSize> kEmptyHidPacket = {};
    inline std::array<std::uint8_t, kHidPacketSize> kHidScratch = {};

    inline std::uint16_t min_u16(const std::uint16_t a, const std::uint16_t b) noexcept {
        return (a < b) ? a : b;
    }

    inline setup_packet parse_setup(const UsbPcdHandle& hpcd) noexcept {
        std::uint8_t b[8] = {};
        daplink::port::usb_copy_setup_packet(hpcd, b);
        return {
            b[0],
            b[1],
            static_cast<std::uint16_t>(b[2] | (static_cast<std::uint16_t>(b[3]) << 8)),
            static_cast<std::uint16_t>(b[4] | (static_cast<std::uint16_t>(b[5]) << 8)),
            static_cast<std::uint16_t>(b[6] | (static_cast<std::uint16_t>(b[7]) << 8))
        };
    }

    inline void ep0_stall(UsbPcdHandle& hpcd) noexcept {
        (void)daplink::port::usb_ep_set_stall(hpcd, 0x00);
        (void)daplink::port::usb_ep_set_stall(hpcd, 0x80);
    }

    inline void ep0_send(UsbPcdHandle& hpcd,
                         const std::uint8_t* data,
                         std::uint16_t len,
                         std::uint16_t req_len) noexcept {
        if (len == 0) {
            g_state.ep0_in_ptr = nullptr;
            g_state.ep0_in_remaining = 0;
            g_state.ep0_in_req_len = req_len;
            g_state.ep0_in_active = false;
            g_state.ep0_in_need_zlp = false;
            (void)daplink::port::usb_ep_transmit(hpcd, 0x80, g_state.ep0_in_zlp, 0);
            return;
        }

        const std::uint16_t send_len = (len > kEp0Mps) ? kEp0Mps : len;
        g_state.ep0_in_ptr = data + send_len;
        g_state.ep0_in_remaining = static_cast<std::uint16_t>(len - send_len);
        g_state.ep0_in_req_len = req_len;
        g_state.ep0_in_need_zlp = (g_state.ep0_in_remaining == 0) && (len < req_len) &&
                                  ((len % kEp0Mps) == 0);
        g_state.ep0_in_active = g_state.ep0_in_remaining > 0 || g_state.ep0_in_need_zlp;
        (void)daplink::port::usb_ep_transmit(
            hpcd, 0x80, const_cast<std::uint8_t*>(data), send_len);
    }

    inline void ep0_status_in(UsbPcdHandle& hpcd) noexcept {
        (void)daplink::port::usb_ep_transmit(hpcd, 0x80, g_state.ep0_in_zlp, 0);
    }

    inline void ep0_prepare_out(UsbPcdHandle& hpcd, std::uint16_t len, const ep0_out_kind kind) noexcept {
        g_state.pending_ep0_out = true;
        g_state.pending_ep0_kind = kind;
        g_state.pending_out_length = min_u16(len, kEp0Mps);
        (void)daplink::port::usb_ep_receive(hpcd, 0x00, g_state.ep0_out, g_state.pending_out_length);
    }

    inline void hid_arm_out(UsbPcdHandle& hpcd, const std::uint8_t index) noexcept {
        if constexpr (!kEnableHid) {
            (void)hpcd;
            (void)index;
        } else {
            g_state.hid.hid_out_active_index = index;
            g_state.hid.hid_out_armed = true;
            (void)daplink::port::usb_ep_receive(hpcd, kHidEpOut, g_state.hid.hid_out[index], kHidEpMps);
        }
    }

    inline void hid_try_rearm(UsbPcdHandle& hpcd) noexcept {
        if constexpr (!kEnableHid) {
            (void)hpcd;
        } else {
            if (g_state.hid.hid_out_armed) {
                return;
            }
            if ((g_state.hid.hid_out_full_mask & 0x3U) == 0x3U) {
                return;
            }
            const std::uint8_t free_index = (g_state.hid.hid_out_full_mask & 0x1U) ? 1U : 0U;
            hid_arm_out(hpcd, free_index);
        }
    }

    inline bool hid_in_send(UsbPcdHandle& hpcd, const std::uint16_t len) noexcept {
        if constexpr (!kEnableHid) {
            (void)hpcd;
            (void)len;
            return false;
        } else {
            if (g_state.hid.hid_in_busy) {
                return false;
            }
            const std::uint16_t send_len = (len > kHidEpMps) ? kHidEpMps : len;
            g_state.hid.hid_in_busy = true;
            (void)daplink::port::usb_ep_transmit(hpcd, kHidEpIn, g_state.hid.hid_in, send_len);
            return true;
        }
    }

    inline void cdc_arm_out(UsbPcdHandle& hpcd, const std::uint8_t index) noexcept {
        if constexpr (!kEnableCdc) {
            (void)hpcd;
            (void)index;
        } else {
            g_state.cdc.cdc_out_active_index = index;
            g_state.cdc.cdc_out_armed = true;
            (void)daplink::port::usb_ep_receive(hpcd, kCdcEpOut, g_state.cdc.cdc_out[index], kCdcEpMps);
        }
    }

    inline void cdc_try_rearm(UsbPcdHandle& hpcd) noexcept {
        if constexpr (!kEnableCdc) {
            (void)hpcd;
        } else {
            if (g_state.cdc.cdc_out_armed) {
                return;
            }
            if ((g_state.cdc.cdc_out_full_mask & 0x3U) == 0x3U) {
                return;
            }
            const std::uint8_t free_index = (g_state.cdc.cdc_out_full_mask & 0x1U) ? 1U : 0U;
            cdc_arm_out(hpcd, free_index);
        }
    }

    inline bool cdc_in_send(UsbPcdHandle& hpcd, const std::uint8_t* data, const std::uint16_t len) noexcept {
        if constexpr (!kEnableCdc) {
            (void)hpcd;
            (void)data;
            (void)len;
            return false;
        } else {
            if (g_state.cdc.cdc_in_busy) {
                return false;
            }
            const std::uint16_t send_len = (len > kCdcEpMps) ? kCdcEpMps : len;
            for (std::uint16_t i = 0; i < send_len; ++i) {
                g_state.cdc.cdc_in[i] = data[i];
            }
            g_state.cdc.cdc_in_busy = true;
            g_state.cdc.cdc_in_last_ms = daplink::port::tick_ms();
            (void)daplink::port::usb_ep_transmit(hpcd, kCdcEpIn, g_state.cdc.cdc_in, send_len);
            return true;
        }
    }

    inline void handle_get_descriptor(UsbPcdHandle& hpcd, const setup_packet& s) noexcept {
        const auto descriptor = model::descriptor(
            static_cast<std::uint8_t>(s.w_value >> 8),
            static_cast<std::uint8_t>(s.w_value & 0xFFU));
        if (descriptor.empty()) {
            ep0_stall(hpcd);
            return;
        }
        std::uint16_t len = static_cast<std::uint16_t>(descriptor.size());
        if (len > s.w_length) {
            len = s.w_length;
        }
        ep0_send(hpcd, descriptor.data(), len, s.w_length);
    }
} // namespace daplink::usb_minimal::detail

export namespace daplink::usb_minimal {
    using namespace detail;

    constexpr std::size_t hid_packet_size = kHidPacketSize;

    struct cdc_line_config {
        std::uint32_t baud = 115200;
        std::uint8_t stop_bits = 0;
        std::uint8_t parity = 0;
        std::uint8_t data_bits = 8;
    };

    inline bool attach(UsbPcdHandle& hpcd) noexcept {
        g_state.hpcd = &hpcd;
        const bool base_ok =
            daplink::port::usb_pma_config_single_buffer(hpcd, 0x00, kPmaEp0Out) &&
            daplink::port::usb_pma_config_single_buffer(hpcd, 0x80, kPmaEp0In);
        bool hid_ok = true;
        if constexpr (kEnableHid) {
            hid_ok =
                daplink::port::usb_pma_config_single_buffer(hpcd, kHidEpIn, kPmaHidIn) &&
                daplink::port::usb_pma_config_single_buffer(hpcd, kHidEpOut, kPmaHidOut);
        }
        bool cdc_ok = true;
        if constexpr (kEnableCdc) {
            if constexpr (kCdcHasCmdEp) {
                cdc_ok =
                    daplink::port::usb_pma_config_single_buffer(hpcd, kCdcEpCmd, kPmaCdcCmd) &&
                    daplink::port::usb_pma_config_single_buffer(hpcd, kCdcEpOut, kPmaCdcOut) &&
                    daplink::port::usb_pma_config_single_buffer(hpcd, kCdcEpIn, kPmaCdcIn);
            } else {
                cdc_ok =
                    daplink::port::usb_pma_config_single_buffer(hpcd, kCdcEpOut, kPmaCdcOut) &&
                    daplink::port::usb_pma_config_single_buffer(hpcd, kCdcEpIn, kPmaCdcIn);
            }
        }
        return base_ok && hid_ok && cdc_ok;
    }

    inline void on_reset(UsbPcdHandle& hpcd) noexcept {
        g_state = {};
        g_state.hpcd = &hpcd;
        g_state.reset_pending = true;
        (void)daplink::port::usb_set_address(hpcd, 0);
        (void)daplink::port::usb_ep_open(hpcd, 0x00, kEp0Mps, UsbEndpointType::control);
        (void)daplink::port::usb_ep_open(hpcd, 0x80, kEp0Mps, UsbEndpointType::control);
        if constexpr (kEnableHid) {
            (void)daplink::port::usb_ep_open(hpcd, kHidEpIn, kHidEpMps, UsbEndpointType::interrupt);
            (void)daplink::port::usb_ep_open(hpcd, kHidEpOut, kHidEpMps, UsbEndpointType::interrupt);
        }
        if constexpr (kEnableCdc) {
            if constexpr (kCdcHasCmdEp) {
                (void)daplink::port::usb_ep_open(
                    hpcd, kCdcEpCmd, kCdcEpCmdMps, UsbEndpointType::interrupt);
            }
            (void)daplink::port::usb_ep_open(hpcd, kCdcEpIn, kCdcEpMps, UsbEndpointType::bulk);
            (void)daplink::port::usb_ep_open(hpcd, kCdcEpOut, kCdcEpMps, UsbEndpointType::bulk);
        }
        (void)daplink::port::usb_ep_receive(hpcd, 0x00, g_state.ep0_out, kEp0Mps);
        if constexpr (kEnableHid) {
            hid_arm_out(hpcd, 0);
        }
        if constexpr (kEnableCdc) {
            cdc_arm_out(hpcd, 0);
        }
    }

    inline void on_setup_stage(UsbPcdHandle& hpcd) noexcept {
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
                        g_state.pending_address = static_cast<std::uint8_t>(s.w_value & 0x7F);
                        g_state.address_pending = true;
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
                    ep0_send(hpcd, g_state.ep0_in_data, 1, s.w_length);
                    return;
                case kReqGetStatus:
                    g_state.ep0_in_data[0] = 0;
                    g_state.ep0_in_data[1] = 0;
                    ep0_send(hpcd, g_state.ep0_in_data, 2, s.w_length);
                    return;
                case kReqGetInterface:
                    g_state.ep0_in_data[0] = 0;
                    ep0_send(hpcd, g_state.ep0_in_data, 1, s.w_length);
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

        if constexpr (kEnableCdc) {
            if (req_type == kTypeClass && ((s.bm_request_type & 0x1F) == kRecipientInterface) &&
                (static_cast<std::uint8_t>(s.w_index & 0xFFU) == kCdcCommInterface)) {
                switch (s.b_request) {
                    case 0x00: // SEND_ENCAPSULATED_COMMAND
                        if (!dir_in && s.w_length <= kEp0Mps) {
                            ep0_prepare_out(hpcd, s.w_length, ep0_out_kind::cdc_encapsulated);
                        } else if (!dir_in) {
                            ep0_status_in(hpcd);
                        } else {
                            ep0_stall(hpcd);
                        }
                        return;
                    case 0x01: // GET_ENCAPSULATED_RESPONSE
                        if (dir_in) {
                            ep0_send(hpcd, g_state.ep0_in_zlp, 0, s.w_length);
                        } else {
                            ep0_stall(hpcd);
                        }
                        return;
                    case 0x02: // SET_COMM_FEATURE
                        if (!dir_in && s.w_length == 2) {
                            ep0_prepare_out(hpcd, s.w_length, ep0_out_kind::cdc_comm_feature);
                        } else if (!dir_in && s.w_length == 0) {
                            ep0_status_in(hpcd);
                        } else {
                            ep0_stall(hpcd);
                        }
                        return;
                    case 0x04: // CLEAR_COMM_FEATURE
                        if (!dir_in && s.w_length == 0) {
                            ep0_status_in(hpcd);
                        } else {
                            ep0_stall(hpcd);
                        }
                        return;
                    case 0x03: // GET_COMM_FEATURE
                        if (dir_in && s.w_length >= 2) {
                            g_state.ep0_in_data[0] = 0;
                            g_state.ep0_in_data[1] = 0;
                            ep0_send(hpcd, g_state.ep0_in_data, 2, s.w_length);
                        } else {
                            ep0_stall(hpcd);
                        }
                        return;
                    case 0x20: // SET_LINE_CODING
                        if (!dir_in && s.w_length == 7) {
                            ep0_prepare_out(hpcd, s.w_length, ep0_out_kind::cdc_line_coding);
                        } else {
                            ep0_stall(hpcd);
                        }
                        return;
                    case 0x21: // GET_LINE_CODING
                        if (dir_in && s.w_length == 7) {
                            std::uint8_t buf[7] = {};
                            const auto& lc = g_state.cdc.cdc_line;
                            buf[0] = static_cast<std::uint8_t>(lc.dwDTERate & 0xFFU);
                            buf[1] = static_cast<std::uint8_t>((lc.dwDTERate >> 8) & 0xFFU);
                            buf[2] = static_cast<std::uint8_t>((lc.dwDTERate >> 16) & 0xFFU);
                            buf[3] = static_cast<std::uint8_t>((lc.dwDTERate >> 24) & 0xFFU);
                            buf[4] = lc.bCharFormat;
                            buf[5] = lc.bParityType;
                            buf[6] = lc.bDataBits;
                            ep0_send(hpcd, buf, 7, s.w_length);
                        } else {
                            ep0_stall(hpcd);
                        }
                        return;
                    case 0x22: // SET_CONTROL_LINE_STATE
                        if (!dir_in && s.w_length == 0) {
                            g_state.cdc.cdc_control_line_state = s.w_value;
                            ep0_status_in(hpcd);
                        } else {
                            ep0_stall(hpcd);
                        }
                        return;
                    case 0x23: // SEND_BREAK
                        if (!dir_in && s.w_length == 0) {
                            ep0_status_in(hpcd);
                        } else {
                            ep0_stall(hpcd);
                        }
                        return;
                    default:
                        break;
                }
            }
        }

        if constexpr (kEnableHid) {
            if (req_type == kTypeClass && ((s.bm_request_type & 0x1F) == kRecipientInterface) &&
                (static_cast<std::uint8_t>(s.w_index & 0xFFU) == kHidInterface)) {
                switch (s.b_request) {
                    case kReqGetReport:
                        g_state.ep0_in_data[0] = 0;
                        ep0_send(hpcd, g_state.ep0_in_data, 1, s.w_length);
                        return;
                    case kReqGetIdle:
                        g_state.ep0_in_data[0] = g_state.idle_rate;
                        ep0_send(hpcd, g_state.ep0_in_data, 1, s.w_length);
                        return;
                    case kReqGetProtocol:
                        g_state.ep0_in_data[0] = g_state.protocol;
                        ep0_send(hpcd, g_state.ep0_in_data, 1, s.w_length);
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
                            if (s.w_length > kEp0Mps) {
                                ep0_stall(hpcd);
                                return;
                            }
                            ep0_prepare_out(hpcd, s.w_length, ep0_out_kind::hid_set_report);
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
        }

        ep0_stall(hpcd);
    }

    inline void on_data_out_stage(UsbPcdHandle& hpcd, std::uint8_t epnum) noexcept {
        if (epnum == 0 && g_state.pending_ep0_out) {
            g_state.pending_ep0_out = false;
            if constexpr (kEnableCdc) {
                if (g_state.pending_ep0_kind == ep0_out_kind::cdc_line_coding) {
                    if (g_state.pending_out_length >= 7) {
                        g_state.cdc.cdc_line.dwDTERate = static_cast<std::uint32_t>(g_state.ep0_out[0]) |
                                                     (static_cast<std::uint32_t>(g_state.ep0_out[1]) << 8) |
                                                     (static_cast<std::uint32_t>(g_state.ep0_out[2]) << 16) |
                                                     (static_cast<std::uint32_t>(g_state.ep0_out[3]) << 24);
                        g_state.cdc.cdc_line.bCharFormat = g_state.ep0_out[4];
                        g_state.cdc.cdc_line.bParityType = g_state.ep0_out[5];
                        g_state.cdc.cdc_line.bDataBits = g_state.ep0_out[6];
                    }
                } else if (g_state.pending_ep0_kind == ep0_out_kind::cdc_encapsulated) {
                    // Ignore encapsulated data.
                } else if (g_state.pending_ep0_kind == ep0_out_kind::cdc_comm_feature) {
                    // Ignore comm feature data.
                }
            }
            g_state.pending_out_length = 0;
            g_state.pending_ep0_kind = ep0_out_kind::none;
            ep0_status_in(hpcd);
            return;
        }
        if (epnum == 1) {
            if constexpr (kEnableHid) {
                const std::uint8_t index = g_state.hid.hid_out_active_index;
                g_state.hid.hid_out_len[index] = daplink::port::usb_ep_rx_count(hpcd, kHidEpOut);
                const bool was_empty = (g_state.hid.hid_out_full_mask == 0U);
                g_state.hid.hid_out_full_mask = static_cast<std::uint8_t>(g_state.hid.hid_out_full_mask | (1U << index));
                if (was_empty) {
                    g_state.hid.hid_out_read_index = index;
                }
                g_state.hid.hid_out_armed = false;
                hid_try_rearm(hpcd);
            }
        }
        if (epnum == (kCdcEpOut & 0x7FU)) {
            if constexpr (kEnableCdc) {
                const std::uint8_t index = g_state.cdc.cdc_out_active_index;
                g_state.cdc.cdc_out_len[index] = daplink::port::usb_ep_rx_count(hpcd, kCdcEpOut);
                const bool was_empty = (g_state.cdc.cdc_out_full_mask == 0U);
                g_state.cdc.cdc_out_full_mask = static_cast<std::uint8_t>(g_state.cdc.cdc_out_full_mask | (1U << index));
                if (was_empty) {
                    g_state.cdc.cdc_out_read_index = index;
                }
                g_state.cdc.cdc_out_armed = false;
                cdc_try_rearm(hpcd);
            }
        }
    }

    inline void on_data_in_stage(UsbPcdHandle& hpcd, std::uint8_t epnum) noexcept {
        if (epnum == 0) {
            if (g_state.ep0_in_active) {
                if (g_state.ep0_in_remaining > 0) {
                    const std::uint16_t send_len =
                        (g_state.ep0_in_remaining > kEp0Mps) ? kEp0Mps : g_state.ep0_in_remaining;
                    const std::uint8_t* send_ptr = g_state.ep0_in_ptr;
                    g_state.ep0_in_ptr = send_ptr + send_len;
                    g_state.ep0_in_remaining =
                        static_cast<std::uint16_t>(g_state.ep0_in_remaining - send_len);
                    if (g_state.ep0_in_remaining == 0 && g_state.ep0_in_need_zlp) {
                        g_state.ep0_in_need_zlp = false;
                        g_state.ep0_in_active = true;
                    } else if (g_state.ep0_in_remaining == 0) {
                        g_state.ep0_in_active = false;
                    }
                    (void)daplink::port::usb_ep_transmit(
                        hpcd, 0x80, const_cast<std::uint8_t*>(send_ptr), send_len);
                    return;
                }
                if (g_state.ep0_in_need_zlp) {
                    g_state.ep0_in_need_zlp = false;
                    g_state.ep0_in_active = false;
                    (void)daplink::port::usb_ep_transmit(hpcd, 0x80, g_state.ep0_in_zlp, 0);
                    return;
                }
                g_state.ep0_in_active = false;
            }
            if (g_state.address_pending) {
                g_state.address_pending = false;
                (void)daplink::port::usb_set_address(hpcd, g_state.pending_address);
            }
            (void)daplink::port::usb_ep_receive(hpcd, 0x00, g_state.ep0_out, kEp0Mps);
        }
        if (epnum == (kHidEpIn & 0x7FU)) {
            if constexpr (kEnableHid) {
                g_state.hid.hid_in_busy = false;
            }
        }
        if (epnum == (kCdcEpIn & 0x7FU)) {
            if constexpr (kEnableCdc) {
                g_state.cdc.cdc_in_busy = false;
            }
        }
    }

    inline bool out_ready() noexcept {
        if constexpr (!kEnableHid) {
            return false;
        }
        return g_state.hid.hid_out_full_mask != 0U;
    }

    inline std::span<const std::uint8_t, kHidPacketSize> out_packet() noexcept {
        if constexpr (!kEnableHid) {
            return std::span<const std::uint8_t, kHidPacketSize>(kEmptyHidPacket);
        }
        return std::span<const std::uint8_t, kHidPacketSize>(g_state.hid.hid_out[g_state.hid.hid_out_read_index]);
    }

    inline std::uint16_t out_length() noexcept {
        if constexpr (!kEnableHid) {
            return 0;
        }
        return g_state.hid.hid_out_len[g_state.hid.hid_out_read_index];
    }

    inline void consume_out() noexcept {
        if constexpr (!kEnableHid) {
            return;
        }
        const std::uint8_t index = g_state.hid.hid_out_read_index;
        g_state.hid.hid_out_full_mask = static_cast<std::uint8_t>(g_state.hid.hid_out_full_mask & ~(1U << index));
        if (g_state.hid.hid_out_full_mask != 0U) {
            g_state.hid.hid_out_read_index = static_cast<std::uint8_t>(index ^ 1U);
        }
        if (g_state.hpcd != nullptr) {
            hid_try_rearm(*g_state.hpcd);
        }
    }

    inline std::span<std::uint8_t, kHidPacketSize> in_packet() noexcept {
        if constexpr (!kEnableHid) {
            return std::span<std::uint8_t, kHidPacketSize>(kHidScratch);
        }
        return std::span<std::uint8_t, kHidPacketSize>(g_state.hid.hid_in);
    }

    inline bool hid_in_busy() noexcept {
        if constexpr (!kEnableHid) {
            return false;
        }
        return g_state.hid.hid_in_busy;
    }

    inline bool try_send_in_packet(const std::uint16_t len) noexcept {
        if constexpr (!kEnableHid) {
            (void)len;
            return false;
        }
        if (g_state.hpcd == nullptr) {
            return false;
        }
        return hid_in_send(*g_state.hpcd, len);
    }

    inline void send_in_packet(const std::uint16_t len) noexcept {
        (void)try_send_in_packet(len);
    }

    inline bool cdc_out_ready() noexcept {
        if constexpr (!kEnableCdc) {
            return false;
        }
        return g_state.cdc.cdc_out_full_mask != 0U;
    }

    inline std::span<const std::uint8_t> cdc_out_packet() noexcept {
        if constexpr (!kEnableCdc) {
            return {};
        }
        return std::span<const std::uint8_t>(g_state.cdc.cdc_out[g_state.cdc.cdc_out_read_index],
                                             g_state.cdc.cdc_out_len[g_state.cdc.cdc_out_read_index]);
    }

    inline std::uint16_t cdc_out_length() noexcept {
        if constexpr (!kEnableCdc) {
            return 0;
        }
        return g_state.cdc.cdc_out_len[g_state.cdc.cdc_out_read_index];
    }

    inline void cdc_consume_out() noexcept {
        if constexpr (!kEnableCdc) {
            return;
        }
        const std::uint8_t index = g_state.cdc.cdc_out_read_index;
        g_state.cdc.cdc_out_full_mask = static_cast<std::uint8_t>(g_state.cdc.cdc_out_full_mask & ~(1U << index));
        if (g_state.cdc.cdc_out_full_mask != 0U) {
            g_state.cdc.cdc_out_read_index = static_cast<std::uint8_t>(index ^ 1U);
        }
        if (g_state.hpcd != nullptr) {
            cdc_try_rearm(*g_state.hpcd);
        }
    }

    inline bool cdc_send_in(const std::uint8_t* data, const std::uint16_t len) noexcept {
        if constexpr (!kEnableCdc) {
            (void)data;
            (void)len;
            return false;
        }
        if (g_state.hpcd == nullptr) {
            return false;
        }
        return cdc_in_send(*g_state.hpcd, data, len);
    }

    inline cdc_line_config cdc_line() noexcept {
        if constexpr (!kEnableCdc) {
            return {};
        }
        return {
            g_state.cdc.cdc_line.dwDTERate,
            g_state.cdc.cdc_line.bCharFormat,
            g_state.cdc.cdc_line.bParityType,
            g_state.cdc.cdc_line.bDataBits
        };
    }

    inline void poll() noexcept {
        if constexpr (!kEnableCdc) {
            return;
        }
        if (!g_state.cdc.cdc_in_busy) {
            return;
        }
        const std::uint32_t now = daplink::port::tick_ms();
        if ((now - g_state.cdc.cdc_in_last_ms) > kCdcInTimeoutMs) {
            g_state.cdc.cdc_in_busy = false;
        }
    }

    inline bool take_reset() noexcept {
        if (!g_state.reset_pending) {
            return false;
        }
        g_state.reset_pending = false;
        return true;
    }
}

