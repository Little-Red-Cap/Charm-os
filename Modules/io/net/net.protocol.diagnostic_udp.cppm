module;

#include <array>

export module net.protocol.diagnostic_udp;

export import net.packet;
export import net.protocol.diagnostic;
export import net.udp;
import net.udp_service_codec;
import util.core;
import util.error;
import util.expected;

export namespace net::diag::udp {
    enum class Kind : util::u8 {
        request = 0u,
        response = 1u,
    };

    enum class Status : util::u8 {
        ok = 0u,
        bad_request = 1u,
        unsupported = 2u,
        internal_error = 3u,
    };

    struct Header {
        Kind kind{Kind::request};
        util::u8 opcode{0};
        util::u16 request_id{0};
        Status status{Status::ok};
    };

    struct DatagramView {
        Header header{};
        PacketView payload{};
    };

    [[nodiscard]] constexpr util::u8 magic0() noexcept {
        return 0x43u;
    }

    [[nodiscard]] constexpr util::u8 magic1() noexcept {
        return 0x44u;
    }

    [[nodiscard]] constexpr util::u8 version() noexcept {
        return 1u;
    }

    [[nodiscard]] constexpr util::usize header_size() noexcept {
        return 8u;
    }

    template <util::usize Capacity>
    [[nodiscard]] Result<void> write_raw_datagram(PacketBuffer<Capacity>& packet,
                                                  Kind kind,
                                                  util::u8 opcode,
                                                  util::u16 request_id,
                                                  Status status,
                                                  ByteView payload) noexcept;

    [[nodiscard]] constexpr Result<DatagramView> parse_datagram(PacketView packet) noexcept;

    template <ServiceOperation Op>
    [[nodiscard]] Result<typename Op::Request> decode_request(const DatagramView& datagram) noexcept;

    template <ServiceOperation Op>
    [[nodiscard]] Result<typename Op::Response> decode_response(const DatagramView& datagram) noexcept;

    template <ServiceOperation Op, util::usize Capacity>
    [[nodiscard]] Result<void> write_request_datagram(PacketBuffer<Capacity>& packet,
                                                      util::u16 request_id,
                                                      const typename Op::Request& request) noexcept;

    template <ServiceOperation Op, util::usize Capacity>
    [[nodiscard]] Result<void> write_response_datagram(PacketBuffer<Capacity>& packet,
                                                       util::u16 request_id,
                                                       Status status,
                                                       const typename Op::Response& response) noexcept;

    template <util::usize Capacity>
    [[nodiscard]] Result<void> write_error_datagram(PacketBuffer<Capacity>& packet,
                                                    util::u8 opcode,
                                                    util::u16 request_id,
                                                    Status status) noexcept;

    namespace detail {
        [[nodiscard]] constexpr util::u16 load_be16(PacketView packet, util::usize offset) noexcept {
            return static_cast<util::u16>(
                (static_cast<util::u16>(packet[offset]) << 8)
                | static_cast<util::u16>(packet[offset + 1]));
        }

        constexpr void store_be16(util::u8* out, util::u16 value) noexcept {
            out[0] = static_cast<util::u8>((value >> 8) & 0xFFu);
            out[1] = static_cast<util::u8>(value & 0xFFu);
        }

        template <class Codec, class T, util::usize Capacity>
        [[nodiscard]] Result<void> write_typed_datagram(PacketBuffer<Capacity>& packet,
                                                        Kind kind,
                                                        util::u8 opcode,
                                                        util::u16 request_id,
                                                        Status status,
                                                        const T& value) noexcept {
            std::array<util::u8, Codec::max_size()> encoded_payload{};
            auto encoded = Codec::encode(
                value,
                MutByteView{encoded_payload.data(), encoded_payload.size()});
            if (!encoded) {
                return util::unexpected(encoded.error());
            }

            return write_raw_datagram(
                packet,
                kind,
                opcode,
                request_id,
                status,
                ByteView{encoded_payload.data(), encoded.value()});
        }

        [[nodiscard]] constexpr errc status_error(Status status) noexcept {
            switch (status) {
                case Status::ok:
                    return errc::ok;
                case Status::bad_request:
                    return errc::invalid_arg;
                case Status::unsupported:
                    return errc::not_supported;
                case Status::internal_error:
                    return errc::bad_state;
            }
            return errc::invalid_arg;
        }

        struct WireTraits {
            using Status = net::diag::udp::Status;
            using DatagramView = net::diag::udp::DatagramView;

