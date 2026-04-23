module;

#include <cstddef>
#include <cstdint>
#include <span>
#ifndef CHARM_DAP_ENABLE_SWO
#define CHARM_DAP_ENABLE_SWO 0
#endif
#ifndef CHARM_DAP_ENABLE_SWO_STREAM
#define CHARM_DAP_ENABLE_SWO_STREAM 0
#endif
#ifndef CHARM_DAP_ENABLE_DAP_UART
#define CHARM_DAP_ENABLE_DAP_UART 0
#endif

export module daplink.cmsis_dap;

export import :core;
export import :protocol;
export import :state;
import daplink.dap_strategy;
import daplink.dap_ops;
import daplink.swd_engine;
import daplink.dap_backend;

export namespace daplink::cmsis_dap {
    namespace detail {
        template <daplink::dap_backend::SwdBackend Backend>
        inline std::uint8_t dap_transfer_once(const State& state, const std::uint8_t request, std::uint32_t& data) noexcept {
            if ((request & kReqRnw) != 0U && (request & kReqApndp) != 0U) {
                std::uint32_t posted_dummy = 0;
                auto ack = swd::Engine<Backend>::transfer(state.config.swd, request, posted_dummy);
                if (ack != kDapTransferOk) {
                    return ack;
                }
                return swd::Engine<Backend>::transfer(state.config.swd, kReqDpRdbuff, data);
            }
            return swd::Engine<Backend>::transfer(state.config.swd, request, data);
        }

        template <daplink::dap_backend::SwdBackend Backend, typename Ops>
        inline void connect_swd(State& state) noexcept {
            Ops::setup_swd_pins_active();
            if (state.config.current_hz != 0U) {
                Ops::set_swj_clock_hz(state.config.current_hz);
            }
            swd::Engine<Backend>::line_reset();
            constexpr std::uint8_t seq[] = {0x9E, 0xE7};
            swd::Engine<Backend>::swj_sequence(seq, 16);
            swd::Engine<Backend>::line_reset();
            state.runtime.dap_port = kDapPortSwd;
            state.runtime.error_streak = 0;
        }

        template <daplink::dap_backend::SwdBackend Backend, typename Ops>
        inline void disconnect_swd(State& state) noexcept {
            state.runtime.dap_port = kDapPortDisabled;
            Ops::setup_swd_pins_hi_z();
        }

        struct CmdResult {
            std::uint16_t in_used;
            std::uint16_t out_used;
            bool ok;
        };

