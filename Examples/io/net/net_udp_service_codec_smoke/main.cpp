#include <array>
#include <cstdio>

import charm.net;
import util.core;
import util.expected;

namespace {
    enum class WireKind : util::u8 {
        request = 0u,
        response = 1u,
    };

    enum class WireStatus : util::u8 {
        ok = 0u,
        bad_request = 1u,
        unsupported = 2u,
        internal_error = 3u,
    };

    struct WireHeader {
        WireKind kind{WireKind::request};
        util::u8 opcode{0};
        util::u16 request_id{0};
        WireStatus status{WireStatus::ok};
    };

    struct WireDatagramView {
        WireHeader header{};
        net::PacketView payload{};
    };

    struct EchoRequest {
        util::u8 value{0};
    };

    struct EchoReply {
        util::u8 value{0};
    };

    struct AddRequest {
        util::u8 lhs{0};
        util::u8 rhs{0};
    };

    struct AddReply {
        util::u8 value{0};
    };

    using EchoOp = net::TrivialServiceOp<0x41u, EchoRequest, EchoReply>;
    using AddOp = net::TrivialServiceOp<0x42u, AddRequest, AddReply>;

    [[nodiscard]] constexpr util::u8 magic0() noexcept {
        return 0xA5u;
    }

    [[nodiscard]] constexpr util::u8 magic1() noexcept {
        return 0x5Au;
    }

    [[nodiscard]] constexpr util::u8 version() noexcept {
        return 2u;
    }

    [[nodiscard]] constexpr util::usize header_size() noexcept {
        return 8u;
    }

    [[nodiscard]] constexpr util::u16 load_be16(net::PacketView packet, util::usize offset) noexcept {
        return static_cast<util::u16>(
            (static_cast<util::u16>(packet[offset]) << 8)
            | static_cast<util::u16>(packet[offset + 1]));
    }

    constexpr void store_be16(util::u8* out, util::u16 value) noexcept {
        out[0] = static_cast<util::u8>((value >> 8) & 0xFFu);
        out[1] = static_cast<util::u8>(value & 0xFFu);
    }

    [[nodiscard]] constexpr net::PacketView make_packet_view(net::ByteView payload) noexcept {
        return net::PacketView{
            .payload = payload,
            .headroom = 0,
            .tailroom = 0,
        };
    }

    [[nodiscard]] constexpr net::errc status_error(WireStatus status) noexcept {
        switch (status) {
            case WireStatus::ok:
                return net::errc::ok;
            case WireStatus::bad_request:
                return net::errc::invalid_arg;
            case WireStatus::unsupported:
                return net::errc::not_supported;
            case WireStatus::internal_error:
                return net::errc::bad_state;
        }
        return net::errc::invalid_arg;
    }

