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

    class IcmpEchoSenderRef {
    public:
        constexpr IcmpEchoSenderRef() noexcept = default;

        static constexpr IcmpEchoSenderRef raw(IcmpEchoSendFn send, void* ctx) noexcept {
            return IcmpEchoSenderRef{send, ctx};
        }

        template <typename Sender>
            requires(
                requires(Sender& value,
                         IpAddress local,
                         IpAddress peer,
                         IcmpType type,
                         util::u16 identifier,
                         util::u16 sequence,
                         ByteView payload) {
                    {
                        value.send(local, peer, type, identifier, sequence, payload)
                    } noexcept -> std::same_as<Result<IcmpSendDisposition>>;
                } ||
                requires(Sender& value,
                         IpAddress local,
                         IpAddress peer,
                         IcmpType type,
                         util::u16 identifier,
                         util::u16 sequence,
                         ByteView payload) {
                    {
                        value(local, peer, type, identifier, sequence, payload)
                    } noexcept -> std::same_as<Result<IcmpSendDisposition>>;
                })
        static constexpr IcmpEchoSenderRef bind(Sender& sender) noexcept {
            return IcmpEchoSenderRef{&invoke<Sender>, &sender};
        }

        [[nodiscard]] constexpr explicit operator bool() const noexcept {
            return send_ != nullptr;
        }

        [[nodiscard]] Result<IcmpSendDisposition> send(IpAddress local,
                                                       IpAddress peer,
                                                       IcmpType type,
                                                       util::u16 identifier,
                                                       util::u16 sequence,
                                                       ByteView payload) const noexcept {
            if (!send_) {
                return util::unexpected(errc::bad_state);
            }
            return send_(ctx_, local, peer, type, identifier, sequence, payload);
        }

    private:
        constexpr IcmpEchoSenderRef(IcmpEchoSendFn send, void* ctx) noexcept
            : send_(send),
              ctx_(ctx) {
        }

        template <typename Sender>
        static Result<IcmpSendDisposition> invoke(void* ctx,
                                                  IpAddress local,
                                                  IpAddress peer,
                                                  IcmpType type,
                                                  util::u16 identifier,
                                                  util::u16 sequence,
                                                  ByteView payload) noexcept {
            auto* sender = static_cast<Sender*>(ctx);
            if (!sender) {
                return util::unexpected(errc::bad_state);
            }
            if constexpr (requires(Sender& value,
                                   IpAddress src,
                                   IpAddress dst,
                                   IcmpType icmp_type,
                                   util::u16 id,
                                   util::u16 seq,
                                   ByteView bytes) {
                              {
                                  value.send(src, dst, icmp_type, id, seq, bytes)
                              } noexcept -> std::same_as<Result<IcmpSendDisposition>>;
                          }) {
                return sender->send(local, peer, type, identifier, sequence, payload);
            } else {
                return (*sender)(local, peer, type, identifier, sequence, payload);
            }
        }

        IcmpEchoSendFn send_{nullptr};
        void* ctx_{nullptr};
    };

    class IcmpControlSenderRef {
    public:
        constexpr IcmpControlSenderRef() noexcept = default;

        static constexpr IcmpControlSenderRef raw(IcmpControlSendFn send, void* ctx) noexcept {
            return IcmpControlSenderRef{send, ctx};
        }

        template <typename Sender>
            requires(
                requires(Sender& value,
                         IpAddress local,
                         IpAddress peer,
                         IcmpType type,
                         util::u16 identifier,
                         util::u16 sequence,
                         ByteView payload,
                         util::u8 ttl,
                         util::u16 ipv4_identification,
                         util::u8 dscp_ecn) {
                    {
                        value.send(
                            local,
                            peer,
                            type,
                            identifier,
                            sequence,
                            payload,
                            ttl,
                            ipv4_identification,
                            dscp_ecn)
                    } noexcept -> std::same_as<Result<IcmpSendDisposition>>;
                } ||
                requires(Sender& value,
                         IpAddress local,
                         IpAddress peer,
                         IcmpType type,
                         util::u16 identifier,
                         util::u16 sequence,
                         ByteView payload,
                         util::u8 ttl,
                         util::u16 ipv4_identification,
                         util::u8 dscp_ecn) {
                    {
                        value(
                            local,
                            peer,
                            type,
                            identifier,
                            sequence,
                            payload,
                            ttl,
                            ipv4_identification,
                            dscp_ecn)
                    } noexcept -> std::same_as<Result<IcmpSendDisposition>>;
                })
        static constexpr IcmpControlSenderRef bind(Sender& sender) noexcept {
            return IcmpControlSenderRef{&invoke<Sender>, &sender};
        }

        [[nodiscard]] constexpr explicit operator bool() const noexcept {
            return send_ != nullptr;
        }

        [[nodiscard]] Result<IcmpSendDisposition> send(IpAddress local,
                                                       IpAddress peer,
                                                       IcmpType type,
                                                       util::u16 identifier,
                                                       util::u16 sequence,
                                                       ByteView payload,
                                                       util::u8 ttl,
                                                       util::u16 ipv4_identification,
                                                       util::u8 dscp_ecn) const noexcept {
            if (!send_) {
                return util::unexpected(errc::bad_state);
            }
            return send_(
                ctx_,
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

    private:
        constexpr IcmpControlSenderRef(IcmpControlSendFn send, void* ctx) noexcept
            : send_(send),
              ctx_(ctx) {
        }

        template <typename Sender>
        static Result<IcmpSendDisposition> invoke(void* ctx,
                                                  IpAddress local,
                                                  IpAddress peer,
                                                  IcmpType type,
                                                  util::u16 identifier,
                                                  util::u16 sequence,
                                                  ByteView payload,
                                                  util::u8 ttl,
                                                  util::u16 ipv4_identification,
                                                  util::u8 dscp_ecn) noexcept {
            auto* sender = static_cast<Sender*>(ctx);
            if (!sender) {
                return util::unexpected(errc::bad_state);
            }
            if constexpr (requires(Sender& value,
                                   IpAddress src,
                                   IpAddress dst,
                                   IcmpType icmp_type,
                                   util::u16 id,
                                   util::u16 seq,
                                   ByteView bytes,
                                   util::u8 hop_limit,
                                   util::u16 ipv4_id,
                                   util::u8 tos) {
                              {
                                  value.send(
                                      src,
                                      dst,
                                      icmp_type,
                                      id,
                                      seq,
                                      bytes,
                                      hop_limit,
                                      ipv4_id,
                                      tos)
                              } noexcept -> std::same_as<Result<IcmpSendDisposition>>;
                          }) {
                return sender->send(
                    local,
                    peer,
                    type,
                    identifier,
                    sequence,
                    payload,
                    ttl,
                    ipv4_identification,
                    dscp_ecn);
            } else {
                return (*sender)(
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

        IcmpControlSendFn send_{nullptr};
        void* ctx_{nullptr};
    };

    template <class Protocol>
    concept IcmpEchoSenderRefBindable = requires(Protocol& protocol, IcmpEchoSenderRef sender) {
        protocol.set_sender(sender);
    };

    template <class Protocol>
    concept IcmpControlSenderRefBindable = requires(Protocol& protocol, IcmpControlSenderRef sender) {
        protocol.set_sender(sender);
    };

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
        if constexpr (IcmpControlSenderRefBindable<Protocol>) {
            protocol.set_sender(IcmpControlSenderRef::raw(
                &detail::icmp_pump_send_trampoline<Pump>,
                &pump));
        } else if constexpr (IcmpEchoSenderRefBindable<Protocol>) {
            protocol.set_sender(IcmpEchoSenderRef::raw(
                &detail::icmp_pump_echo_send_trampoline<Pump>,
                &pump));
        } else if constexpr (IcmpControlSenderBindable<Protocol>) {
            protocol.set_sender(&detail::icmp_pump_send_trampoline<Pump>, &pump);
        } else if constexpr (IcmpEchoSenderBindable<Protocol>) {
            protocol.set_sender(&detail::icmp_pump_echo_send_trampoline<Pump>, &pump);
        }
        return {};
    }
}