        template <daplink::dap_backend::SwdBackend Backend, typename Ops, typename Policy>
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
                        case kDapInfoProtocolVersion:
                            len = fill_info(info.protocol_version, &out[2]);
                            break;
                        case kDapInfoProductFwVersion:
                            len = fill_info(info.product_fw_version, &out[2]);
                            break;
                        case kDapInfoCapabilities:
                            out[2] = kCapabilities;
                            len = 1;
                            break;
                        case kDapInfoUartRxBufferSize:
#if CHARM_DAP_ENABLE_DAP_UART
                            write_le32(&out[2], kUartBufferSize);
                            len = 4;
#else
                            len = 0;
#endif
                            break;
                        case kDapInfoUartTxBufferSize:
#if CHARM_DAP_ENABLE_DAP_UART
                            write_le32(&out[2], kUartBufferSize);
                            len = 4;
#else
                            len = 0;
#endif
                            break;
                        case kDapInfoSwoBufferSize:
#if CHARM_DAP_ENABLE_SWO
                            write_le32(&out[2], kSwoBufferSize);
                            len = 4;
#else
                            len = 0;
#endif
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
                        Ops::set_connected_led(in[2] != 0U);
                    } else if (in[1] == 1) {
                        Ops::set_running_led(in[2] != 0U);
                    }
                    out[1] = kDapOk;
                    return {3, 2, true};
                case kCmsisDapConnect:
                    if (in_size < 2 || out_size < 2) {
                        out[0] = kCmsisDapInvalid;
                        return {1, 1, false};
                    }
                    if (in[1] == 0 || in[1] == kDapPortSwd) {
                        connect_swd<Backend, Ops>(state);
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
                    disconnect_swd<Backend, Ops>(state);
                    out[1] = kDapOk;
                    return {1, 2, true};
                case kCmsisDapTransferConfigure:
                    if (in_size < 6 || out_size < 2) {
                        out[0] = kCmsisDapInvalid;
                        return {1, 1, false};
                    }
                    state.config.swd.idle_cycles = in[1];
                    state.config.swd.retry_count = static_cast<std::uint16_t>(in[2] | (in[3] << 8));
                    state.config.match_retry = static_cast<std::uint16_t>(in[4] | (in[5] << 8));
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
                    const std::uint8_t done = static_cast<std::uint8_t>(Ops::reset_target());
                    out[1] = done;
                    return {1, 2, true};
                }
                case kCmsisDapSwjClock: {
                    if (in_size < 5 || out_size < 2) {
                        out[0] = kCmsisDapInvalid;
                        return {1, 1, false};
                    }
                    const auto hz = read_le32(&in[1]);
                    state.config.current_hz = hz;
                    Ops::set_swj_clock_hz(hz);
                    out[1] = kDapOk;
                    return {5, 2, true};
                }
                case kCmsisDapSwjPins:
                    if (in_size < 3 || out_size < 2) {
                        out[0] = kCmsisDapInvalid;
                        return {1, 1, false};
                    }
                    out[1] = Ops::swj_pins(in[1], in[2]);
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
                    state.config.swd.turnaround = static_cast<std::uint8_t>((in[1] & 0x3U) + 1U);
                    state.config.swd.data_phase = (in[1] & 0x4U) != 0U;
                    out[1] = kDapOk;
                    return {2, 2, true};
#if CHARM_DAP_ENABLE_SWO
                case kCmsisDapSwoTransport: {
                    if (in_size < 2 || out_size < 2) {
                        out[0] = kCmsisDapInvalid;
                        return {1, 1, false};
                    }
                    const auto transport = in[1];
                    bool ok = false;
                    if ((state.runtime.swo_status & kSwoCaptureActive) == 0U) {
                        if (transport == 0U || transport == 1U) {
                            state.runtime.swo_transport = transport;
                            ok = true;
                        } else if (CHARM_DAP_ENABLE_SWO_STREAM && transport == 2U) {
                            state.runtime.swo_transport = transport;
                            ok = true;
                        }
                    }
                    out[1] = ok ? kDapOk : kDapError;
                    return {2, 2, ok};
                }
                case kCmsisDapSwoMode: {
                    if (in_size < 2 || out_size < 2) {
                        out[0] = kCmsisDapInvalid;
                        return {1, 1, false};
                    }
                    const auto mode = in[1];
                    bool ok = false;
                    if (mode == kSwoModeOff) {
                        state.runtime.swo_mode = kSwoModeOff;
                        state.runtime.swo_status = 0;
                        ok = true;
                    } else if (mode == kSwoModeUart) {
                        state.runtime.swo_mode = kSwoModeUart;
                        ok = true;
                    }
                    out[1] = ok ? kDapOk : kDapError;
                    return {2, 2, ok};
                }
                case kCmsisDapSwoBaudrate: {
                    if (in_size < 5 || out_size < 5) {
                        out[0] = kCmsisDapInvalid;
                        return {1, 1, false};
                    }
                    const auto baud = read_le32(&in[1]);
                    if (state.runtime.swo_mode == kSwoModeUart && baud != 0U) {
                        state.runtime.swo_baudrate = baud;
                        write_le32(&out[1], baud);
                        return {5, 5, true};
                    }
                    write_le32(&out[1], 0U);
                    return {5, 5, false};
                }
                case kCmsisDapSwoControl: {
                    if (in_size < 2 || out_size < 2) {
                        out[0] = kCmsisDapInvalid;
                        return {1, 1, false};
                    }
                    const bool active = (in[1] & kSwoCaptureActive) != 0U;
                    bool ok = false;
                    if (state.runtime.swo_mode != kSwoModeOff) {
                        if (active) {
                            state.runtime.swo_status =
                                static_cast<std::uint8_t>(state.runtime.swo_status | kSwoCaptureActive);
                        } else {
                            state.runtime.swo_status =
                                static_cast<std::uint8_t>(state.runtime.swo_status & ~kSwoCaptureActive);
                        }
                        ok = true;
                    }
                    out[1] = ok ? kDapOk : kDapError;
                    return {2, 2, ok};
                }
                case kCmsisDapSwoStatus: {
                    if (out_size < 6) {
                        out[0] = kCmsisDapInvalid;
                        return {1, 1, false};
                    }
                    out[1] = state.runtime.swo_status;
                    write_le32(&out[2], 0U);
                    return {1, 6, true};
                }
                case kCmsisDapSwoExtendedStatus: {
                    if (in_size < 2 || out_size < 2) {
                        out[0] = kCmsisDapInvalid;
                        return {1, 1, false};
                    }
                    std::uint16_t out_idx = 1;
                    const auto mask = in[1];
                    if ((mask & 0x01U) != 0U) {
                        if (out_idx >= out_size) {
                            out[0] = kCmsisDapInvalid;
                            return {1, 1, false};
                        }
                        out[out_idx++] = state.runtime.swo_status;
                    }
                    if ((mask & 0x02U) != 0U) {
                        if ((out_idx + 3U) >= out_size) {
                            out[0] = kCmsisDapInvalid;
                            return {1, 1, false};
                        }
                        write_le32(&out[out_idx], 0U);
                        out_idx = static_cast<std::uint16_t>(out_idx + 4U);
                    }
                    return {2, out_idx, true};
                }
                case kCmsisDapSwoData: {
                    if (in_size < 3 || out_size < 4) {
                        out[0] = kCmsisDapInvalid;
                        return {1, 1, false};
                    }
                    const std::uint16_t max_count = static_cast<std::uint16_t>(in[1] | (in[2] << 8));
                    (void)max_count;
                    out[1] = state.runtime.swo_status;
                    out[2] = 0;
                    out[3] = 0;
                    return {3, 4, true};
                }
#endif
#if CHARM_DAP_ENABLE_DAP_UART
                case kCmsisDapUartTransport:
                case kCmsisDapUartConfigure:
                case kCmsisDapUartTransfer:
                case kCmsisDapUartControl:
                case kCmsisDapUartStatus:
                    if (out_size < 2) {
                        out[0] = kCmsisDapInvalid;
                        return {1, 1, false};
                    }
                    out[1] = kDapError;
                    return {2, 2, false};
#endif
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