            [[nodiscard]] static constexpr util::usize header_size() noexcept {
                return net::diag::udp::header_size();
            }

            [[nodiscard]] static constexpr Kind request_kind() noexcept {
                return Kind::request;
            }

            [[nodiscard]] static constexpr Kind response_kind() noexcept {
                return Kind::response;
            }

            [[nodiscard]] static constexpr Status ok_status() noexcept {
                return Status::ok;
            }

            [[nodiscard]] static constexpr Status bad_request_status() noexcept {
                return Status::bad_request;
            }

            [[nodiscard]] static constexpr Status unsupported_status() noexcept {
                return Status::unsupported;
            }

            [[nodiscard]] static constexpr Status internal_error_status() noexcept {
                return Status::internal_error;
            }

            [[nodiscard]] static constexpr errc status_error(Status status) noexcept {
                return net::diag::udp::detail::status_error(status);
            }

            [[nodiscard]] static constexpr Result<DatagramView> parse_datagram(PacketView packet) noexcept {
                return net::diag::udp::parse_datagram(packet);
            }

            template <ServiceOperation Op>
            [[nodiscard]] static Result<typename Op::Request> decode_request(
                const DatagramView& datagram) noexcept {
                return net::diag::udp::decode_request<Op>(datagram);
            }

            template <ServiceOperation Op, util::usize Capacity>
            [[nodiscard]] static Result<void> write_request_datagram(
                PacketBuffer<Capacity>& packet,
                util::u16 request_id,
                const typename Op::Request& request) noexcept {
                return net::diag::udp::write_request_datagram<Op>(packet, request_id, request);
            }

            template <ServiceOperation Op, util::usize Capacity>
            [[nodiscard]] static Result<void> write_response_datagram(
                PacketBuffer<Capacity>& packet,
                util::u16 request_id,
                Status status,
                const typename Op::Response& response) noexcept {
                return net::diag::udp::write_response_datagram<Op>(
                    packet,
                    request_id,
                    status,
                    response);
            }

            template <util::usize Capacity>
            [[nodiscard]] static Result<void> write_error_datagram(
                PacketBuffer<Capacity>& packet,
                util::u8 opcode,
                util::u16 request_id,
                Status status) noexcept {
                return net::diag::udp::write_error_datagram(packet, opcode, request_id, status);
            }
        };
    }

    template <util::usize Capacity>
    [[nodiscard]] Result<void> write_raw_datagram(PacketBuffer<Capacity>& packet,
                                                  Kind kind,
                                                  util::u8 opcode,
                                                  util::u16 request_id,
                                                  Status status,
                                                  ByteView payload) noexcept {
        if ((header_size() + payload.size()) > Capacity) {
            return util::unexpected(errc::buffer_overflow);
        }

        auto reset = packet.reset();
        if (!reset) {
            return util::unexpected(reset.error());
        }

        std::array<util::u8, header_size()> header{};
        header[0] = magic0();
        header[1] = magic1();
        header[2] = version();
        header[3] = static_cast<util::u8>(kind);
        header[4] = opcode;
        header[5] = static_cast<util::u8>(status);
        detail::store_be16(header.data() + 6, request_id);

        auto appended_header = packet.append(ByteView{header.data(), header.size()});
        if (!appended_header) {
            return util::unexpected(appended_header.error());
        }

        auto appended_payload = packet.append(payload);
        if (!appended_payload) {
            return util::unexpected(appended_payload.error());
        }
        return {};
    }

    [[nodiscard]] constexpr Result<DatagramView> parse_datagram(PacketView packet) noexcept {
        if (packet.size() < header_size()) {
            return util::unexpected(errc::invalid_format);
        }
        if (packet[0] != magic0() || packet[1] != magic1()) {
            return util::unexpected(errc::invalid_format);
        }
        if (packet[2] != version()) {
            return util::unexpected(errc::not_supported);
        }

        const auto raw_kind = packet[3];
        if (raw_kind != static_cast<util::u8>(Kind::request)
            && raw_kind != static_cast<util::u8>(Kind::response)) {
            return util::unexpected(errc::invalid_format);
        }

        const auto raw_status = packet[5];
        if (raw_status > static_cast<util::u8>(Status::internal_error)) {
            return util::unexpected(errc::invalid_format);
        }

        return Result<DatagramView>{std::in_place, DatagramView{
            .header = Header{
                .kind = static_cast<Kind>(raw_kind),
                .opcode = packet[4],
                .request_id = detail::load_be16(packet, 6),
                .status = static_cast<Status>(raw_status),
            },
            .payload = packet.subspan(header_size()),
        }};
    }

