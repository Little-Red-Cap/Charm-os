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

    template <class Protocol>
    concept IcmpPumpBindableProtocol = IcmpEchoSink<Protocol>
        && requires(Protocol& protocol,
                    IcmpEchoSendFn fn,
                    void* ctx) {
               protocol.set_sender(fn, ctx);
           };

    template <class Pump, class Protocol>
    concept IcmpProtocolPump = IcmpPumpBindableProtocol<Protocol>
        && requires(Pump& pump,
                    Protocol& protocol,
                    IpAddress local,
                    IpAddress peer,
                    ByteView payload) {
               pump.set_echo_sink(protocol);
               { pump.send(local, peer, IcmpType::echo_request, 1u, 2u, payload) } noexcept
                   -> std::same_as<Result<IcmpSendDisposition>>;
           };

    namespace detail {
        template <class Pump>
        [[nodiscard]] Result<IcmpSendDisposition> icmp_pump_send_trampoline(
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
    }

    template <class Pump, class Protocol>
        requires IcmpProtocolPump<Pump, Protocol>
    [[nodiscard]] Result<void> bind_icmp_protocol(Pump& pump,
                                                  Protocol& protocol) noexcept {
        pump.set_echo_sink(protocol);
        protocol.set_sender(&detail::icmp_pump_send_trampoline<Pump>, &pump);
        return {};
    }
}
