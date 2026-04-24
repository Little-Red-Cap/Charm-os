module;

#include <array>

export module net.posix;

import net.common;
import net.socket;
import net.backend.stub;
import posix.fd_table;
import util.core;
import util.error;
import util.expected;

export namespace posix {
    inline constexpr int AF_INET = 2;
    inline constexpr int SOCK_STREAM = 1;
    inline constexpr int SOCK_DGRAM = 2;

    [[nodiscard]] constexpr net::ByteView to_net_byte_view(ByteView view) noexcept {
        return net::ByteView{view.data(), view.size()};
    }

    [[nodiscard]] constexpr net::MutByteView to_net_mut_byte_view(MutByteView view) noexcept {
        return net::MutByteView{view.data(), view.size()};
    }

    template <util::usize MaxSockets>
    class SocketService {
    public:
        void init() noexcept {
            provider_ = {};
            for (util::usize i = 0; i < handles_.size(); ++i) {
                handles_[i] = {};
                used_[i] = false;
            }
        }

        void bind_provider(net::SocketProviderRef provider) noexcept {
            provider_ = provider;
        }

        template <typename T>
        void bind_stack(const T& stack) noexcept {
            bind_provider(stack.provider());
        }

        [[nodiscard]] bool ready() const noexcept {
            return provider_.valid();
        }

        [[nodiscard]] util::Result<FdEntry> socket(int domain, int type, int protocol = 0) noexcept {
            if (!ready()) {
                return util::unexpected(util::Errc::nosys);
            }
            if (domain != AF_INET) {
                return util::unexpected(util::Errc::not_supported);
            }

            const auto kind = map_socket_kind(type, protocol);
            if (!kind) {
                return util::unexpected(kind.error());
            }

            auto* handle = allocate_handle();
            if (!handle) {
                return util::unexpected(util::Errc::buffer_overflow);
            }

            handle->owner = this;
            handle->refs = 1;
            handle->kind = kind.value();

            auto opened = handle->socket.open(provider_, handle->kind);
            if (!opened) {
                release_handle(handle);
                return util::unexpected(opened.error());
            }

            return make_entry(*handle);
        }

        [[nodiscard]] util::Result<void> bind(FdEntry& entry, const net::Endpoint& ep) noexcept {
            auto* handle = handle_from_entry(entry);
            if (!handle) {
                return util::unexpected(util::Errc::invalid_arg);
            }
            return handle->socket.bind(ep);
        }

        [[nodiscard]] util::Result<void> connect(FdEntry& entry, const net::Endpoint& ep) noexcept {
            auto* handle = handle_from_entry(entry);
            if (!handle) {
                return util::unexpected(util::Errc::invalid_arg);
            }
            return handle->socket.connect(ep);
        }

        [[nodiscard]] util::Result<void> listen(FdEntry& entry, util::u16 backlog) noexcept {
            auto* handle = handle_from_entry(entry);
            if (!handle) {
                return util::unexpected(util::Errc::invalid_arg);
            }
            return handle->socket.listen(backlog);
        }

        template <util::usize MaxFds>
        [[nodiscard]] util::Result<int> accept(FdTable<MaxFds>& table,
                                               FdEntry& entry,
                                               net::Endpoint* peer = nullptr) noexcept {
            auto* handle = handle_from_entry(entry);
            if (!handle) {
                return util::unexpected(util::Errc::invalid_arg);
            }

            auto* accepted = allocate_handle();
            if (!accepted) {
                return util::unexpected(util::Errc::buffer_overflow);
            }

            accepted->owner = this;
            accepted->refs = 1;
            accepted->kind = net::SocketKind::tcp;

            auto ok = handle->socket.accept(accepted->socket, peer);
            if (!ok) {
                release_handle(accepted);
                return util::unexpected(ok.error());
            }

            auto rfd = table.attach(make_entry(*accepted));
            if (!rfd) {
                (void)accepted->socket.close();
                release_handle(accepted);
                return util::unexpected(rfd.error());
            }
            return rfd.value();
        }

