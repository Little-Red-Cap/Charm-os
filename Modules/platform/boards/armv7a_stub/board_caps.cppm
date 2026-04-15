module;

export module platform.board.armv7a_stub;

import platform.board;
import util.core;

namespace platform::board::armv7a_stub::detail {
    inline void* select_hook_ctx(void* default_ctx, void* hook_ctx) noexcept {
        return hook_ctx ? hook_ctx : default_ctx;
    }
}

export namespace platform::board::armv7a_stub {
    struct BootAddressLayout {
        util::usize xip_window_base{0};
        util::usize ram_payload_base{0};
    };

    struct BootTransferHooks {
        void* ctx{nullptr};
        bool (*copy_payload)(void* ctx,
                             const BootLoadTransferRequest& request) noexcept {nullptr};
    };

    struct BootExecHooks {
        void* ctx{nullptr};
        bool (*prepare_jump)(void* ctx,
                             const BootExecRequest& request) noexcept {nullptr};
        bool (*jump)(void* ctx,
                     const BootExecRequest& request) noexcept {nullptr};
    };

    struct BootContext {
        BootAddressLayout layout{};
        BootTransferHooks transfer{};
        BootExecHooks exec{};
    };

    inline util::usize resolve_payload_base(void* ctx,
                                            const BootLoadResolveRequest& request) noexcept {
        if (!ctx) {
            return 0;
        }

        const auto& boot = *static_cast<const BootContext*>(ctx);
        if (request.kind == BootLoadKind::xip) {
            return boot.layout.xip_window_base == 0
                ? 0
                : boot.layout.xip_window_base + request.storage_payload_offset;
        }
        return boot.layout.ram_payload_base;
    }

    inline bool load_payload(void* ctx,
                             const BootLoadTransferRequest& request) noexcept {
        if (!ctx) {
            return false;
        }

        const auto& boot = *static_cast<const BootContext*>(ctx);
        if (request.kind == BootLoadKind::xip) {
            return request.payload_base != 0;
        }
        if (!boot.transfer.copy_payload) {
            return false;
        }

        return boot.transfer.copy_payload(
            detail::select_hook_ctx(ctx, boot.transfer.ctx),
            request);
    }

    inline bool prepare_jump(void* ctx,
                             const BootExecRequest& request) noexcept {
        if (!ctx) {
            return false;
        }

        const auto& boot = *static_cast<const BootContext*>(ctx);
        if (!boot.exec.prepare_jump) {
            return request.payload_base != 0 && request.entry_addr != 0;
        }

        return boot.exec.prepare_jump(
            detail::select_hook_ctx(ctx, boot.exec.ctx),
            request);
    }

    inline bool jump(void* ctx,
                     const BootExecRequest& request) noexcept {
        if (!ctx) {
            return false;
        }

        const auto& boot = *static_cast<const BootContext*>(ctx);
        if (!boot.exec.jump) {
            return false;
        }

        return boot.exec.jump(
            detail::select_hook_ctx(ctx, boot.exec.ctx),
            request);
    }

    inline BootBoardCaps make_boot_caps(BootContext& ctx) noexcept {
        return BootBoardCaps{
            .load = BootLoadDesc{
                .ctx = &ctx,
                .resolve_payload_base = &resolve_payload_base,
                .load_payload = &load_payload
            },
            .exec = BootExecDesc{
                .ctx = &ctx,
                .prepare_jump = &prepare_jump,
                .jump = &jump
            }
        };
    }

    inline BoardCaps make_board_caps(BootContext& ctx) noexcept {
        return with_boot_caps(BoardCaps{}, make_boot_caps(ctx));
    }
}
