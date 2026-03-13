module;
#include <cstddef>
#include <cstdint>
#include <span>
export module daplink.cmsis_dap;


import daplink.swd_engine;

export namespace daplink::cmsis_dap {
    constexpr std::size_t kPacketSize = 64;
    constexpr std::uint8_t kPacketCount = 4;

    struct InfoField {
        const char* data;
        std::uint8_t size;
    };

    struct DeviceInfo {
        InfoField vendor;
        InfoField product;
        InfoField serial;
        InfoField fw_version;
    };

    template <std::size_t N>
    constexpr InfoField make_info_field(const char (&text)[N]) noexcept {
        static_assert(N > 0);
        static_assert((N - 1) <= 255);
        return {text, static_cast<std::uint8_t>(N - 1)};
    }

    struct State {
        std::uint8_t dap_port = 0;
        swd::Config swd_cfg{};
        std::uint16_t match_retry = 0;
        std::uint32_t match_mask = 0;
    };

    namespace detail {
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
        constexpr std::uint8_t kCmsisDapQueueCommands = 0x7E;
        constexpr std::uint8_t kCmsisDapExecuteCommands = 0x7F;
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
        constexpr std::uint8_t kDapTransferMismatch = 0x10;
        constexpr std::uint8_t kReqApndp = 1U << 0;
        constexpr std::uint8_t kReqRnw = 1U << 1;
        constexpr std::uint8_t kReqMatchValue = 1U << 4;
        constexpr std::uint8_t kReqMatchMask = 1U << 5;
        constexpr std::uint8_t kReqDpRdbuff = kReqRnw | (1U << 2) | (1U << 3);

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

        template <swd::Backend Backend>
        inline std::uint8_t dap_transfer_once(const State& state, const std::uint8_t request, std::uint32_t& data) noexcept {
            if ((request & kReqRnw) != 0U && (request & kReqApndp) != 0U) {
                std::uint32_t posted_dummy = 0;
                auto ack = swd::Engine<Backend>::transfer(state.swd_cfg, request, posted_dummy);
                if (ack != kDapTransferOk) {
                    return ack;
                }
                return swd::Engine<Backend>::transfer(state.swd_cfg, kReqDpRdbuff, data);
            }
            return swd::Engine<Backend>::transfer(state.swd_cfg, request, data);
        }

        template <swd::Backend Backend>
        inline void connect_swd(State& state) noexcept {
            Backend::setup_swd_pins_active();
            swd::Engine<Backend>::line_reset();
            constexpr std::uint8_t seq[] = {0x9E, 0xE7};
            swd::Engine<Backend>::swj_sequence(seq, 16);
            swd::Engine<Backend>::line_reset();
            state.dap_port = kDapPortSwd;
        }

        template <swd::Backend Backend>
        inline void disconnect_swd(State& state) noexcept {
            state.dap_port = kDapPortDisabled;
            Backend::setup_swd_pins_hi_z();
        }

        struct CmdResult {
            std::uint16_t in_used;
            std::uint16_t out_used;
            bool ok;
        };