        [[nodiscard]] util::Result<util::usize> send(FdEntry& entry, ByteView buf) noexcept {
            auto* handle = handle_from_entry(entry);
            if (!handle) {
                return util::unexpected(util::Errc::invalid_arg);
            }
            return handle->socket.send(to_net_byte_view(buf));
        }

        [[nodiscard]] util::Result<util::usize> recv(FdEntry& entry, MutByteView buf) noexcept {
            auto* handle = handle_from_entry(entry);
            if (!handle) {
                return util::unexpected(util::Errc::invalid_arg);
            }
            return bridge_stream_recv(*handle, to_net_mut_byte_view(buf));
        }

        [[nodiscard]] util::Result<util::usize> sendto(FdEntry& entry,
                                                       const net::Endpoint& peer,
                                                       ByteView buf) noexcept {
            auto* handle = handle_from_entry(entry);
            if (!handle) {
                return util::unexpected(util::Errc::invalid_arg);
            }
            return handle->socket.send_to(peer, to_net_byte_view(buf));
        }

        [[nodiscard]] util::Result<util::usize> recvfrom(FdEntry& entry,
                                                         net::Endpoint* peer,
                                                         MutByteView buf) noexcept {
            auto* handle = handle_from_entry(entry);
            if (!handle) {
                return util::unexpected(util::Errc::invalid_arg);
            }
            return handle->socket.recv_from(peer, to_net_mut_byte_view(buf));
        }

        [[nodiscard]] util::Result<void> shutdown(FdEntry& entry,
                                                  net::ShutdownMode mode = net::ShutdownMode::both) noexcept {
            auto* handle = handle_from_entry(entry);
            if (!handle) {
                return util::unexpected(util::Errc::invalid_arg);
            }
            return handle->socket.shutdown(mode);
        }

    private:
        struct Handle {
            net::Socket socket{};
            SocketService* owner{nullptr};
            net::SocketKind kind{net::SocketKind::tcp};
            util::u32 refs{0};
        };

        [[nodiscard]] static util::Result<net::SocketKind> map_socket_kind(int type, int protocol) noexcept {
            (void)protocol;
            switch (type) {
                case SOCK_STREAM:
                    return net::SocketKind::tcp;
                case SOCK_DGRAM:
                    return net::SocketKind::udp;
                default:
                    return util::unexpected(util::Errc::not_supported);
            }
        }

        [[nodiscard]] Handle* allocate_handle() noexcept {
            for (util::usize i = 0; i < handles_.size(); ++i) {
                if (used_[i]) continue;
                used_[i] = true;
                handles_[i] = {};
                return &handles_[i];
            }
            return nullptr;
        }

        void release_handle(Handle* handle) noexcept {
            if (!handle) return;
            const auto index = static_cast<util::usize>(handle - handles_.data());
            if (index >= handles_.size()) return;
            handles_[index] = {};
            used_[index] = false;
        }

        [[nodiscard]] FdEntry make_entry(Handle& handle) noexcept {
            FdEntry entry{};
            entry.kind = FdKind::socket;
            entry.flags = FdFlags::read_write | FdFlags::non_block;
            entry.ops = &SocketService::ops();
            entry.ctx = &handle;
            entry.inheritable = true;
            return entry;
        }

        [[nodiscard]] Handle* handle_from_entry(FdEntry& entry) noexcept {
            if (entry.kind != FdKind::socket) return nullptr;
            if (entry.ops != &SocketService::ops()) return nullptr;
            auto* handle = static_cast<Handle*>(entry.ctx);
            if (!handle || handle->owner != this || handle->refs == 0) return nullptr;
            return handle;
        }

        [[nodiscard]] static Handle* handle_from_ctx(void* ctx) noexcept {
            auto* handle = static_cast<Handle*>(ctx);
            if (!handle || !handle->owner || handle->refs == 0) {
                return nullptr;
            }
            return handle;
        }

