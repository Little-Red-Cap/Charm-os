//
// Created by Joho on 2026/03/05.
//

module;
#include <span>

export module canopen.transport;

import util.core;
import util.error;
import canopen.types;

export namespace canopen {
    struct TransportOps {
        util::Result<util::usize> (*recv)(void* ctx, std::span<CanFrame> out) noexcept { nullptr };
        util::Result<util::usize> (*send)(void* ctx, std::span<const CanFrame> in) noexcept { nullptr };
    };

    struct Transport {
        void* ctx{nullptr};
        TransportOps ops{};

        util::Result<util::usize> recv(std::span<CanFrame> out) noexcept {
            if (!ops.recv) return util::unexpected(util::Errc::not_supported);
            return ops.recv(ctx, out);
        }

        util::Result<util::usize> send(std::span<const CanFrame> in) noexcept {
            if (!ops.send) return util::unexpected(util::Errc::not_supported);
            return ops.send(ctx, in);
        }
    };
} // namespace canopen
