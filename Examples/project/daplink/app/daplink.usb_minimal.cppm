module;

#include "usb.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <type_traits>
export module daplink.usb_minimal;

import daplink.app_config;

namespace daplink::usb_minimal::detail {
    using UsbProfile = daplink::app_config::UsbProfile;
    constexpr auto& kConfig = daplink::app_config::kConfig;
    constexpr UsbProfile kUsbProfile = kConfig.usb.profile;
    constexpr bool kEnableHid =
        (kUsbProfile == UsbProfile::hid) || (kUsbProfile == UsbProfile::composite);
    constexpr bool kEnableCdc =
        (kUsbProfile == UsbProfile::cdc) || (kUsbProfile == UsbProfile::composite);
    constexpr bool kCdcHasCmdEp = kEnableCdc && kConfig.usb.cdc_has_cmd_ep;

    constexpr std::uint8_t kEp0Mps = kConfig.usb.ep0_mps;
    constexpr std::uint8_t kHidEpOut = kConfig.usb.hid_ep_out;
    constexpr std::uint8_t kHidEpIn = kConfig.usb.hid_ep_in;
    constexpr std::uint16_t kHidEpMps = kConfig.usb.hid_ep_mps;
    constexpr std::size_t kHidPacketSize = kConfig.usb.hid_packet_size;
    constexpr std::uint8_t kCdcEpCmd = kConfig.usb.cdc_ep_cmd;
    constexpr std::uint8_t kCdcEpOut = kConfig.usb.cdc_ep_out;
    constexpr std::uint8_t kCdcEpIn = kConfig.usb.cdc_ep_in;
    constexpr std::uint16_t kCdcEpCmdMps = kConfig.usb.cdc_ep_cmd_mps;
    constexpr std::uint16_t kCdcEpMps = kConfig.usb.cdc_ep_mps;

    constexpr std::uint16_t kPmaEp0Out = 0x18;
    constexpr std::uint16_t kPmaEp0In = 0x58;
    constexpr std::uint16_t kPmaHidIn = 0x98;
    constexpr std::uint16_t kPmaHidOut = 0xD8;
    constexpr std::uint16_t kPmaCdcCmd = 0x118;
    constexpr std::uint16_t kPmaCdcOut = 0x120;
    constexpr std::uint16_t kPmaCdcIn = 0x160;
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
    constexpr std::uint8_t kCdcCommInterface = 0;
    constexpr std::uint8_t kCdcDataInterface = 1;
    constexpr std::uint8_t kHidInterface = kEnableCdc ? 2 : 0;

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

