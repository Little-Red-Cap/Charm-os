module;

#include <concepts>

export module net.icmp_protocol_binding;

export import net.common;
export import net.icmp;
export import net.pump;
import util.core;
import util.error;
import util.expected;

export namespace net {
    using IcmpEchoSendFn = Result<IcmpSendDisposition> (*)(void* ctx,
                                                           IpAddress local,
                                                           IpAddress peer,
                                                           IcmpType type,
                                                           util::u16 identifier,
                                                           util::u16 sequence,
                                                           ByteView payload) noexcept;
    using IcmpControlSendFn = Result<IcmpSendDisposition> (*)(void* ctx,
                                                              IpAddress local,
                                                              IpAddress peer,
                                                              IcmpType type,
                                                              util::u16 identifier,
                                                              util::u16 sequence,
                                                              ByteView payload,
                                                              util::u8 ttl,
                                                              util::u16 ipv4_identification,
                                                              util::u8 dscp_ecn) noexcept;

    template <class Protocol>
    concept IcmpEchoSenderBindable = requires(Protocol& protocol,
                                              IcmpEchoSendFn fn,
                                              void* ctx) {
        protocol.set_sender(fn, ctx);
    };

    template <class Protocol>
    concept IcmpControlSenderBindable = requires(Protocol& protocol,
                                                 IcmpControlSendFn fn,
                                                 void* ctx) {
        protocol.set_sender(fn, ctx);
    };

    namespace detail {
        template <class Pump>
        [[nodiscard]] Result<IcmpSendDisposition> icmp_pump_echo_send_trampoline(
            void* ctx,
            IpAddress local,
            IpAddress peer,
            IcmpType type,
            util::u16 identifier,
            util::u16 sequence,
            ByteView payload) noexcept {
            auto* pump = static_cast<Pump*>(ctx);
            if (!pump) {
                return util::unexpected(errc::bad_state);
            }
            return pump->send(
                local,
                peer,
                type,
                identifier,
                sequence,
                payload);
        }

        template <class Pump>
        [[nodiscard]] Result<IcmpSendDisposition> icmp_pump_send_trampoline(
            void* ctx,
            IpAddress local,
            IpAddress peer,
            IcmpType type,
            util::u16 identifier,
            util::u16 sequence,
            ByteView payload,
            util::u8 ttl,
            util::u16 ipv4_identification,
            util::u8 dscp_ecn) noexcept {
            auto* pump = static_cast<Pump*>(ctx);
            if (!pump) {
                return util::unexpected(errc::bad_state);
            }
            return pump->send(
                local,
                peer,
                type,
                identifier,
                sequence,
                payload,
                ttl,
                ipv4_identification,
                dscp_ecn);
        }
    }

    template <class Pump, class Protocol>
    [[nodiscard]] Result<void> bind_icmp_protocol(Pump& pump,
                                                  Protocol& protocol) noexcept {
        if constexpr (IcmpEchoSink<Protocol>) {
            pump.set_echo_sink(protocol);
        }
        if constexpr (IcmpErrorQuoteSink<Protocol>) {
            pump.set_error_quote_sink(protocol);
        }
        if constexpr (IcmpControlSenderBindable<Protocol>) {
            protocol.set_sender(&detail::icmp_pump_send_trampoline<Pump>, &pump);
        } else if constexpr (IcmpEchoSenderBindable<Protocol>) {
            protocol.set_sender(&detail::icmp_pump_echo_send_trampoline<Pump>, &pump);
        }
        return {};
    }
}