        template <swd::Backend Backend>
        inline CmdResult process_single(
            State& state,
            DeviceInfo info,
            std::span<const std::uint8_t> in,
            std::span<std::uint8_t> out
        ) noexcept {
            if (in.empty() || out.empty()) {
                return {0, 0, false};
            }

            const auto in_size = static_cast<std::uint16_t>(in.size());
            const auto out_size = static_cast<std::uint16_t>(out.size());
            const std::uint8_t cmd = in[0];
            out[0] = cmd;

            switch (cmd) {
                case kCmsisDapInfo: {
                    if (in_size < 2 || out_size < 2) {
                        out[0] = kCmsisDapInvalid;
                        return {1, 1, false};
                    }
                    std::uint8_t len = 0;
                    switch (in[1]) {
                        case kDapInfoVendor:
                            len = fill_info(info.vendor, &out[2]);
                            break;
                        case kDapInfoProduct:
                            len = fill_info(info.product, &out[2]);
                            break;
                        case kDapInfoSerial:
                            len = fill_info(info.serial, &out[2]);
                            break;
                        case kDapInfoFwVersion:
                            len = fill_info(info.fw_version, &out[2]);
                            break;
                        case kDapInfoCapabilities:
                            out[2] = 0x11;
                            len = 1;
                            break;
                        case kDapInfoPacketCount:
                            out[2] = kPacketCount;
                            len = 1;
                            break;
                        case kDapInfoPacketSize:
                            out[2] = static_cast<std::uint8_t>(kPacketSize & 0xFFU);
                            out[3] = static_cast<std::uint8_t>((kPacketSize >> 8) & 0xFFU);
                            len = 2;
                            break;
                        default:
                            len = 0;
                            break;
                    }
                    if (out_size < static_cast<std::uint16_t>(2U + len)) {
                        out[0] = kCmsisDapInvalid;
                        return {1, 1, false};
                    }
                    out[1] = len;
                    return {2, static_cast<std::uint16_t>(2U + len), true};
                }
                case kCmsisDapHostStatus:
                    if (in_size < 3 || out_size < 2) {
                        out[0] = kCmsisDapInvalid;
                        return {1, 1, false};
                    }
                    if (in[1] == 0) {
                        if constexpr (requires { Backend::set_connected_led(true); }) {
                            Backend::set_connected_led(in[2] != 0U);
                        }
                    } else if (in[1] == 1) {
                        if constexpr (requires { Backend::set_running_led(true); }) {
                            Backend::set_running_led(in[2] != 0U);
                        }
                    }
                    out[1] = kDapOk;
                    return {3, 2, true};
                case kCmsisDapConnect:
                    if (in_size < 2 || out_size < 2) {
                        out[0] = kCmsisDapInvalid;
                        return {1, 1, false};
                    }
                    if (in[1] == 0 || in[1] == kDapPortSwd) {
                        connect_swd<Backend>(state);
                        out[1] = kDapPortSwd;
                    } else {
                        out[1] = kDapPortDisabled;
                    }
                    return {2, 2, true};
                case kCmsisDapDisconnect:
                    if (out_size < 2) {
                        out[0] = kCmsisDapInvalid;
                        return {1, 1, false};
                    }
                    disconnect_swd<Backend>(state);
                    out[1] = kDapOk;
                    return {1, 2, true};
                case kCmsisDapTransferConfigure:
                    if (in_size < 6 || out_size < 2) {
                        out[0] = kCmsisDapInvalid;
                        return {1, 1, false};
                    }
                    state.swd_cfg.idle_cycles = in[1];
                    state.swd_cfg.retry_count = static_cast<std::uint16_t>(in[2] | (in[3] << 8));
                    state.match_retry = static_cast<std::uint16_t>(in[4] | (in[5] << 8));
                    out[1] = kDapOk;
                    return {6, 2, true};
                case kCmsisDapWriteAbort:
                case kCmsisDapDelay:
                    if ((cmd == kCmsisDapWriteAbort && in_size < 5) || (cmd == kCmsisDapDelay && in_size < 3) || out_size < 2) {
                        out[0] = kCmsisDapInvalid;
                        return {1, 1, false};
                    }
                    out[1] = kDapOk;
                    return {static_cast<std::uint16_t>(cmd == kCmsisDapWriteAbort ? 5 : 3), 2, true};
                case kCmsisDapResetTarget: {
                    if (out_size < 2) {
                        out[0] = kCmsisDapInvalid;
                        return {1, 1, false};
                    }
                    std::uint8_t done = 0;
                    if constexpr (requires { Backend::reset_target(); }) {
                        done = static_cast<std::uint8_t>(Backend::reset_target());
                    }
                    out[1] = done;
                    return {1, 2, true};
                }
                case kCmsisDapSwjClock: {
                    if (in_size < 5 || out_size < 2) {
                        out[0] = kCmsisDapInvalid;
                        return {1, 1, false};
                    }
                    const auto hz = read_le32(&in[1]);
                    Backend::set_swj_clock_hz(hz);
                    out[1] = kDapOk;
                    return {5, 2, true};
                }
                case kCmsisDapSwjPins:
                    if (in_size < 3 || out_size < 2) {
                        out[0] = kCmsisDapInvalid;
                        return {1, 1, false};
                    }
                    out[1] = Backend::swj_pins(in[1], in[2]);
                    return {3, 2, true};
                case kCmsisDapSwjSequence: {
                    if (in_size < 2 || out_size < 2) {
                        out[0] = kCmsisDapInvalid;
                        return {1, 1, false};
                    }
                    std::uint32_t bits = in[1];
                    if (bits == 0) {
                        bits = 256;
                    }
                    const std::uint16_t bytes = static_cast<std::uint16_t>((bits + 7U) / 8U);
                    const std::uint16_t needed = static_cast<std::uint16_t>(2U + bytes);
                    if (in_size < needed) {
                        out[0] = kCmsisDapInvalid;
                        return {1, 1, false};
                    }
                    swd::Engine<Backend>::swj_sequence(&in[2], bits);
                    out[1] = kDapOk;
                    return {needed, 2, true};
                }
                case kCmsisDapSwdConfigure:
                    if (in_size < 2 || out_size < 2) {
                        out[0] = kCmsisDapInvalid;
                        return {1, 1, false};
                    }
                    state.swd_cfg.turnaround = static_cast<std::uint8_t>((in[1] & 0x3U) + 1U);
                    state.swd_cfg.data_phase = (in[1] & 0x4U) != 0U;
                    out[1] = kDapOk;
                    return {2, 2, true};
                case kCmsisDapTransfer: {
                    if (in_size < 3 || out_size < 3) {
                        out[0] = kCmsisDapInvalid;
                        return {1, 1, false};
                    }
                    std::uint8_t response_count = 0;
                    std::uint8_t response_value = kDapTransferOk;
                    std::uint16_t in_idx = 3;
                    std::uint16_t out_idx = 3;
                    const auto transfer_count = in[2];

                    if (state.dap_port != kDapPortSwd) {
                        response_value = kDapTransferError;
                    } else {
                        for (std::uint8_t i = 0; i < transfer_count; ++i) {
                            if (in_idx >= in_size) {
                                response_value = kDapTransferError;
                                break;
                            }
                            const auto request = in[in_idx++];
                            std::uint32_t data = 0;
                            const bool is_read = (request & kReqRnw) != 0U;
                            const bool is_match_value = (request & kReqMatchValue) != 0U;
                            const bool is_match_mask = (request & kReqMatchMask) != 0U;

                            if (!is_read || is_match_value || is_match_mask) {
                                if ((in_idx + 3U) >= in_size) {
                                    response_value = kDapTransferError;
                                    break;
                                }
                                data = read_le32(&in[in_idx]);
                                in_idx = static_cast<std::uint16_t>(in_idx + 4U);
                            }

                            if (is_match_mask) {
                                state.match_mask = data;
                                response_count = static_cast<std::uint8_t>(response_count + 1);
                                continue;
                            }

                            if (is_match_value) {
                                std::uint32_t sampled = 0;
                                std::uint8_t ack = kDapTransferError;
                                std::uint16_t retries = state.match_retry;
                                while (true) {
                                    ack = dap_transfer_once<Backend>(state, request, sampled);
                                    if (ack != kDapTransferOk) {
                                        break;
                                    }
                                    if ((sampled & state.match_mask) == data) {
                                        break;
                                    }
                                    if (retries == 0U) {
                                        ack = static_cast<std::uint8_t>(kDapTransferOk | kDapTransferMismatch);
                                        break;
                                    }
                                    --retries;
                                }
                                if (ack != kDapTransferOk) {
                                    response_value = ack;
                                    break;
                                }
                                response_count = static_cast<std::uint8_t>(response_count + 1);
                                continue;
                            }

                            const auto ack = dap_transfer_once<Backend>(state, request, data);
                            if (ack != kDapTransferOk) {
                                response_value = ack;
                                break;
                            }

                            response_count = static_cast<std::uint8_t>(response_count + 1);
                            if (is_read) {
                                if ((out_idx + 3U) >= out_size) {
                                    response_value = kDapTransferError;
                                    break;
                                }
                                write_le32(&out[out_idx], data);
                                out_idx = static_cast<std::uint16_t>(out_idx + 4U);
                            }
                        }
                    }

                    out[1] = response_count;
                    out[2] = response_value;
                    return {in_idx, out_idx, response_value == kDapTransferOk};
                }
                case kCmsisDapTransferBlock: {
                    if (in_size < 4 || out_size < 4) {
                        out[0] = kCmsisDapInvalid;
                        return {1, 1, false};
                    }
                    std::uint16_t response_count = 0;
                    std::uint8_t response_value = kDapTransferOk;
                    std::uint16_t in_idx = 4;
                    std::uint16_t out_idx = 4;
                    const auto transfer_count = static_cast<std::uint16_t>(in[1] | (in[2] << 8));
                    const auto request = in[3];
                    const bool is_read = (request & kReqRnw) != 0U;
                    const bool is_ap = (request & kReqApndp) != 0U;
                    const std::uint16_t max_read_words = static_cast<std::uint16_t>((out_size - 4U) / 4U);
                    const std::uint16_t max_write_words = static_cast<std::uint16_t>((in_size > in_idx) ? ((in_size - in_idx) / 4U) : 0U);
                    std::uint16_t max_words = is_read ? max_read_words : max_write_words;
                    std::uint16_t exec_words = transfer_count;

                    if (state.dap_port != kDapPortSwd) {
                        response_value = kDapTransferError;
                    } else if ((request & (kReqMatchValue | kReqMatchMask)) != 0U) {
                        response_value = kDapTransferError;
                    } else {
                        if (exec_words > max_words) {
                            exec_words = max_words;
                            response_value = kDapTransferError;
                        }

                        for (std::uint16_t i = 0; i < exec_words; ++i) {
                            std::uint32_t data = 0;
                            if (!is_read) {
                                if ((in_idx + 3U) >= in_size) {
                                    response_value = kDapTransferError;
                                    break;
                                }
                                data = read_le32(&in[in_idx]);
                                in_idx = static_cast<std::uint16_t>(in_idx + 4U);
                            }

                            const auto ack = dap_transfer_once<Backend>(state, request, data);
                            if (ack != kDapTransferOk) {
                                response_value = ack;
                                break;
                            }

                            ++response_count;
                            if (is_read) {
                                if ((out_idx + 3U) >= out_size) {
                                    response_value = kDapTransferError;
                                    break;
                                }
                                write_le32(&out[out_idx], data);
                                out_idx = static_cast<std::uint16_t>(out_idx + 4U);
                            }
                        }

                        if (is_ap && is_read && response_count != exec_words && response_value == kDapTransferOk) {
                            response_value = kDapTransferError;
                        }
                    }

                    out[1] = static_cast<std::uint8_t>(response_count & 0xFFU);
                    out[2] = static_cast<std::uint8_t>((response_count >> 8) & 0xFFU);
                    out[3] = response_value;
                    return {in_idx, out_idx, response_value == kDapTransferOk};
                }
                default:
                    out[0] = kCmsisDapInvalid;
                    return {1, 1, false};
            }
        }

