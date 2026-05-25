module;

#include <concepts>

export module net.udp_protocol_binding;

export import net.common;
export import net.pump;
export import net.udp;
import util.core;
import util.error;
import util.expected;

export namespace net {
    template <class Protocol>
    concept UdpSenderRefBindable = UdpDatagramSink<Protocol>
        && requires(Protocol& protocol, UdpSenderRef sender) {
               protocol.set_sender(sender);
           };

    template <class Protocol>
    concept UdpLegacySenderBindable = UdpDatagramSink<Protocol>
        && requires(Protocol& protocol, UdpSendFn fn, void* ctx) {
               protocol.set_sender(fn, ctx);
           };

    template <class Pump, class Protocol>
    concept UdpProtocolPump = (UdpSenderRefBindable<Protocol> || UdpLegacySenderBindable<Protocol>)
        && requires(Pump& pump,
                    util::u16 local_port,
                    Protocol& protocol,
                    Endpoint local,
                    const Endpoint& peer,
                    ByteView payload) {
               { pump.bind_udp(local_port, protocol) } noexcept -> std::same_as<Result<void>>;
               { pump.send(local, peer, payload) } noexcept -> std::same_as<Result<UdpSendDisposition>>;
           };

    namespace detail {
        template <class Pump>
        [[nodiscard]] Result<UdpSendDisposition> udp_pump_send_trampoline(
            void* ctx,
            Endpoint local,
            const Endpoint& peer,
            ByteView payload) noexcept {
            auto* pump = static_cast<Pump*>(ctx);
            if (!pump) {
                return util::unexpected(errc::bad_state);
            }
            return pump->send(local, peer, payload);
        }
    }

    template <class Pump, class Protocol>
        requires UdpProtocolPump<Pump, Protocol>
    [[nodiscard]] Result<void> bind_udp_protocol(Pump& pump,
                                                 util::u16 local_port,
                                                 Protocol& protocol) noexcept {
        auto bound = pump.bind_udp(local_port, protocol);
        if (!bound) {
            return util::unexpected(bound.error());
        }

        if constexpr (UdpSenderRefBindable<Protocol>) {
            protocol.set_sender(UdpSenderRef::raw(&detail::udp_pump_send_trampoline<Pump>, &pump));
        } else {
            protocol.set_sender(&detail::udp_pump_send_trampoline<Pump>, &pump);
        }
        return {};
    }

    template <class Pump, class Protocol>
        requires UdpProtocolPump<Pump, Protocol>
    [[nodiscard]] Result<void> bind_udp_protocol(Pump& pump,
                                                 const Endpoint& local,
                                                 Protocol& protocol) noexcept {
        if (local.port == 0u) {
            return util::unexpected(errc::invalid_arg);
        }
        return bind_udp_protocol(pump, local.port, protocol);
    }
}