    template <util::usize Capacity>
    [[nodiscard]] net::Result<void> write_raw_datagram(net::PacketBuffer<Capacity>& packet,
                                                       WireKind kind,
                                                       util::u8 opcode,
                                                       util::u16 request_id,
                                                       WireStatus status,
                                                       net::ByteView payload) noexcept {
        if ((header_size() + payload.size()) > Capacity) {
            return util::unexpected(net::errc::buffer_overflow);
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
        store_be16(header.data() + 6, request_id);

        auto appended_header = packet.append(net::ByteView{header.data(), header.size()});
        if (!appended_header) {
            return util::unexpected(appended_header.error());
        }

        auto appended_payload = packet.append(payload);
        if (!appended_payload) {
            return util::unexpected(appended_payload.error());
        }
        return {};
    }

    [[nodiscard]] constexpr net::Result<WireDatagramView> parse_datagram(net::PacketView packet) noexcept {
        if (packet.size() < header_size()) {
            return util::unexpected(net::errc::invalid_format);
        }
        if (packet[0] != magic0() || packet[1] != magic1()) {
            return util::unexpected(net::errc::invalid_format);
        }
        if (packet[2] != version()) {
            return util::unexpected(net::errc::not_supported);
        }

        const auto raw_kind = packet[3];
        if (raw_kind != static_cast<util::u8>(WireKind::request)
            && raw_kind != static_cast<util::u8>(WireKind::response)) {
            return util::unexpected(net::errc::invalid_format);
        }

        const auto raw_status = packet[5];
        if (raw_status > static_cast<util::u8>(WireStatus::internal_error)) {
            return util::unexpected(net::errc::invalid_format);
        }

        return net::Result<WireDatagramView>{std::in_place, WireDatagramView{
            .header = WireHeader{
                .kind = static_cast<WireKind>(raw_kind),
                .opcode = packet[4],
                .request_id = load_be16(packet, 6),
                .status = static_cast<WireStatus>(raw_status),
            },
            .payload = packet.subspan(header_size()),
        }};
    }

    template <class Codec, class Value, util::usize Capacity>
    [[nodiscard]] net::Result<void> write_typed_datagram(net::PacketBuffer<Capacity>& packet,
                                                         WireKind kind,
                                                         util::u8 opcode,
                                                         util::u16 request_id,
                                                         WireStatus status,
                                                         const Value& value) noexcept {
        std::array<util::u8, Codec::max_size()> encoded_payload{};
        auto encoded = Codec::encode(
            value,
            net::MutByteView{encoded_payload.data(), encoded_payload.size()});
        if (!encoded) {
            return util::unexpected(encoded.error());
        }

        return write_raw_datagram(
            packet,
            kind,
            opcode,
            request_id,
            status,
            net::ByteView{encoded_payload.data(), encoded.value()});
    }

    template <net::ServiceOperation Op>
    [[nodiscard]] net::Result<typename Op::Request> decode_request(
        const WireDatagramView& datagram) noexcept {
        if (datagram.header.kind != WireKind::request || datagram.header.opcode != Op::opcode) {
            return util::unexpected(net::errc::invalid_format);
        }
        return Op::RequestCodec::decode(datagram.payload.payload);
    }

    template <net::ServiceOperation Op, util::usize Capacity>
    [[nodiscard]] net::Result<void> write_request_datagram(
        net::PacketBuffer<Capacity>& packet,
        util::u16 request_id,
        const typename Op::Request& request) noexcept {
        return write_typed_datagram<typename Op::RequestCodec>(
            packet,
            WireKind::request,
            Op::opcode,
            request_id,
            WireStatus::ok,
            request);
    }

    template <net::ServiceOperation Op, util::usize Capacity>
    [[nodiscard]] net::Result<void> write_response_datagram(
        net::PacketBuffer<Capacity>& packet,
        util::u16 request_id,
        WireStatus status,
        const typename Op::Response& response) noexcept {
        return write_typed_datagram<typename Op::ResponseCodec>(
            packet,
            WireKind::response,
            Op::opcode,
            request_id,
            status,
            response);
    }

    template <util::usize Capacity>
    [[nodiscard]] net::Result<void> write_error_datagram(
        net::PacketBuffer<Capacity>& packet,
        util::u8 opcode,
        util::u16 request_id,
        WireStatus status) noexcept {
        return write_raw_datagram(
            packet,
            WireKind::response,
            opcode,
            request_id,
            status,
            {});
    }

    struct WireTraits {
        using Status = WireStatus;
        using DatagramView = WireDatagramView;

        [[nodiscard]] static constexpr util::usize header_size() noexcept {
            return ::header_size();
        }

        [[nodiscard]] static constexpr WireKind request_kind() noexcept {
            return WireKind::request;
        }

        [[nodiscard]] static constexpr WireKind response_kind() noexcept {
            return WireKind::response;
        }

        [[nodiscard]] static constexpr WireStatus ok_status() noexcept {
            return WireStatus::ok;
        }

        [[nodiscard]] static constexpr WireStatus bad_request_status() noexcept {
            return WireStatus::bad_request;
        }

        [[nodiscard]] static constexpr WireStatus unsupported_status() noexcept {
            return WireStatus::unsupported;
        }

        [[nodiscard]] static constexpr WireStatus internal_error_status() noexcept {
            return WireStatus::internal_error;
        }

        [[nodiscard]] static constexpr net::errc status_error(WireStatus status) noexcept {
            return ::status_error(status);
        }

        [[nodiscard]] static constexpr net::Result<WireDatagramView> parse_datagram(
            net::PacketView packet) noexcept {
            return ::parse_datagram(packet);
        }

        template <net::ServiceOperation Op>
        [[nodiscard]] static net::Result<typename Op::Request> decode_request(
            const WireDatagramView& datagram) noexcept {
            return ::decode_request<Op>(datagram);
        }

        template <net::ServiceOperation Op, util::usize Capacity>
        [[nodiscard]] static net::Result<void> write_request_datagram(
            net::PacketBuffer<Capacity>& packet,
            util::u16 request_id,
            const typename Op::Request& request) noexcept {
            return ::write_request_datagram<Op>(packet, request_id, request);
        }

        template <net::ServiceOperation Op, util::usize Capacity>
        [[nodiscard]] static net::Result<void> write_response_datagram(
            net::PacketBuffer<Capacity>& packet,
            util::u16 request_id,
            WireStatus status,
            const typename Op::Response& response) noexcept {
            return ::write_response_datagram<Op>(packet, request_id, status, response);
        }

        template <util::usize Capacity>
        [[nodiscard]] static net::Result<void> write_error_datagram(
            net::PacketBuffer<Capacity>& packet,
            util::u8 opcode,
            util::u16 request_id,
            WireStatus status) noexcept {
            return ::write_error_datagram(packet, opcode, request_id, status);
        }
    };

    [[nodiscard]] bool same_ipv4(const net::IpAddress& lhs, const net::IpAddress& rhs) noexcept {
        if (!lhs.is_ipv4() || !rhs.is_ipv4()) {
            return false;
        }
        for (util::usize i = 0; i < 4; ++i) {
            if (lhs.bytes[i] != rhs.bytes[i]) {
                return false;
            }
        }
        return true;
    }

    [[nodiscard]] bool same_endpoint(const net::Endpoint& lhs, const net::Endpoint& rhs) noexcept {
        return lhs.port == rhs.port && same_ipv4(lhs.address, rhs.address);
    }

    struct CaptureSink {
        net::Endpoint local{};
        net::Endpoint peer{};
        std::array<util::u8, 64> payload{};
        util::usize size{0};
        bool used{false};

        void clear() noexcept {
            local = {};
            peer = {};
            payload = {};
            size = 0;
            used = false;
        }

        [[nodiscard]] net::ByteView view() const noexcept {
            return net::ByteView{payload.data(), size};
        }

        static net::Result<net::UdpSendDisposition> send(void* ctx,
                                                         net::Endpoint local,
                                                         const net::Endpoint& peer,
                                                         net::ByteView payload) noexcept {
            auto* self = static_cast<CaptureSink*>(ctx);
            if (!self) {
                return util::unexpected(net::errc::bad_state);
            }
            if (payload.size() > self->payload.size()) {
                return util::unexpected(net::errc::buffer_overflow);
            }

            self->local = local;
            self->peer = peer;
            self->size = payload.size();
            self->used = true;
            for (util::usize i = 0; i < payload.size(); ++i) {
                self->payload[i] = payload[i];
            }
            return net::Result<net::UdpSendDisposition>{
                std::in_place,
                net::UdpSendDisposition::transmitted
            };
        }
    };

    template <util::usize Count, util::usize Capacity>
    [[nodiscard]] net::Result<net::OwnedPacket> make_owned_packet(
        net::PacketPool<Count, Capacity>& pool,
        net::ByteView payload) noexcept {
        using Lease = typename net::PacketPool<Count, Capacity>::Lease;

        auto lease = pool.acquire();
        if (!lease) {
            return util::unexpected(lease.error());
        }

        auto appended = lease.value()->append(payload);
        if (!appended) {
            return util::unexpected(appended.error());
        }

        return net::Result<net::OwnedPacket>{
            std::in_place,
            net::OwnedPacket{static_cast<Lease&&>(lease.value())}
        };
    }

    struct ClientState {
        util::u16 echo_request_id{0};
        util::u16 timeout_request_id{0};
        bool echo_ok{false};
        bool timeout_ok{false};
        bool failed{false};

        static void on_echo(void* ctx,
                            util::u16 request_id,
                            WireStatus status,
                            const EchoReply& response) noexcept {
            auto* self = static_cast<ClientState*>(ctx);
            if (!self) {
                return;
            }

            self->echo_ok = request_id == self->echo_request_id
                && status == WireStatus::ok
                && response.value == 22u;
            if (!self->echo_ok) {
                self->failed = true;
            }
        }

        static void on_timeout(void* ctx, util::u16 request_id) noexcept {
            auto* self = static_cast<ClientState*>(ctx);
            if (!self) {
                return;
            }

            self->timeout_ok = request_id == self->timeout_request_id;
            if (!self->timeout_ok) {
                self->failed = true;
            }
        }
    };

    struct ServerState {
        bool echo_called{false};

        static WireStatus on_echo(void* ctx,
                                  const EchoRequest& request,
                                  EchoReply& response) noexcept {
            auto* self = static_cast<ServerState*>(ctx);
            if (!self) {
                return WireStatus::internal_error;
            }

            self->echo_called = request.value == 21u;
            response.value = static_cast<util::u8>(request.value + 1u);
            return self->echo_called ? WireStatus::ok : WireStatus::bad_request;
        }
    };
}

int main() {
    constexpr auto client_local = net::Endpoint::ipv4(10, 1, 1, 10, 4100);
    constexpr auto server_local = net::Endpoint::ipv4(10, 1, 1, 20, 4200);

    net::udp::service::Client<WireTraits, 8, 4> client{};
    net::udp::service::Server<WireTraits, 8, 4> server{};
    net::PacketPool<8, 64> pool{};
    CaptureSink client_tx{};
    CaptureSink server_tx{};
    ClientState client_state{};
    ServerState server_state{};

    client.set_sender(&CaptureSink::send, &client_tx);
    server.set_sender(&CaptureSink::send, &server_tx);

    auto echo_route = server.template set_route<EchoOp>(&ServerState::on_echo, &server_state);
    if (!echo_route || !server.template has_route<EchoOp>() || server.route_count() != 1u) {
        std::fputs("net udp service codec smoke route register failed\n", stderr);
        return 1;
    }

    auto echo_request = client.template send_request<EchoOp>(
        client_local,
        server_local,
        EchoRequest{21u},
        100u,
        25u,
        &ClientState::on_echo,
        nullptr,
        &client_state);
    if (!echo_request
        || !client_tx.used
        || !same_endpoint(client_tx.local, client_local)
        || !same_endpoint(client_tx.peer, server_local)
        || client.request_count() != 1u
        || client.pending_count() != 1u) {
        std::fputs("net udp service codec smoke client send failed\n", stderr);
        return 2;
    }
    client_state.echo_request_id = echo_request.value();

    auto inbound_request = make_owned_packet(pool, client_tx.view());
    if (!inbound_request) {
        std::fputs("net udp service codec smoke request packet acquire failed\n", stderr);
        return 3;
    }
    auto consumed_request = server.consume(
        net::UdpDatagramInfo{
            .local = server_local,
            .peer = client_local,
            .length = static_cast<util::u16>(client_tx.size),
            .checksum = 0,
        },
        static_cast<net::OwnedPacket&&>(inbound_request.value()));
    if (!consumed_request
        || !server_state.echo_called
        || !server_tx.used
        || !same_endpoint(server_tx.local, server_local)
        || !same_endpoint(server_tx.peer, client_local)
        || server.request_count() != 1u
        || server.reply_count() != 1u
        || server.error_reply_count() != 0u) {
        std::fputs("net udp service codec smoke server consume failed\n", stderr);
        return 4;
    }

    auto inbound_reply = make_owned_packet(pool, server_tx.view());
    if (!inbound_reply) {
        std::fputs("net udp service codec smoke reply packet acquire failed\n", stderr);
        return 5;
    }
    auto consumed_reply = client.consume(
        net::UdpDatagramInfo{
            .local = client_local,
            .peer = server_local,
            .length = static_cast<util::u16>(server_tx.size),
            .checksum = 0,
        },
        static_cast<net::OwnedPacket&&>(inbound_reply.value()));
    if (!consumed_reply
        || !client_state.echo_ok
        || client_state.failed
        || client.response_count() != 1u
        || client.pending_count() != 0u
        || client.drop_count() != 0u) {
        std::fputs("net udp service codec smoke client consume failed\n", stderr);
        return 6;
    }

    auto cleared = server.template clear_route<EchoOp>();
    if (!cleared || server.template has_route<EchoOp>() || server.route_count() != 0u) {
        std::fputs("net udp service codec smoke route clear failed\n", stderr);
        return 7;
    }

    server_tx.clear();
    net::PacketBuffer<32> unsupported_request{};
    auto wrote_unsupported = write_request_datagram<EchoOp>(unsupported_request, 0x33u, EchoRequest{9u});
    if (!wrote_unsupported) {
        std::fputs("net udp service codec smoke unsupported request encode failed\n", stderr);
        return 8;
    }
    auto unsupported_packet = make_owned_packet(pool, unsupported_request.view().payload);
    if (!unsupported_packet) {
        std::fputs("net udp service codec smoke unsupported packet acquire failed\n", stderr);
        return 9;
    }
    auto consumed_unsupported = server.consume(
        net::UdpDatagramInfo{
            .local = server_local,
            .peer = client_local,
            .length = static_cast<util::u16>(unsupported_request.size()),
            .checksum = 0,
        },
        static_cast<net::OwnedPacket&&>(unsupported_packet.value()));
    if (!consumed_unsupported || !server_tx.used || server.error_reply_count() != 1u) {
        std::fputs("net udp service codec smoke unsupported consume failed\n", stderr);
        return 10;
    }

    auto unsupported_reply = parse_datagram(make_packet_view(server_tx.view()));
    if (!unsupported_reply
        || unsupported_reply.value().header.kind != WireKind::response
        || unsupported_reply.value().header.opcode != EchoOp::opcode
        || unsupported_reply.value().header.status != WireStatus::unsupported) {
        std::fputs("net udp service codec smoke unsupported reply check failed\n", stderr);
        return 11;
    }

    auto echo_route_again = server.template set_route<EchoOp>(&ServerState::on_echo, &server_state);
    if (!echo_route_again || !server.template has_route<EchoOp>() || server.route_count() != 1u) {
        std::fputs("net udp service codec smoke route re-register failed\n", stderr);
        return 12;
    }

    server_tx.clear();
    net::PacketBuffer<32> malformed_request{};
    auto wrote_malformed = write_raw_datagram(
        malformed_request,
        WireKind::request,
        EchoOp::opcode,
        0x44u,
        WireStatus::ok,
        {});
    if (!wrote_malformed) {
        std::fputs("net udp service codec smoke malformed request encode failed\n", stderr);
        return 13;
    }
    auto malformed_packet = make_owned_packet(pool, malformed_request.view().payload);
    if (!malformed_packet) {
        std::fputs("net udp service codec smoke malformed packet acquire failed\n", stderr);
        return 14;
    }
    auto consumed_malformed = server.consume(
        net::UdpDatagramInfo{
            .local = server_local,
            .peer = client_local,
            .length = static_cast<util::u16>(malformed_request.size()),
            .checksum = 0,
        },
        static_cast<net::OwnedPacket&&>(malformed_packet.value()));
    if (!consumed_malformed || !server_tx.used || server.error_reply_count() != 2u) {
        std::fputs("net udp service codec smoke malformed consume failed\n", stderr);
        return 15;
    }

    auto malformed_reply = parse_datagram(make_packet_view(server_tx.view()));
    if (!malformed_reply
        || malformed_reply.value().header.kind != WireKind::response
        || malformed_reply.value().header.opcode != EchoOp::opcode
        || malformed_reply.value().header.status != WireStatus::bad_request) {
        std::fputs("net udp service codec smoke malformed reply check failed\n", stderr);
        return 16;
    }

    auto timeout_request = client.template send_request<AddOp>(
        client_local,
        server_local,
        AddRequest{3u, 4u},
        200u,
        25u,
        nullptr,
        &ClientState::on_timeout,
        &client_state);
    if (!timeout_request || client.pending_count() != 1u) {
        std::fputs("net udp service codec smoke timeout send failed\n", stderr);
        return 17;
    }
    client_state.timeout_request_id = timeout_request.value();

    client.tick(226u);
    if (!client_state.timeout_ok
        || client.timeout_count() != 1u
        || client.pending_count() != 0u
        || client.last_error() != net::errc::ok
        || server.last_error() != net::errc::ok) {
        std::fputs("net udp service codec smoke timeout check failed\n", stderr);
        return 18;
    }

    std::puts("net udp service codec smoke: ok");
    return 0;
}
