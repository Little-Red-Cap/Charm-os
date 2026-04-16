module;

#include <array>

export module net.protocol.diagnostic_udp;

export import net.packet;
export import net.protocol.diagnostic;
export import net.udp;
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

    template <util::usize MaxPayload = 64>
    class Server {
    public:
        using SendFn = Result<UdpSendDisposition> (*)(
            void* ctx,
            Endpoint local,
            const Endpoint& peer,
            ByteView payload) noexcept;
        using ErrorFn = void (*)(void* ctx, errc error) noexcept;
        using PingHandler = Status (*)(void* ctx,
                                       const PingRequest& request,
                                       PingReply& response) noexcept;
        using CountHandler = Status (*)(void* ctx,
                                        const EmptyMessage& request,
                                        CounterValue& response) noexcept;
        using MetaHandler = Status (*)(void* ctx,
                                       const MetaRequest& request,
                                       MetaReply& response) noexcept;

        static_assert(PingOp::RequestCodec::max_size() <= MaxPayload);
        static_assert(PingOp::ResponseCodec::max_size() <= MaxPayload);
        static_assert(CountOp::RequestCodec::max_size() <= MaxPayload);
        static_assert(CountOp::ResponseCodec::max_size() <= MaxPayload);
        static_assert(MetaOp::RequestCodec::max_size() <= MaxPayload);
        static_assert(MetaOp::ResponseCodec::max_size() <= MaxPayload);

        void set_sender(SendFn fn, void* ctx) noexcept {
            sender_ = fn;
            sender_ctx_ = ctx;
        }

        void set_error_handler(ErrorFn fn, void* ctx = nullptr) noexcept {
            error_ = fn;
            error_ctx_ = ctx;
        }

        void reset() noexcept {
            request_count_ = 0;
            reply_count_ = 0;
            error_reply_count_ = 0;
            queued_reply_count_ = 0;
            drop_count_ = 0;
            last_error_ = errc::ok;
        }

        [[nodiscard]] util::usize request_count() const noexcept {
            return request_count_;
        }

        [[nodiscard]] util::usize reply_count() const noexcept {
            return reply_count_;
        }

        [[nodiscard]] util::usize error_reply_count() const noexcept {
            return error_reply_count_;
        }

        [[nodiscard]] util::usize queued_reply_count() const noexcept {
            return queued_reply_count_;
        }

        [[nodiscard]] util::usize drop_count() const noexcept {
            return drop_count_;
        }

        [[nodiscard]] errc last_error() const noexcept {
            return last_error_;
        }

        [[nodiscard]] Result<void> on_ping(PingHandler fn,
                                           void* ctx = nullptr) noexcept {
            if (!fn) {
                return util::unexpected(errc::invalid_arg);
            }
            ping_handler_ = fn;
            ping_ctx_ = ctx;
            return {};
        }

        [[nodiscard]] Result<void> on_count(CountHandler fn,
                                            void* ctx = nullptr) noexcept {
            if (!fn) {
                return util::unexpected(errc::invalid_arg);
            }
            count_handler_ = fn;
            count_ctx_ = ctx;
            return {};
        }

        [[nodiscard]] Result<void> on_meta(MetaHandler fn,
                                           void* ctx = nullptr) noexcept {
            if (!fn) {
                return util::unexpected(errc::invalid_arg);
            }
            meta_handler_ = fn;
            meta_ctx_ = ctx;
            return {};
        }

        [[nodiscard]] bool has_ping() const noexcept {
            return ping_handler_ != nullptr;
        }

        [[nodiscard]] bool has_count() const noexcept {
            return count_handler_ != nullptr;
        }

        [[nodiscard]] bool has_meta() const noexcept {
            return meta_handler_ != nullptr;
        }

        [[nodiscard]] Result<void> consume(const UdpDatagramInfo& info,
                                           OwnedPacket packet) noexcept {
            const auto parsed = parse_datagram(packet.view());
            if (!parsed) {
                ++drop_count_;
                return {};
            }
            if (parsed.value().header.kind != Kind::request) {
                ++drop_count_;
                return {};
            }

            ++request_count_;
            switch (parsed.value().header.opcode) {
                case PingOp::opcode: {
                    const auto request = decode_request<PingOp>(parsed.value());
                    if (!request) {
                        return send_error_reply(info,
                                                PingOp::opcode,
                                                parsed.value().header.request_id,
                                                Status::bad_request);
                    }
                    if (!ping_handler_) {
                        return send_error_reply(info,
                                                PingOp::opcode,
                                                parsed.value().header.request_id,
                                                Status::unsupported);
                    }

                    PingReply response{};
                    const auto status = ping_handler_(ping_ctx_, request.value(), response);
                    if (status != Status::ok) {
                        return send_error_reply(
                            info,
                            PingOp::opcode,
                            parsed.value().header.request_id,
                            status);
                    }
                    return send_typed_reply<PingOp>(
                        info,
                        parsed.value().header.request_id,
                        response);
                }

                case CountOp::opcode: {
                    const auto request = decode_request<CountOp>(parsed.value());
                    if (!request) {
                        return send_error_reply(info,
                                                CountOp::opcode,
                                                parsed.value().header.request_id,
                                                Status::bad_request);
                    }
                    if (!count_handler_) {
                        return send_error_reply(info,
                                                CountOp::opcode,
                                                parsed.value().header.request_id,
                                                Status::unsupported);
                    }

                    CounterValue response{};
                    const auto status = count_handler_(count_ctx_, request.value(), response);
                    if (status != Status::ok) {
                        return send_error_reply(
                            info,
                            CountOp::opcode,
                            parsed.value().header.request_id,
                            status);
                    }
                    return send_typed_reply<CountOp>(
                        info,
                        parsed.value().header.request_id,
                        response);
                }

                case MetaOp::opcode: {
                    const auto request = decode_request<MetaOp>(parsed.value());
                    if (!request) {
                        return send_error_reply(info,
                                                MetaOp::opcode,
                                                parsed.value().header.request_id,
                                                Status::bad_request);
                    }
                    if (!meta_handler_) {
                        return send_error_reply(info,
                                                MetaOp::opcode,
                                                parsed.value().header.request_id,
                                                Status::unsupported);
                    }

                    MetaReply response{};
                    const auto status = meta_handler_(meta_ctx_, request.value(), response);
                    if (status != Status::ok) {
                        return send_error_reply(
                            info,
                            MetaOp::opcode,
                            parsed.value().header.request_id,
                            status);
                    }
                    return send_typed_reply<MetaOp>(
                        info,
                        parsed.value().header.request_id,
                        response);
                }

                default:
                    return send_error_reply(info,
                                            parsed.value().header.opcode,
                                            parsed.value().header.request_id,
                                            Status::unsupported);
            }
        }

    private:
        static constexpr util::usize wire_capacity = MaxPayload + header_size();

        template <ServiceOperation Op>
        [[nodiscard]] Result<void> send_typed_reply(const UdpDatagramInfo& info,
                                                    util::u16 request_id,
                                                    const typename Op::Response& response) noexcept {
            PacketBuffer<wire_capacity> datagram{};
            auto encoded = write_response_datagram<Op>(
                datagram,
                request_id,
                Status::ok,
                response);
            if (!encoded) {
                report_error(encoded.error());
                return util::unexpected(encoded.error());
            }
            return send_payload(info, datagram.view().payload, Status::ok);
        }

        [[nodiscard]] Result<void> send_error_reply(const UdpDatagramInfo& info,
                                                    util::u8 opcode,
                                                    util::u16 request_id,
                                                    Status status) noexcept {
            PacketBuffer<wire_capacity> datagram{};
            auto encoded = write_error_datagram(datagram, opcode, request_id, status);
            if (!encoded) {
                report_error(encoded.error());
                return util::unexpected(encoded.error());
            }
            return send_payload(info, datagram.view().payload, status);
        }

        [[nodiscard]] Result<void> send_payload(const UdpDatagramInfo& info,
                                                ByteView payload,
                                                Status status) noexcept {
            if (!sender_) {
                report_error(errc::bad_state);
                return util::unexpected(errc::bad_state);
            }

            auto sent = sender_(sender_ctx_, info.local, info.peer, payload);
            if (!sent) {
                report_error(sent.error());
                return util::unexpected(sent.error());
            }

            ++reply_count_;
            if (status != Status::ok) {
                ++error_reply_count_;
            }
            if (sent.value() == UdpSendDisposition::queued) {
                ++queued_reply_count_;
            }
            return {};
        }

        void report_error(errc error) noexcept {
            last_error_ = error;
            if (error_) {
                error_(error_ctx_, error);
            }
        }

        SendFn sender_{nullptr};
        void* sender_ctx_{nullptr};
        ErrorFn error_{nullptr};
        void* error_ctx_{nullptr};
        PingHandler ping_handler_{nullptr};
        void* ping_ctx_{nullptr};
        CountHandler count_handler_{nullptr};
        void* count_ctx_{nullptr};
        MetaHandler meta_handler_{nullptr};
        void* meta_ctx_{nullptr};
        util::usize request_count_{0};
        util::usize reply_count_{0};
        util::usize error_reply_count_{0};
        util::usize queued_reply_count_{0};
        util::usize drop_count_{0};
        errc last_error_{errc::ok};
    };
}