        template <swd::Backend Backend>
        inline void process_execute(
            State& state,
            DeviceInfo info,
            std::span<const std::uint8_t, kPacketSize> in,
            std::span<std::uint8_t, kPacketSize> out,
            std::uint8_t response_cmd
        ) noexcept {
            if (in.size() < 2) {
                out[0] = kCmsisDapInvalid;
                return;
            }
            const std::uint8_t count = in[1];
            out[0] = response_cmd;
            std::uint16_t in_idx = 2;
            std::uint16_t out_idx = 2;
            std::uint8_t executed = 0;

            while (executed < count) {
                if (in_idx >= kPacketSize || out_idx >= kPacketSize) {
                    break;
                }
                auto res = process_single<Backend>(state, info, in.subspan(in_idx), out.subspan(out_idx));
                if (res.in_used == 0 || res.out_used == 0) {
                    break;
                }
                in_idx = static_cast<std::uint16_t>(in_idx + res.in_used);
                out_idx = static_cast<std::uint16_t>(out_idx + res.out_used);
                ++executed;
            }

            out[1] = executed;
        }
    }

    template <swd::Backend Backend>
    inline void process_packet(
        State& state,
        DeviceInfo info,
        std::span<const std::uint8_t, kPacketSize> in,
        std::span<std::uint8_t, kPacketSize> out
    ) noexcept {
        for (std::size_t i = 0; i < kPacketSize; ++i) {
            out[i] = 0;
        }

        const std::uint8_t cmd = in[0];
        if (cmd == detail::kCmsisDapQueueCommands) {
            detail::process_execute<Backend>(state, info, in, out, detail::kCmsisDapExecuteCommands);
            return;
        }
        if (cmd == detail::kCmsisDapExecuteCommands) {
            detail::process_execute<Backend>(state, info, in, out, cmd);
            return;
        }

        detail::process_single<Backend>(state, info, in, out);
    }
}
