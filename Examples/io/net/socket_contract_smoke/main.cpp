#include <cstdio>

import charm.net;
import net.backend.stub;
import net.backend.win;
import util.core;

namespace {
    [[nodiscard]] net::Endpoint ipv6_endpoint(util::u16 port) noexcept {
        net::Endpoint ep{};
        ep.address.family = net::AddressFamily::ipv6;
        ep.address.bytes[15] = 1;
        ep.port = port;
        return ep;
    }

    template <class ResultLike>
    bool expect_ok(ResultLike&& result, const char* suite, const char* step) {
        if (result) {
            return true;
        }
        std::fprintf(stderr, "%s: %s failed (%d)\n", suite, step, static_cast<int>(result.error()));
        return false;
    }

    template <class ResultLike>
    bool expect_error(ResultLike&& result, net::errc expected, const char* suite, const char* step) {
        if (result) {
            std::fprintf(stderr, "%s: %s unexpectedly succeeded\n", suite, step);
            return false;
        }
        if (result.error() != expected) {
            std::fprintf(stderr,
                         "%s: %s expected %d but got %d\n",
                         suite,
                         step,
                         static_cast<int>(expected),
                         static_cast<int>(result.error()));
            return false;
        }
        return true;
    }

    template <class Provider>
    bool run_socket_contract_suite(const char* suite, Provider& provider) {
        auto ref = net::make_socket_provider_ref(provider);
        util::u8 byte{0x2A};
        util::u8 rx[4]{};

        net::Socket listener{};
        if (!expect_ok(listener.open(ref, net::SocketKind::tcp), suite, "tcp open")) return false;
        if (!expect_error(listener.listen(1), net::errc::bad_state, suite, "tcp listen before bind")) return false;
        if (!expect_error(listener.send(net::ByteView{&byte, 1}), net::errc::bad_state, suite, "tcp send before connect")) return false;
        if (!expect_error(listener.recv_from(nullptr, net::MutByteView{rx, 1}), net::errc::not_supported, suite, "tcp recv_from")) return false;
        if (!expect_error(listener.bind(net::Endpoint{}), net::errc::invalid_arg, suite, "tcp bind unspecified")) return false;
        if (!expect_error(listener.bind(ipv6_endpoint(33000)), net::errc::not_supported, suite, "tcp bind ipv6")) return false;
        if (!expect_ok(listener.bind(net::Endpoint::ipv4_loopback(0)), suite, "tcp bind ephemeral")) return false;
        if (!expect_error(listener.bind(net::Endpoint::ipv4_loopback(33001)), net::errc::bad_state, suite, "tcp rebind")) return false;
        if (!expect_ok(listener.listen(1), suite, "tcp listen")) return false;
        net::Socket accepted{};
        if (!expect_error(listener.accept(accepted, nullptr), net::errc::would_block, suite, "tcp accept empty")) return false;
        if (!expect_ok(listener.close(), suite, "tcp close")) return false;

        net::Socket connector{};
        if (!expect_ok(connector.open(ref, net::SocketKind::tcp), suite, "tcp connector open")) return false;
        if (!expect_error(connector.connect(net::Endpoint::ipv4_any(33000)),
                          net::errc::invalid_arg,
                          suite,
                          "tcp connect any")) return false;
        if (!expect_error(connector.connect(net::Endpoint::ipv4_loopback(0)),
                          net::errc::invalid_arg,
                          suite,
                          "tcp connect zero port")) return false;
        if (!expect_error(connector.connect(ipv6_endpoint(33000)),
                          net::errc::not_supported,
                          suite,
                          "tcp connect ipv6")) return false;
        if (!expect_ok(connector.close(), suite, "tcp connector close")) return false;

        net::Socket udp{};
        if (!expect_ok(udp.open(ref, net::SocketKind::udp), suite, "udp open")) return false;
        if (!expect_error(udp.listen(1), net::errc::not_supported, suite, "udp listen")) return false;
        if (!expect_error(udp.send(net::ByteView{&byte, 1}), net::errc::bad_state, suite, "udp send before connect")) return false;
        if (!expect_error(udp.send_to(net::Endpoint::ipv4_any(34000), net::ByteView{&byte, 1}),
                          net::errc::invalid_arg,
                          suite,
                          "udp send_to any")) return false;
        if (!expect_error(udp.send_to(ipv6_endpoint(34000), net::ByteView{&byte, 1}),
                          net::errc::not_supported,
                          suite,
                          "udp send_to ipv6")) return false;
        if (!expect_ok(udp.bind(net::Endpoint::ipv4_loopback(0)), suite, "udp bind ephemeral")) return false;
        if (!expect_error(udp.recv(net::MutByteView{rx, 1}), net::errc::would_block, suite, "udp recv empty")) return false;
        if (!expect_ok(udp.close(), suite, "udp close")) return false;

        return true;
    }
}

int main() {
    net::backend::StubProvider<16> stub{};
    net::backend::WinProvider<16> win{};

    if (!run_socket_contract_suite("stub", stub)) {
        return 1;
    }
    if (!run_socket_contract_suite("win", win)) {
        return 2;
    }

    std::puts("net socket contract smoke: ok");
    return 0;
}
