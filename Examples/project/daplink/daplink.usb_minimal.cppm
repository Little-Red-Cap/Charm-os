module;

#include "usb.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
export module daplink.usb_minimal;

namespace daplink::usb_minimal::detail {
    constexpr std::uint8_t kEp0Mps = 64;
    constexpr std::uint8_t kHidEpOut = 0x01;
    constexpr std::uint8_t kHidEpIn = 0x81;
    constexpr std::uint16_t kHidEpMps = 64;
    constexpr std::size_t kHidPacketSize = 64;

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
        std::uint8_t pending_address = 0;
        bool address_pending = false;
        bool pending_ep0_out = false;
        std::uint16_t pending_out_length = 0;
        std::uint8_t ep0_out[kEp0Mps] = {};
        std::uint8_t ep0_in_zlp[1] = {};
        std::uint8_t ep0_in_data[2] = {};
        std::uint8_t hid_out[2][kHidPacketSize] = {};
        std::uint16_t hid_out_len[2] = {};
        std::uint8_t hid_in[kHidPacketSize] = {};
        volatile std::uint8_t hid_out_full_mask = 0;
        volatile std::uint8_t hid_out_active_index = 0;
        volatile std::uint8_t hid_out_read_index = 0;
        volatile bool hid_out_armed = false;
        volatile bool reset_pending = false;
        PCD_HandleTypeDef* hpcd = nullptr;
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

    constexpr auto manufacturer_string = make_string_descriptor("Charm");
    constexpr auto product_string = make_string_descriptor("Charm CMSIS-DAP");
    constexpr auto serial_string = make_string_descriptor("0001");

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

    inline void hid_arm_out(PCD_HandleTypeDef& hpcd, const std::uint8_t index) noexcept {
        g_state.hid_out_active_index = index;
        g_state.hid_out_armed = true;
        (void)HAL_PCD_EP_Receive(&hpcd, kHidEpOut, g_state.hid_out[index], kHidEpMps);
    }

    inline void hid_try_rearm(PCD_HandleTypeDef& hpcd) noexcept {
        if (g_state.hid_out_armed) {
            return;
        }
        if ((g_state.hid_out_full_mask & 0x3U) == 0x3U) {
            return;
        }
        const std::uint8_t free_index = (g_state.hid_out_full_mask & 0x1U) ? 1U : 0U;
        hid_arm_out(hpcd, free_index);
    }

    inline void hid_in_send(PCD_HandleTypeDef& hpcd, const std::uint16_t len) noexcept {
        const std::uint16_t send_len = (len > kHidEpMps) ? kHidEpMps : len;
        (void)HAL_PCD_EP_Transmit(&hpcd, kHidEpIn, g_state.hid_in, send_len);
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

    constexpr std::size_t hid_packet_size = kHidPacketSize;

    inline bool attach(PCD_HandleTypeDef& hpcd) noexcept {
        g_state.hpcd = &hpcd;
        const auto r0 = HAL_PCDEx_PMAConfig(&hpcd, 0x00, PCD_SNG_BUF, 0x18);
        const auto r1 = HAL_PCDEx_PMAConfig(&hpcd, 0x80, PCD_SNG_BUF, 0x58);
        const auto r2 = HAL_PCDEx_PMAConfig(&hpcd, kHidEpIn, PCD_SNG_BUF, 0xC0);
        const auto r3 = HAL_PCDEx_PMAConfig(&hpcd, kHidEpOut, PCD_SNG_BUF, 0x100);
        return (r0 == HAL_OK) && (r1 == HAL_OK) && (r2 == HAL_OK) && (r3 == HAL_OK);
    }

    inline void on_reset(PCD_HandleTypeDef& hpcd) noexcept {
        g_state = {};
        g_state.hpcd = &hpcd;
        g_state.reset_pending = true;
        (void)HAL_PCD_SetAddress(&hpcd, 0);
        (void)HAL_PCD_EP_Open(&hpcd, 0x00, kEp0Mps, EP_TYPE_CTRL);
        (void)HAL_PCD_EP_Open(&hpcd, 0x80, kEp0Mps, EP_TYPE_CTRL);
        (void)HAL_PCD_EP_Open(&hpcd, kHidEpIn, kHidEpMps, EP_TYPE_INTR);
        (void)HAL_PCD_EP_Open(&hpcd, kHidEpOut, kHidEpMps, EP_TYPE_INTR);
        (void)HAL_PCD_EP_Receive(&hpcd, 0x00, g_state.ep0_out, kEp0Mps);
        hid_arm_out(hpcd, 0);
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
                        if (s.w_length > kEp0Mps) {
                            ep0_stall(hpcd);
                            return;
                        }
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
            const std::uint8_t index = g_state.hid_out_active_index;
            g_state.hid_out_len[index] = static_cast<std::uint16_t>(HAL_PCD_EP_GetRxCount(&hpcd, kHidEpOut));
            const bool was_empty = (g_state.hid_out_full_mask == 0U);
            g_state.hid_out_full_mask = static_cast<std::uint8_t>(g_state.hid_out_full_mask | (1U << index));
            if (was_empty) {
                g_state.hid_out_read_index = index;
            }
            g_state.hid_out_armed = false;
            hid_try_rearm(hpcd);
        }
    }

    inline void on_data_in_stage(PCD_HandleTypeDef& hpcd, std::uint8_t epnum) noexcept {
        if (epnum == 0) {
            if (g_state.address_pending) {
                g_state.address_pending = false;
                (void)HAL_PCD_SetAddress(&hpcd, g_state.pending_address);
            }
            (void)HAL_PCD_EP_Receive(&hpcd, 0x00, g_state.ep0_out, kEp0Mps);
        }
    }

    inline bool out_ready() noexcept {
        return g_state.hid_out_full_mask != 0U;
    }

    inline std::span<const std::uint8_t, kHidPacketSize> out_packet() noexcept {
        return std::span<const std::uint8_t, kHidPacketSize>(g_state.hid_out[g_state.hid_out_read_index]);
    }

    inline std::uint16_t out_length() noexcept {
        return g_state.hid_out_len[g_state.hid_out_read_index];
    }

    inline void consume_out() noexcept {
        const std::uint8_t index = g_state.hid_out_read_index;
        g_state.hid_out_full_mask = static_cast<std::uint8_t>(g_state.hid_out_full_mask & ~(1U << index));
        if (g_state.hid_out_full_mask != 0U) {
            g_state.hid_out_read_index = static_cast<std::uint8_t>(index ^ 1U);
        }
        if (g_state.hpcd != nullptr) {
            hid_try_rearm(*g_state.hpcd);
        }
    }

    inline std::span<std::uint8_t, kHidPacketSize> in_packet() noexcept {
        return std::span<std::uint8_t, kHidPacketSize>(g_state.hid_in);
    }

    inline void send_in_packet(const std::uint16_t len) noexcept {
        if (g_state.hpcd == nullptr) {
            return;
        }
        hid_in_send(*g_state.hpcd, len);
    }

    inline bool take_reset() noexcept {
        if (!g_state.reset_pending) {
            return false;
        }
        g_state.reset_pending = false;
        return true;
    }
}