        [[nodiscard]] static util::Result<util::usize> bridge_stream_recv(Handle& handle,
                                                                          net::MutByteView buf) noexcept {
            auto received = handle.socket.recv(buf);
            if (!received) {
                if (handle.kind == net::SocketKind::tcp && received.error() == util::Errc::closed) {
                    return util::unexpected(util::Errc::end_of_stream);
                }
                return util::unexpected(received.error());
            }
            return received;
        }

        static util::Result<util::usize> read(void* ctx, MutByteView buf) noexcept {
            auto* handle = handle_from_ctx(ctx);
            if (!handle) {
                return util::unexpected(util::Errc::invalid_arg);
            }
            return bridge_stream_recv(*handle, to_net_mut_byte_view(buf));
        }

        static util::Result<util::usize> write(void* ctx, ByteView buf) noexcept {
            auto* handle = handle_from_ctx(ctx);
            if (!handle) {
                return util::unexpected(util::Errc::invalid_arg);
            }
            return handle->socket.send(to_net_byte_view(buf));
        }

        static util::Result<void> close(void* ctx) noexcept {
            auto* handle = handle_from_ctx(ctx);
            if (!handle) {
                return util::unexpected(util::Errc::invalid_arg);
            }
            if (handle->refs > 1) {
                --handle->refs;
                return {};
            }
            auto closed = handle->socket.close();
            if (!closed) {
                return util::unexpected(closed.error());
            }
            auto* owner = handle->owner;
            owner->release_handle(handle);
            return {};
        }

        static util::Result<void> stat(void* ctx, PosixStat& out) noexcept {
            auto* handle = handle_from_ctx(ctx);
            if (!handle) {
                return util::unexpected(util::Errc::invalid_arg);
            }
            out.mode = make_stat_mode(S_IFSOCK, kModePermChar);
            out.size = 0;
            return {};
        }

        static util::Result<void> dup(void* ctx) noexcept {
            auto* handle = handle_from_ctx(ctx);
            if (!handle) {
                return util::unexpected(util::Errc::invalid_arg);
            }
            ++handle->refs;
            return {};
        }

        static const FdOps& ops() noexcept {
            static const FdOps kOps{
                &SocketService::read,
                &SocketService::write,
                &SocketService::close,
                &SocketService::stat,
                &SocketService::dup,
                nullptr
            };
            return kOps;
        }

        net::SocketProviderRef provider_{};
        std::array<Handle, MaxSockets> handles_{}; 
        std::array<bool, MaxSockets> used_{}; 
    };
}

#ifndef NDEBUG
namespace posix {
    inline bool net_posix_self_check() noexcept {
        net::backend::StubProvider<8, 64, 64, 4> provider{};
        SocketService<8> service{};
        service.init();
        service.bind_provider(net::make_socket_provider_ref(provider));

        auto listener = service.socket(AF_INET, SOCK_STREAM);
        auto client = service.socket(AF_INET, SOCK_STREAM);
        if (!listener || !client) return false;
        if (!service.bind(listener.value(), net::Endpoint::ipv4_loopback(33333))) return false;
        if (!service.listen(listener.value(), 2)) return false;
        if (!service.connect(client.value(), net::Endpoint::ipv4_loopback(33333))) return false;

        FdTable<8> table{};
        table.init();
        auto lfd = table.attach(listener.value());
        auto cfd = table.attach(client.value());
        if (!lfd || !cfd) return false;

        net::Endpoint peer{};
        auto afd = service.accept(table, *table.get(lfd.value()).value(), &peer);
        if (!afd) return false;

        util::u8 tx[4]{1, 2, 3, 4};
        util::u8 rx[4]{};
        auto* centry = table.get(cfd.value()).value();
        auto* aentry = table.get(afd.value()).value();
        if (!service.send(*centry, ByteView{tx, 4})) return false;
        if (!service.recv(*aentry, MutByteView{rx, 4})) return false;
        return rx[0] == 1 && rx[3] == 4;
    }
}
#endif
