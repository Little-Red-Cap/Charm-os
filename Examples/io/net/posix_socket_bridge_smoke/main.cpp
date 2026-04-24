#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <span>
#include <thread>

#ifdef errno
#undef errno
#endif
#ifdef EBADF
#undef EBADF
#endif
#ifdef EAGAIN
#undef EAGAIN
#endif

import net.backend.win;
import net.common;
import net.posix;
import net.stack;
import posix.api;
import posix.errno;
import posix.fd_table;
import posix.file;
import posix.pipe;
import posix.proc;
import posix.user_runtime;
import util.core;

namespace {
    using namespace std::chrono_literals;

    constexpr util::u32 kSocketTypeMask = 0170000u;
    constexpr util::u32 kSocketTypeBits = 0140000u;

    template <class Fn>
    bool wait_until(Fn&& fn, int attempts = 300, std::chrono::milliseconds delay = 2ms) {
        for (int i = 0; i < attempts; ++i) {
            if (fn()) return true;
            std::this_thread::sleep_for(delay);
        }
        return false;
    }

    bool bytes_eq(const char* lhs, const char* rhs, util::usize count) noexcept {
        for (util::usize i = 0; i < count; ++i) {
            if (lhs[i] != rhs[i]) return false;
        }
        return true;
    }

    bool expect_badfd(int rc) noexcept {
        return rc < 0 && posix::get_errno() == posix::EBADF;
    }

    bool parse_fd_arg(const char* text, int& out) noexcept {
        if (!text || text[0] == '\0') return false;
        int value = 0;
        for (const char* cur = text; *cur != '\0'; ++cur) {
            if (*cur < '0' || *cur > '9') return false;
            value = (value * 10) + (*cur - '0');
        }
        out = value;
        return true;
    }

    int socket_stdio_child_main(int argc, char** argv, char**) {
        if (argc != 2 || !argv || !argv[1]) return 41;

        int source_fd = -1;
        if (!parse_fd_arg(argv[1], source_fd)) return 42;

        posix::PosixStat socket_stat{};
        if (posix::user::fstat(0, &socket_stat) != 0) return 43;
        if ((socket_stat.mode & kSocketTypeMask) != kSocketTypeBits) return 44;
        if (posix::user::fstat(1, &socket_stat) != 0) return 45;
        if ((socket_stat.mode & kSocketTypeMask) != kSocketTypeBits) return 46;
        if (posix::user::fcntl(0, posix::F_GETFD) != 0) return 47;
        if (posix::user::fcntl(1, posix::F_GETFD) != 0) return 48;

        posix::set_errno(0);
        if (posix::user::fstat(source_fd, &socket_stat) != -1 || posix::get_errno() != posix::EBADF) return 49;

        char rx[8]{};
        const auto received = posix::user::read(0, rx, sizeof(rx));
        if (received != 4 || !bytes_eq(rx, "ping", 4)) return 50;

        const char pong[4]{'p', 'o', 'n', 'g'};
        if (posix::user::write(1, pong, 4) != 4) return 51;
        return 0;
    }
}

