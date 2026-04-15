#include <array>
#include <cstdio>

import net.packet;
import util.core;

namespace {
    bool bytes_eq(net::ByteView lhs, net::ByteView rhs) noexcept {
        if (lhs.size() != rhs.size()) {
            return false;
        }
        for (util::usize i = 0; i < lhs.size(); ++i) {
            if (lhs[i] != rhs[i]) {
                return false;
            }
        }
        return true;
    }
}

int main() {
    net::PacketBuffer<32> packet{};
    auto reset = packet.reset(8);
    if (!reset || packet.headroom() != 8 || packet.size() != 0 || packet.tailroom() != 24) {
        std::fputs("packet buffer reset failed\n", stderr);
        return 1;
    }

    static constexpr util::u8 payload[]{'d', 'a', 't', 'a'};
    auto appended = packet.append(net::ByteView{payload, sizeof(payload)});
    if (!appended || packet.size() != sizeof(payload)) {
        std::fputs("packet buffer append failed\n", stderr);
        return 2;
    }

    static constexpr util::u8 header[]{0x08u, 0x00u};
    auto prepended = packet.prepend(net::ByteView{header, sizeof(header)});
    if (!prepended || packet.headroom() != 6 || packet.size() != sizeof(header) + sizeof(payload)) {
        std::fputs("packet buffer prepend failed\n", stderr);
        return 3;
    }

    static constexpr util::u8 wire[]{0x08u, 0x00u, 'd', 'a', 't', 'a'};
    if (!bytes_eq(packet.view().payload, net::ByteView{wire, sizeof(wire)})) {
        std::fputs("packet buffer wire mismatch\n", stderr);
        return 4;
    }

    auto trimmed_front = packet.trim_front(sizeof(header));
    auto trimmed_back = packet.trim_back(1);
    static constexpr util::u8 trimmed[]{'d', 'a', 't'};
    if (!trimmed_front || !trimmed_back
        || !bytes_eq(packet.view().payload, net::ByteView{trimmed, sizeof(trimmed)})) {
        std::fputs("packet buffer trim failed\n", stderr);
        return 5;
    }

    std::array<util::u8, 9> overflow_bytes{};
    auto overflow = packet.prepend(net::ByteView{overflow_bytes.data(), overflow_bytes.size()});
    if (overflow || overflow.error() != net::errc::buffer_overflow) {
        std::fputs("packet buffer overflow check failed\n", stderr);
        return 6;
    }

    net::PacketPool<2, 32> pool{};
    auto lease1 = pool.acquire(4);
    auto lease2 = pool.acquire();
    if (!lease1 || !lease2 || pool.in_use_count() != 2 || pool.free_count() != 0) {
        std::fputs("packet pool acquire failed\n", stderr);
        return 7;
    }

    auto exhausted = pool.acquire();
    if (exhausted || exhausted.error() != net::errc::busy) {
        std::fputs("packet pool exhaustion check failed\n", stderr);
        return 8;
    }

    auto moved = static_cast<net::PacketPool<2, 32>::Lease&&>(lease1.value());
    if (!moved.valid() || pool.in_use_count() != 2) {
        std::fputs("packet lease move failed\n", stderr);
        return 9;
    }

    moved.release();
    if (pool.in_use_count() != 1 || pool.free_count() != 1) {
        std::fputs("packet lease release failed\n", stderr);
        return 10;
    }

    {
        auto owned_lease = pool.acquire(1);
        if (!owned_lease) {
            std::fputs("owned packet acquire failed\n", stderr);
            return 11;
        }
        auto owned_append = owned_lease.value()->append(net::ByteView{payload, sizeof(payload)});
        if (!owned_append) {
            std::fputs("owned packet append failed\n", stderr);
            return 12;
        }

        net::OwnedPacket owned{static_cast<net::PacketPool<2, 32>::Lease&&>(owned_lease.value())};
        if (!owned.valid() || !owned.owns_storage()
            || !bytes_eq(owned.view().payload, net::ByteView{payload, sizeof(payload)})) {
            std::fputs("owned packet view failed\n", stderr);
            return 13;
        }
        if (pool.in_use_count() != 2) {
            std::fputs("owned packet did not retain lease\n", stderr);
            return 14;
        }
    }
    if (pool.in_use_count() != 1 || pool.free_count() != 1) {
        std::fputs("owned packet release failed\n", stderr);
        return 15;
    }

    auto reacquired = pool.acquire(2);
    if (!reacquired || pool.in_use_count() != 2) {
        std::fputs("packet pool reacquire failed\n", stderr);
        return 16;
    }

    std::puts("net packet pool smoke: ok");
    return 0;
}