    struct empty_state {};

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
        [[no_unique_address]] std::conditional_t<kEnableHid, hid_state, empty_state> hid{};
        [[no_unique_address]] std::conditional_t<kEnableCdc, cdc_state, empty_state> cdc{};
        volatile bool reset_pending = false;
        PCD_HandleTypeDef* hpcd = nullptr;
    };

    inline usb_state g_state{};
    constexpr std::uint32_t kCdcInTimeoutMs = kConfig.cdc.in_timeout_ms;

    constexpr std::uint8_t kDeviceClass =
        (kUsbProfile == UsbProfile::composite) ? 0xEF : 0x00;
    constexpr std::uint8_t kDeviceSubClass =
        (kUsbProfile == UsbProfile::composite) ? 0x02 : 0x00;
    constexpr std::uint8_t kDeviceProtocol =
        (kUsbProfile == UsbProfile::composite) ? 0x01 : 0x00;

    constexpr std::uint8_t device_descriptor[] = {
        0x12, 0x01, 0x00, 0x02, kDeviceClass, kDeviceSubClass, kDeviceProtocol, kEp0Mps,
        static_cast<std::uint8_t>(kConfig.usb.vid & 0xFFU),
        static_cast<std::uint8_t>((kConfig.usb.vid >> 8) & 0xFFU),
        static_cast<std::uint8_t>(kConfig.usb.pid & 0xFFU),
        static_cast<std::uint8_t>((kConfig.usb.pid >> 8) & 0xFFU),
        0x00, 0x01, 0x01, 0x02,
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

    consteval auto make_configuration_descriptor() {
        if constexpr (kUsbProfile == UsbProfile::composite) {
            return std::array<std::uint8_t, 0x6B>{
        0x09, 0x02, 0x6B, 0x00, 0x03, 0x01, 0x00, 0x80, 0x32,
        0x08, 0x0B, 0x00, 0x02, 0x02, 0x02, 0x01, 0x00,
        0x09, 0x04, 0x00, 0x00, 0x01, 0x02, 0x02, 0x01, 0x00,
        0x05, 0x24, 0x00, 0x10, 0x01,
        0x05, 0x24, 0x01, 0x03, 0x01,
        0x04, 0x24, 0x02, 0x06,
        0x05, 0x24, 0x06, 0x00, 0x01,
        0x07, 0x05, kCdcEpCmd, 0x03, 0x08, 0x00, 0x10,
        0x09, 0x04, 0x01, 0x00, 0x02, 0x0A, 0x00, 0x00, 0x00,
        0x07, 0x05, kCdcEpOut, 0x02, 0x40, 0x00, 0x00,
        0x07, 0x05, kCdcEpIn, 0x02, 0x40, 0x00, 0x00,
        0x09, 0x04, 0x02, 0x00, 0x02, 0x03, 0x00, 0x00, 0x00,
        0x09, 0x21, 0x11, 0x01, 0x00, 0x01, 0x22, static_cast<std::uint8_t>(sizeof(hid_report_descriptor)), 0x00,
        0x07, 0x05, kHidEpIn, 0x03, 0x40, 0x00, 0x01,
        0x07, 0x05, kHidEpOut, 0x03, 0x40, 0x00, 0x01
            };
        } else if constexpr (kUsbProfile == UsbProfile::cdc) {
            return std::array<std::uint8_t, 0x4B>{
        0x09, 0x02, 0x4B, 0x00, 0x02, 0x01, 0x00, 0x80, 0x32,
        0x08, 0x0B, 0x00, 0x02, 0x02, 0x02, 0x01, 0x00,
        0x09, 0x04, 0x00, 0x00, 0x01, 0x02, 0x02, 0x01, 0x00,
        0x05, 0x24, 0x00, 0x10, 0x01,
        0x05, 0x24, 0x01, 0x03, 0x01,
        0x04, 0x24, 0x02, 0x06,
        0x05, 0x24, 0x06, 0x00, 0x01,
        0x07, 0x05, kCdcEpCmd, 0x03, 0x08, 0x00, 0x10,
        0x09, 0x04, 0x01, 0x00, 0x02, 0x0A, 0x00, 0x00, 0x00,
        0x07, 0x05, kCdcEpOut, 0x02, 0x40, 0x00, 0x00,
        0x07, 0x05, kCdcEpIn, 0x02, 0x40, 0x00, 0x00
            };
        } else {
            return std::array<std::uint8_t, 0x29>{
        0x09, 0x02, 0x29, 0x00, 0x01, 0x01, 0x00, 0x80, 0x32,
        0x09, 0x04, 0x00, 0x00, 0x02, 0x03, 0x00, 0x00, 0x00,
        0x09, 0x21, 0x11, 0x01, 0x00, 0x01, 0x22, static_cast<std::uint8_t>(sizeof(hid_report_descriptor)), 0x00,
        0x07, 0x05, kHidEpIn, 0x03, 0x40, 0x00, 0x01,
        0x07, 0x05, kHidEpOut, 0x03, 0x40, 0x00, 0x01
            };
        }
    }

    constexpr auto configuration_descriptor = make_configuration_descriptor();
    static_assert((kUsbProfile == UsbProfile::composite && configuration_descriptor.size() == 0x6B) ||
                  (kUsbProfile == UsbProfile::cdc && configuration_descriptor.size() == 0x4B) ||
                  (kUsbProfile == UsbProfile::hid && configuration_descriptor.size() == 0x29));

    constexpr std::uint8_t lang_id_descriptor[] = {0x04, 0x03, 0x09, 0x04};

    template <std::size_t N>
    consteval auto make_string_descriptor(const char (&text)[N]) {
        static_assert(N > 0);
        constexpr std::size_t kLen = 2 + (N - 1) * 2;
        std::array<std::uint8_t, kLen> desc{};
        desc[0] = static_cast<std::uint8_t>(kLen);
        desc[1] = 0x03;
        for (std::size_t i = 0; i < N - 1; ++i) {
            desc[2 + i * 2] = static_cast<std::uint8_t>(text[i]);
            desc[3 + i * 2] = 0;
        }
        return desc;
    }

    constexpr auto manufacturer_string = make_string_descriptor(daplink::app_config::kUsbManufacturer);
    constexpr auto product_string = make_string_descriptor(daplink::app_config::kUsbProduct);
    constexpr auto serial_string = make_string_descriptor(daplink::app_config::kUsbSerial);
    inline constexpr std::array<std::uint8_t, kHidPacketSize> kEmptyHidPacket = {};
    inline std::array<std::uint8_t, kHidPacketSize> kHidScratch = {};

    inline std::uint16_t min_u16(const std::uint16_t a, const std::uint16_t b) noexcept {
        return (a < b) ? a : b;
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

    inline void ep0_send(PCD_HandleTypeDef& hpcd,
                         const std::uint8_t* data,
                         std::uint16_t len,
                         std::uint16_t req_len) noexcept {
        if (len == 0) {
            g_state.ep0_in_ptr = nullptr;
            g_state.ep0_in_remaining = 0;
            g_state.ep0_in_req_len = req_len;
            g_state.ep0_in_active = false;
            g_state.ep0_in_need_zlp = false;
            (void)HAL_PCD_EP_Transmit(&hpcd, 0x80, g_state.ep0_in_zlp, 0);
            return;
        }

        const std::uint16_t send_len = (len > kEp0Mps) ? kEp0Mps : len;
        g_state.ep0_in_ptr = data + send_len;
        g_state.ep0_in_remaining = static_cast<std::uint16_t>(len - send_len);
        g_state.ep0_in_req_len = req_len;
        g_state.ep0_in_need_zlp = (g_state.ep0_in_remaining == 0) && (len < req_len) &&
                                  ((len % kEp0Mps) == 0);
        g_state.ep0_in_active = g_state.ep0_in_remaining > 0 || g_state.ep0_in_need_zlp;
        (void)HAL_PCD_EP_Transmit(&hpcd, 0x80, const_cast<std::uint8_t*>(data), send_len);
    }

    inline void ep0_status_in(PCD_HandleTypeDef& hpcd) noexcept {
        (void)HAL_PCD_EP_Transmit(&hpcd, 0x80, g_state.ep0_in_zlp, 0);
    }

    inline void ep0_prepare_out(PCD_HandleTypeDef& hpcd, std::uint16_t len, const ep0_out_kind kind) noexcept {
        g_state.pending_ep0_out = true;
        g_state.pending_ep0_kind = kind;
        g_state.pending_out_length = min_u16(len, kEp0Mps);
        (void)HAL_PCD_EP_Receive(&hpcd, 0x00, g_state.ep0_out, g_state.pending_out_length);
    }

    inline void hid_arm_out(PCD_HandleTypeDef& hpcd, const std::uint8_t index) noexcept {
        if constexpr (!kEnableHid) {
            (void)hpcd;
            (void)index;
        } else {
            g_state.hid.hid_out_active_index = index;
            g_state.hid.hid_out_armed = true;
            (void)HAL_PCD_EP_Receive(&hpcd, kHidEpOut, g_state.hid.hid_out[index], kHidEpMps);
        }
    }

    inline void hid_try_rearm(PCD_HandleTypeDef& hpcd) noexcept {
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

    inline bool hid_in_send(PCD_HandleTypeDef& hpcd, const std::uint16_t len) noexcept {
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
            (void)HAL_PCD_EP_Transmit(&hpcd, kHidEpIn, g_state.hid.hid_in, send_len);
            return true;
        }
    }

    inline void cdc_arm_out(PCD_HandleTypeDef& hpcd, const std::uint8_t index) noexcept {
        if constexpr (!kEnableCdc) {
            (void)hpcd;
            (void)index;
        } else {
            g_state.cdc.cdc_out_active_index = index;
            g_state.cdc.cdc_out_armed = true;
            (void)HAL_PCD_EP_Receive(&hpcd, kCdcEpOut, g_state.cdc.cdc_out[index], kCdcEpMps);
        }
    }

    inline void cdc_try_rearm(PCD_HandleTypeDef& hpcd) noexcept {
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

    inline bool cdc_in_send(PCD_HandleTypeDef& hpcd, const std::uint8_t* data, const std::uint16_t len) noexcept {
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
            g_state.cdc.cdc_in_last_ms = HAL_GetTick();
            (void)HAL_PCD_EP_Transmit(&hpcd, kCdcEpIn, g_state.cdc.cdc_in, send_len);
            return true;
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
                data = configuration_descriptor.data();
                len = static_cast<std::uint16_t>(configuration_descriptor.size());
                break;
            case kDescTypeString:
                if (desc_index == 0) {
                    data = lang_id_descriptor;
                    len = static_cast<std::uint16_t>(sizeof(lang_id_descriptor));
                } else if (desc_index == 1) {
                    data = manufacturer_string.data();
                    len = static_cast<std::uint16_t>(manufacturer_string.size());
                } else if (desc_index == 2) {
                    data = product_string.data();
                    len = static_cast<std::uint16_t>(product_string.size());
                } else if (desc_index == 3) {
                    data = serial_string.data();
                    len = static_cast<std::uint16_t>(serial_string.size());
                }
                break;
            case kDescTypeReport:
                if constexpr (!kEnableHid) {
                    ep0_stall(hpcd);
                    return;
                } else {
                    data = hid_report_descriptor;
                    len = static_cast<std::uint16_t>(sizeof(hid_report_descriptor));
                }
                break;
            case kDescTypeHid:
                if constexpr (!kEnableHid) {
                    ep0_stall(hpcd);
                    return;
                } else {
                    for (std::size_t offset = 0; offset + 1 < configuration_descriptor.size(); ) {
                        const std::uint8_t dlen = configuration_descriptor[offset];
                        const std::uint8_t dtype = configuration_descriptor[offset + 1];
                        if (dlen == 0) {
                            break;
                        }
                        if (dtype == kDescTypeHid) {
                            data = configuration_descriptor.data() + offset;
                            len = dlen;
                            break;
                        }
                        offset += dlen;
                    }
                }
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
        ep0_send(hpcd, data, len, s.w_length);
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

    inline bool attach(PCD_HandleTypeDef& hpcd) noexcept {
        g_state.hpcd = &hpcd;
        const auto r0 = HAL_PCDEx_PMAConfig(&hpcd, 0x00, PCD_SNG_BUF, kPmaEp0Out);
        const auto r1 = HAL_PCDEx_PMAConfig(&hpcd, 0x80, PCD_SNG_BUF, kPmaEp0In);
        const bool base_ok = (r0 == HAL_OK) && (r1 == HAL_OK);
        bool hid_ok = true;
        if constexpr (kEnableHid) {
            const auto r2 = HAL_PCDEx_PMAConfig(&hpcd, kHidEpIn, PCD_SNG_BUF, kPmaHidIn);
            const auto r3 = HAL_PCDEx_PMAConfig(&hpcd, kHidEpOut, PCD_SNG_BUF, kPmaHidOut);
            hid_ok = (r2 == HAL_OK) && (r3 == HAL_OK);
        }
        bool cdc_ok = true;
        if constexpr (kEnableCdc) {
            if constexpr (kCdcHasCmdEp) {
                const auto r4 = HAL_PCDEx_PMAConfig(&hpcd, kCdcEpCmd, PCD_SNG_BUF, kPmaCdcCmd);
                const auto r5 = HAL_PCDEx_PMAConfig(&hpcd, kCdcEpOut, PCD_SNG_BUF, kPmaCdcOut);
                const auto r6 = HAL_PCDEx_PMAConfig(&hpcd, kCdcEpIn, PCD_SNG_BUF, kPmaCdcIn);
                cdc_ok = (r4 == HAL_OK) && (r5 == HAL_OK) && (r6 == HAL_OK);
            } else {
                const auto r5 = HAL_PCDEx_PMAConfig(&hpcd, kCdcEpOut, PCD_SNG_BUF, kPmaCdcOut);
                const auto r6 = HAL_PCDEx_PMAConfig(&hpcd, kCdcEpIn, PCD_SNG_BUF, kPmaCdcIn);
                cdc_ok = (r5 == HAL_OK) && (r6 == HAL_OK);
            }
        }
        return base_ok && hid_ok && cdc_ok;
    }

    inline void on_reset(PCD_HandleTypeDef& hpcd) noexcept {
        g_state = {};
        g_state.hpcd = &hpcd;
        g_state.reset_pending = true;
        (void)HAL_PCD_SetAddress(&hpcd, 0);
        (void)HAL_PCD_EP_Open(&hpcd, 0x00, kEp0Mps, EP_TYPE_CTRL);
        (void)HAL_PCD_EP_Open(&hpcd, 0x80, kEp0Mps, EP_TYPE_CTRL);
        if constexpr (kEnableHid) {
            (void)HAL_PCD_EP_Open(&hpcd, kHidEpIn, kHidEpMps, EP_TYPE_INTR);
            (void)HAL_PCD_EP_Open(&hpcd, kHidEpOut, kHidEpMps, EP_TYPE_INTR);
        }
        if constexpr (kEnableCdc) {
            if constexpr (kCdcHasCmdEp) {
                (void)HAL_PCD_EP_Open(&hpcd, kCdcEpCmd, kCdcEpCmdMps, EP_TYPE_INTR);
            }
            (void)HAL_PCD_EP_Open(&hpcd, kCdcEpIn, kCdcEpMps, EP_TYPE_BULK);
            (void)HAL_PCD_EP_Open(&hpcd, kCdcEpOut, kCdcEpMps, EP_TYPE_BULK);
        }
        (void)HAL_PCD_EP_Receive(&hpcd, 0x00, g_state.ep0_out, kEp0Mps);
        if constexpr (kEnableHid) {
            hid_arm_out(hpcd, 0);
        }
        if constexpr (kEnableCdc) {
            cdc_arm_out(hpcd, 0);
        }
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

    inline void on_data_out_stage(PCD_HandleTypeDef& hpcd, std::uint8_t epnum) noexcept {
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
                g_state.hid.hid_out_len[index] = static_cast<std::uint16_t>(HAL_PCD_EP_GetRxCount(&hpcd, kHidEpOut));
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
                g_state.cdc.cdc_out_len[index] = static_cast<std::uint16_t>(HAL_PCD_EP_GetRxCount(&hpcd, kCdcEpOut));
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

    inline void on_data_in_stage(PCD_HandleTypeDef& hpcd, std::uint8_t epnum) noexcept {
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
                    (void)HAL_PCD_EP_Transmit(&hpcd, 0x80,
                                              const_cast<std::uint8_t*>(send_ptr), send_len);
                    return;
                }
                if (g_state.ep0_in_need_zlp) {
                    g_state.ep0_in_need_zlp = false;
                    g_state.ep0_in_active = false;
                    (void)HAL_PCD_EP_Transmit(&hpcd, 0x80, g_state.ep0_in_zlp, 0);
                    return;
                }
                g_state.ep0_in_active = false;
            }
            if (g_state.address_pending) {
                g_state.address_pending = false;
                (void)HAL_PCD_SetAddress(&hpcd, g_state.pending_address);
            }
            (void)HAL_PCD_EP_Receive(&hpcd, 0x00, g_state.ep0_out, kEp0Mps);
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
        const std::uint32_t now = HAL_GetTick();
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