int main() {
    using FdTableType = posix::FdTable<32>;
    using FileServiceType = posix::FileService<4>;
    using PipeServiceType = posix::PipeService<4, 64>;
    using ProcServiceType = posix::ProcService<4, 4, 32, 4>;
    using SocketServiceType = posix::SocketService<16>;
    using ApiType = posix::Api<32, 4, 64, 4, 4, 4, 128, 16, 16, 256, 4096, 4096, 4, 32, 16>;

    net::backend::WinProvider<32> provider{};
    net::Stack stack{provider};

    FdTableType fds{};
    FileServiceType files{};
    PipeServiceType pipes{};
    ProcServiceType procs{};
    SocketServiceType sockets{};
    fds.init();
    files.init();
    pipes.init();
    procs.init();
    sockets.init();
    sockets.bind_stack(stack);

    ApiType api{fds, files, pipes, procs, &sockets};
    posix::user::ProcessBinding<ApiType> runtime_binding{api};
    posix::user::bind_process_runtime(procs, runtime_binding);

    if (!procs.register_executable("socket-stdio", &socket_stdio_child_main)) {
        std::fputs("posix socket stdio child register failed\n", stderr);
        return 1;
    }

    int listener = api.socket(posix::AF_INET, posix::SOCK_STREAM);
    if (listener < 0) {
        std::fputs("posix tcp socket failed\n", stderr);
        return 2;
    }

    util::u16 tcp_port = 0;
    for (util::u16 port = 27000; port < 27100; ++port) {
        if (api.bind(listener, net::Endpoint::ipv4_loopback(port)) != 0) continue;
        if (api.listen(listener, 2) != 0) {
            std::fputs("posix tcp listen failed\n", stderr);
            return 2;
        }
        tcp_port = port;
        break;
    }
    if (tcp_port == 0) {
        std::fputs("posix tcp bind failed\n", stderr);
        return 3;
    }

    posix::PosixStat socket_stat{};
    if (api.fstat(listener, &socket_stat) != 0) {
        std::fputs("posix socket fstat failed\n", stderr);
        return 5;
    }
    if ((socket_stat.mode & kSocketTypeMask) != kSocketTypeBits) {
        std::fputs("posix socket fstat type mismatch\n", stderr);
        return 6;
    }

    constexpr int duplicated_listener_fd = 15;
    if (api.dup2(listener, duplicated_listener_fd) != duplicated_listener_fd) {
        std::fputs("posix socket dup2 failed\n", stderr);
        return 7;
    }
    if (api.close(listener) != 0) {
        std::fputs("posix original listener close failed\n", stderr);
        return 8;
    }

    int client = api.socket(posix::AF_INET, posix::SOCK_STREAM);
    if (client < 0) {
        std::fputs("posix client socket failed\n", stderr);
        return 9;
    }
    if (api.connect(client, net::Endpoint::ipv4_loopback(tcp_port)) != 0) {
        std::fputs("posix tcp connect failed\n", stderr);
        return 10;
    }

    net::Endpoint accepted_peer{};
    int accepted = -1;
    if (!wait_until([&]() {
        accepted = api.accept(duplicated_listener_fd, &accepted_peer);
        return accepted >= 0;
    })) {
        std::fputs("posix tcp accept timeout\n", stderr);
        return 11;
    }
    if (accepted < 0) {
        std::fputs("posix tcp accept failed\n", stderr);
        return 12;
    }
    if (accepted_peer.port == 0) {
        std::fputs("posix tcp accept peer invalid\n", stderr);
        return 13;
    }

    const int accepted_dup = api.dup(accepted);
    if (accepted_dup < 0) {
        std::fputs("posix accepted dup failed\n", stderr);
        return 14;
    }
    if (api.close(accepted) != 0) {
        std::fputs("posix accepted close after dup failed\n", stderr);
        return 15;
    }
    if (api.fstat(accepted_dup, &socket_stat) != 0
        || (socket_stat.mode & kSocketTypeMask) != kSocketTypeBits) {
        std::fputs("posix accepted dup fstat failed\n", stderr);
        return 16;
    }

    if (api.fcntl(accepted_dup, posix::F_SETFD, posix::FD_CLOEXEC) != 0
        || api.fcntl(accepted_dup, posix::F_GETFD) != posix::FD_CLOEXEC) {
        std::fputs("posix accepted dup cloexec failed\n", stderr);
        return 17;
    }

    const char ping[4]{'p', 'i', 'n', 'g'};
    if (!wait_until([&]() {
        const auto written = api.write(client, ping, 4);
        return written == 4;
    })) {
        std::fputs("posix spawn ping write timeout\n", stderr);
        return 18;
    }

    char source_fd_arg[16]{};
    std::snprintf(source_fd_arg, sizeof(source_fd_arg), "%d", accepted_dup);
    const char* child_argv[] = {"socket-stdio", source_fd_arg, nullptr};
    posix::SpawnConfig child_cfg{};
    child_cfg.path = "socket-stdio";
    child_cfg.argv = std::span<const char* const>(child_argv, 2);
    child_cfg.stdio_in = accepted_dup;
    child_cfg.stdio_out = accepted_dup;

    const int child_pid = api.spawn(child_cfg);
    if (child_pid <= 0) {
        std::fputs("posix socket stdio spawn failed\n", stderr);
        return 19;
    }

    int child_status = -1;
    if (api.waitpid(posix::ProcessId{child_pid}, &child_status, 0) != child_pid || child_status != 0) {
        std::fputs("posix socket stdio waitpid failed\n", stderr);
        return 20;
    }

    char tcp_rx[8]{};
    bool child_pong_ok = false;
    if (!wait_until([&]() {
        const auto read = api.read(client, tcp_rx, sizeof(tcp_rx));
        if (read >= 0) {
            child_pong_ok = read == 4 && bytes_eq(tcp_rx, "pong", 4);
            return true;
        }
        return false;
    })) {
        std::fputs("posix socket stdio pong timeout\n", stderr);
        return 21;
    }
    if (!child_pong_ok) {
        std::fputs("posix socket stdio pong mismatch\n", stderr);
        return 22;
    }

    if (api.fcntl(accepted_dup, posix::F_GETFD) != posix::FD_CLOEXEC
        || api.fstat(accepted_dup, &socket_stat) != 0
        || (socket_stat.mode & kSocketTypeMask) != kSocketTypeBits) {
        std::fputs("posix parent socket after spawn mismatch\n", stderr);
        return 23;
    }

    if (!wait_until([&]() {
        const auto written = api.write(client, ping, 4);
        return written == 4;
    })) {
        std::fputs("posix tcp write timeout\n", stderr);
        return 24;
    }

    bool tcp_recv_ok = false;
    if (!wait_until([&]() {
        const auto received = api.recv(accepted_dup, tcp_rx, sizeof(tcp_rx));
        if (received >= 0) {
            tcp_recv_ok = received == 4 && bytes_eq(tcp_rx, ping, 4);
            return true;
        }
        return false;
    })) {
        std::fputs("posix tcp recv timeout\n", stderr);
        return 25;
    }
    if (!tcp_recv_ok) {
        std::fputs("posix tcp recv mismatch\n", stderr);
        return 26;
    }

    const char pong[4]{'p', 'o', 'n', 'g'};
    if (!wait_until([&]() {
        const auto sent = api.send(accepted_dup, pong, 4);
        return sent == 4;
    })) {
        std::fputs("posix tcp send timeout\n", stderr);
        return 27;
    }

    bool tcp_read_ok = false;
    if (!wait_until([&]() {
        const auto read = api.read(client, tcp_rx, sizeof(tcp_rx));
        if (read >= 0) {
            tcp_read_ok = read == 4 && bytes_eq(tcp_rx, pong, 4);
            return true;
        }
        return false;
    })) {
        std::fputs("posix tcp read timeout\n", stderr);
        return 28;
    }
    if (!tcp_read_ok) {
        std::fputs("posix tcp read mismatch\n", stderr);
        return 29;
    }

    if (api.shutdown(client, net::ShutdownMode::both) != 0) {
        std::fputs("posix tcp shutdown failed\n", stderr);
        return 30;
    }

    if (api.close(client) != 0) {
        std::fputs("posix tcp client close failed\n", stderr);
        return 31;
    }

    bool tcp_eof_ok = false;
    if (!wait_until([&]() {
        const auto read = api.read(accepted_dup, tcp_rx, sizeof(tcp_rx));
        if (read == 0) {
            tcp_eof_ok = true;
            return true;
        }
        if (read < 0 && posix::get_errno() == posix::EAGAIN) {
            return false;
        }
        return true;
    })) {
        std::fputs("posix tcp eof timeout\n", stderr);
        return 32;
    }
    if (!tcp_eof_ok) {
        std::fputs("posix tcp eof mismatch\n", stderr);
        return 33;
    }

    if (api.close(accepted_dup) != 0) {
        std::fputs("posix accepted dup close failed\n", stderr);
        return 34;
    }
    if (!expect_badfd(api.fstat(accepted_dup, &socket_stat))) {
        std::fputs("posix closed socket fstat contract failed\n", stderr);
        return 35;
    }
    if (!expect_badfd(api.fstat(-1, &socket_stat))) {
        std::fputs("posix invalid socket fstat contract failed\n", stderr);
        return 36;
    }

    int udp_a = api.socket(posix::AF_INET, posix::SOCK_DGRAM);
    int udp_b = api.socket(posix::AF_INET, posix::SOCK_DGRAM);
    if (udp_a < 0 || udp_b < 0) {
        std::fputs("posix udp socket failed\n", stderr);
        return 37;
    }

    util::u16 udp_a_port = 0;
    util::u16 udp_b_port = 0;
    for (util::u16 port = 28000; port < 28100; ++port) {
        if (api.bind(udp_a, net::Endpoint::ipv4_loopback(port)) != 0) continue;
        udp_a_port = port;
        break;
    }
    if (udp_a_port == 0) {
        std::fputs("posix udp a bind failed\n", stderr);
        return 38;
    }

    for (util::u16 port = static_cast<util::u16>(udp_a_port + 1); port < 28150; ++port) {
        if (api.bind(udp_b, net::Endpoint::ipv4_loopback(port)) != 0) continue;
        udp_b_port = port;
        break;
    }
    if (udp_b_port == 0) {
        std::fputs("posix udp b bind failed\n", stderr);
        return 39;
    }

    const auto udp_sent = api.sendto(udp_a, ping, 4, net::Endpoint::ipv4_loopback(udp_b_port));
    if (udp_sent != 4) {
        std::fputs("posix udp sendto failed\n", stderr);
        return 40;
    }

    bool udp_recv_ok = false;
    net::Endpoint udp_peer{};
    if (!wait_until([&]() {
        const auto received = api.recvfrom(udp_b, tcp_rx, sizeof(tcp_rx), &udp_peer);
        if (received >= 0) {
            udp_recv_ok = received == 4 && udp_peer.port == udp_a_port && bytes_eq(tcp_rx, ping, 4);
            return true;
        }
        return false;
    })) {
        std::fputs("posix udp recvfrom timeout\n", stderr);
        return 41;
    }
    if (!udp_recv_ok) {
        std::fputs("posix udp recvfrom mismatch\n", stderr);
        return 42;
    }

    if (api.close(duplicated_listener_fd) != 0
        || api.close(udp_a) != 0
        || api.close(udp_b) != 0) {
        std::fputs("posix socket close failed\n", stderr);
        return 43;
    }

    std::puts("net posix socket bridge smoke: ok");
    return 0;
}