    template <ServiceOperation Op>
    [[nodiscard]] Result<typename Op::Request> decode_request(const DatagramView& datagram) noexcept {
        if (datagram.header.kind != Kind::request || datagram.header.opcode != Op::opcode) {
            return util::unexpected(errc::invalid_format);
        }
        return Op::RequestCodec::decode(datagram.payload.payload);
    }

    template <ServiceOperation Op>
    [[nodiscard]] Result<typename Op::Response> decode_response(const DatagramView& datagram) noexcept {
        if (datagram.header.kind != Kind::response || datagram.header.opcode != Op::opcode) {
            return util::unexpected(errc::invalid_format);
        }
        return Op::ResponseCodec::decode(datagram.payload.payload);
    }

    template <ServiceOperation Op, util::usize Capacity>
    [[nodiscard]] Result<void> write_request_datagram(PacketBuffer<Capacity>& packet,
                                                      util::u16 request_id,
                                                      const typename Op::Request& request) noexcept {
        return detail::write_typed_datagram<typename Op::RequestCodec>(
            packet,
            Kind::request,
            Op::opcode,
            request_id,
            Status::ok,
            request);
    }

    template <ServiceOperation Op, util::usize Capacity>
    [[nodiscard]] Result<void> write_response_datagram(PacketBuffer<Capacity>& packet,
                                                       util::u16 request_id,
                                                       Status status,
                                                       const typename Op::Response& response) noexcept {
        return detail::write_typed_datagram<typename Op::ResponseCodec>(
            packet,
            Kind::response,
            Op::opcode,
            request_id,
            status,
            response);
    }

    template <util::usize Capacity>
    [[nodiscard]] Result<void> write_error_datagram(PacketBuffer<Capacity>& packet,
                                                    util::u8 opcode,
                                                    util::u16 request_id,
                                                    Status status) noexcept {
        return write_raw_datagram(
            packet,
            Kind::response,
            opcode,
            request_id,
            status,
            {});
    }

    template <util::usize MaxPayload = 64, util::usize MaxPending = 4>
    class Client : private net::udp::service::Client<detail::WireTraits, MaxPayload, MaxPending> {
        using Base = net::udp::service::Client<detail::WireTraits, MaxPayload, MaxPending>;

    public:
        using SendFn = typename Base::SendFn;
        using ErrorFn = typename Base::ErrorFn;

        template <ServiceOperation Op>
        using ResponseFn = typename Base::template ResponseFn<Op>;

        template <ServiceOperation Op>
        using TimeoutFn = typename Base::template TimeoutFn<Op>;

        static_assert(PingOp::RequestCodec::max_size() <= MaxPayload);
        static_assert(PingOp::ResponseCodec::max_size() <= MaxPayload);
        static_assert(CountOp::RequestCodec::max_size() <= MaxPayload);
        static_assert(CountOp::ResponseCodec::max_size() <= MaxPayload);
        static_assert(SlowCountOp::RequestCodec::max_size() <= MaxPayload);
        static_assert(SlowCountOp::ResponseCodec::max_size() <= MaxPayload);
        static_assert(MetaOp::RequestCodec::max_size() <= MaxPayload);
        static_assert(MetaOp::ResponseCodec::max_size() <= MaxPayload);

        using Base::cancel_request;
        using Base::consume;
        using Base::drop_count;
        using Base::has_pending;
        using Base::last_error;
        using Base::payload_capacity;
        using Base::pending_count;
        using Base::queued_count;
        using Base::request_count;
        using Base::reset;
        using Base::response_count;
        using Base::set_error_handler;
        using Base::set_sender;
        using Base::tick;
        using Base::timeout_count;

        [[nodiscard]] Result<util::u16> ping(const Endpoint& local,
                                             const Endpoint& peer,
                                             const PingRequest& request,
                                             util::u32 now_ms,
                                             util::u32 timeout_ms,
                                             ResponseFn<PingOp> on_response = nullptr,
                                             TimeoutFn<PingOp> on_timeout = nullptr,
                                             void* user = nullptr) noexcept {
            return this->template send_request<PingOp>(
                local,
                peer,
                request,
                now_ms,
                timeout_ms,
                on_response,
                on_timeout,
                user);
        }