                    if (state.runtime.dap_port != kDapPortSwd) {
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
                                state.config.match_mask = data;
                                response_count = static_cast<std::uint8_t>(response_count + 1);
                                continue;
                            }

                            if (is_match_value) {
                                std::uint32_t sampled = 0;
                                std::uint8_t ack = kDapTransferError;
                                std::uint16_t retries = state.config.match_retry;
                                while (true) {
                                    ack = dap_transfer_once<Backend>(state, request, sampled);
                                    if (ack != kDapTransferOk) {
                                        break;
                                    }
                                    if ((sampled & state.config.match_mask) == data) {
                                        break;
                                    }
                                    if (retries == 0U) {
                                        ack = static_cast<std::uint8_t>(kDapTransferOk | kDapTransferMismatch);
                                        break;
                                    }
                                    --retries;
                                }
                                Policy::template on_transfer_result<Backend, Ops>(state, ack);
                                if (ack != kDapTransferOk) {
                                    response_value = ack;
                                    break;
                                }
                                response_count = static_cast<std::uint8_t>(response_count + 1);
                                continue;
                            }

                            const auto ack = dap_transfer_once<Backend>(state, request, data);
                            Policy::template on_transfer_result<Backend, Ops>(state, ack);
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
                    const std::uint16_t max_write_words =
                        static_cast<std::uint16_t>((in_size > in_idx) ? ((in_size - in_idx) / 4U) : 0U);
                    std::uint16_t max_words = is_read ? max_read_words : max_write_words;
                    std::uint16_t exec_words = transfer_count;

                    if (state.runtime.dap_port != kDapPortSwd) {
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
                            Policy::template on_transfer_result<Backend, Ops>(state, ack);
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

        template <daplink::dap_backend::SwdBackend Backend, typename Ops, typename Policy>
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
                auto res = process_single<Backend, Ops, Policy>(
                    state, info, in.subspan(in_idx), out.subspan(out_idx));
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

    struct Processor {
        State& state;
        DeviceInfo info;

        Processor(State& s, DeviceInfo i) noexcept : state(s), info(i) {}

        template <daplink::dap_backend::SwdBackend Backend,
                  daplink::dap_backend::DapOps Ops = DefaultOps<Backend>,
                  typename Policy = daplink::dap_strategy::DefaultTransferPolicy<State>>
        void process_packet(
            std::span<const std::uint8_t, kPacketSize> in,
            std::span<std::uint8_t, kPacketSize> out
        ) noexcept {
            for (std::size_t i = 0; i < kPacketSize; ++i) {
                out[i] = 0;
            }

            const std::uint8_t cmd = in[0];
            if (cmd == detail::kCmsisDapQueueCommands) {
                detail::process_execute<Backend, Ops, Policy>(
                    state, info, in, out, detail::kCmsisDapExecuteCommands);
                return;
            }
            if (cmd == detail::kCmsisDapExecuteCommands) {
                detail::process_execute<Backend, Ops, Policy>(state, info, in, out, cmd);
                return;
            }

            detail::process_single<Backend, Ops, Policy>(state, info, in, out);
        }
    };

    template <daplink::dap_backend::SwdBackend Backend,
              daplink::dap_backend::DapOps Ops = DefaultOps<Backend>,
              typename Policy = daplink::dap_strategy::DefaultTransferPolicy<State>>
    inline void process_packet(
        State& state,
        DeviceInfo info,
        std::span<const std::uint8_t, kPacketSize> in,
        std::span<std::uint8_t, kPacketSize> out
    ) noexcept {
        Processor{state, info}.template process_packet<Backend, Ops, Policy>(in, out);
    }
}