        [[nodiscard]] Result<util::u16> query_count(const Endpoint& local,
                                                    const Endpoint& peer,
                                                    util::u32 now_ms,
                                                    util::u32 timeout_ms,
                                                    ResponseFn<CountOp> on_response = nullptr,
                                                    TimeoutFn<CountOp> on_timeout = nullptr,
                                                    void* user = nullptr) noexcept {
            return this->template send_request<CountOp>(
                local,
                peer,
                EmptyMessage{},
                now_ms,
                timeout_ms,
                on_response,
                on_timeout,
                user);
        }

        [[nodiscard]] Result<util::u16> query_slow_count(const Endpoint& local,
                                                         const Endpoint& peer,
                                                         const CounterValue& request,
                                                         util::u32 now_ms,
                                                         util::u32 timeout_ms,
                                                         ResponseFn<SlowCountOp> on_response = nullptr,
                                                         TimeoutFn<SlowCountOp> on_timeout = nullptr,
                                                         void* user = nullptr) noexcept {
            return this->template send_request<SlowCountOp>(
                local,
                peer,
                request,
                now_ms,
                timeout_ms,
                on_response,
                on_timeout,
                user);
        }

        [[nodiscard]] Result<util::u16> query_meta(const Endpoint& local,
                                                   const Endpoint& peer,
                                                   const MetaRequest& request,
                                                   util::u32 now_ms,
                                                   util::u32 timeout_ms,
                                                   ResponseFn<MetaOp> on_response = nullptr,
                                                   TimeoutFn<MetaOp> on_timeout = nullptr,
                                                   void* user = nullptr) noexcept {
            return this->template send_request<MetaOp>(
                local,
                peer,
                request,
                now_ms,
                timeout_ms,
                on_response,
                on_timeout,
                user);
        }
    };

    template <util::usize MaxPayload = 64>
    class Server : private net::udp::service::Server<detail::WireTraits, MaxPayload, 4> {
        using Base = net::udp::service::Server<detail::WireTraits, MaxPayload, 4>;

    public:
        using SendFn = typename Base::SendFn;
        using ErrorFn = typename Base::ErrorFn;
        using PingHandler = typename Base::template RouteFn<PingOp>;
        using CountHandler = typename Base::template RouteFn<CountOp>;
        using MetaHandler = typename Base::template RouteFn<MetaOp>;
        using SlowCountHandler = typename Base::template RouteFn<SlowCountOp>;

        static_assert(PingOp::RequestCodec::max_size() <= MaxPayload);
        static_assert(PingOp::ResponseCodec::max_size() <= MaxPayload);
        static_assert(CountOp::RequestCodec::max_size() <= MaxPayload);
        static_assert(CountOp::ResponseCodec::max_size() <= MaxPayload);
        static_assert(SlowCountOp::RequestCodec::max_size() <= MaxPayload);
        static_assert(SlowCountOp::ResponseCodec::max_size() <= MaxPayload);
        static_assert(MetaOp::RequestCodec::max_size() <= MaxPayload);
        static_assert(MetaOp::ResponseCodec::max_size() <= MaxPayload);

        using Base::consume;
        using Base::drop_count;
        using Base::error_reply_count;
        using Base::last_error;
        using Base::queued_reply_count;
        using Base::reply_count;
        using Base::request_count;
        using Base::reset;
        using Base::set_error_handler;
        using Base::set_sender;

        [[nodiscard]] Result<void> on_ping(PingHandler fn, void* ctx = nullptr) noexcept {
            return this->template set_route<PingOp>(fn, ctx);
        }

        [[nodiscard]] Result<void> on_count(CountHandler fn, void* ctx = nullptr) noexcept {
            return this->template set_route<CountOp>(fn, ctx);
        }

        [[nodiscard]] Result<void> on_meta(MetaHandler fn, void* ctx = nullptr) noexcept {
            return this->template set_route<MetaOp>(fn, ctx);
        }

        [[nodiscard]] Result<void> on_slow_count(SlowCountHandler fn,
                                                 void* ctx = nullptr) noexcept {
            return this->template set_route<SlowCountOp>(fn, ctx);
        }

        [[nodiscard]] bool has_ping() const noexcept {
            return this->template has_route<PingOp>();
        }

        [[nodiscard]] bool has_count() const noexcept {
            return this->template has_route<CountOp>();
        }

        [[nodiscard]] bool has_meta() const noexcept {
            return this->template has_route<MetaOp>();
        }

        [[nodiscard]] bool has_slow_count() const noexcept {
            return this->template has_route<SlowCountOp>();
        }
    };
}
