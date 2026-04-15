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

    struct BootExceptionLayout {
        util::usize vector_base{0};
    };

    struct BootTranslationLayout {
        util::usize translation_table_base{0};
    };

    struct BootContext;

    struct BootPrepareContext {
        const BootContext* boot{nullptr};
        const BootExecRequest* request{nullptr};

        constexpr explicit operator bool() const noexcept {
            return boot && request;
        }

        const BootExecRequest& exec() const noexcept;
        util::usize vector_base() const noexcept;
        util::usize translation_table_base() const noexcept;
    };

    struct BootTransferHooks {
        void* ctx{nullptr};
        bool (*copy_payload)(void* ctx,
                             const BootLoadTransferRequest& request) noexcept {nullptr};
    };

    struct BootPrepareHooks {
        void* ctx{nullptr};
        bool (*mask_cpu_exceptions)(void* ctx,
                                    const BootPrepareContext& request) noexcept {nullptr};
        bool (*quiesce_interrupt_controller)(void* ctx,
                                             const BootPrepareContext& request) noexcept {nullptr};
        bool (*activate_payload_mapping)(void* ctx,
                                         const BootPrepareContext& request) noexcept {nullptr};
        bool (*clean_data_cache)(void* ctx,
                                 const BootPrepareContext& request) noexcept {nullptr};
        bool (*invalidate_instruction_cache)(void* ctx,
                                             const BootPrepareContext& request) noexcept {nullptr};
        bool (*invalidate_tlb)(void* ctx,
                               const BootPrepareContext& request) noexcept {nullptr};
        bool (*switch_exception_vectors)(void* ctx,
                                         const BootPrepareContext& request) noexcept {nullptr};
        bool (*sync_context)(void* ctx,
                             const BootPrepareContext& request) noexcept {nullptr};
    };

    struct BootPreparePolicy {
        bool mask_cpu_exceptions{true};
        bool quiesce_interrupt_controller{true};
        bool activate_payload_mapping{true};
        bool clean_data_cache{true};
        bool invalidate_instruction_cache{true};
        bool invalidate_tlb{true};
        bool switch_exception_vectors{false};
        bool sync_context{true};
    };

    struct BootExecHooks {
        void* ctx{nullptr};
        bool (*prepare_jump)(void* ctx,
                             const BootPrepareContext& request) noexcept {nullptr};
        bool (*jump)(void* ctx,
                     const BootPrepareContext& request) noexcept {nullptr};
        BootPrepareHooks maintenance{};
        BootPreparePolicy policy{};
    };

    struct BootContext {
        BootAddressLayout layout{};
        BootExceptionLayout exception{};
        BootTranslationLayout translation{};
        BootTransferHooks transfer{};
        BootExecHooks exec{};
    };

    inline const BootExecRequest& BootPrepareContext::exec() const noexcept {
        return *request;
    }

    inline util::usize BootPrepareContext::vector_base() const noexcept {
        return boot ? boot->exception.vector_base : 0;
    }

    inline util::usize BootPrepareContext::translation_table_base() const noexcept {
        return boot ? boot->translation.translation_table_base : 0;
    }

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
        const BootPrepareContext prepare_ctx{&boot, &request};
        const auto invoke_prepare_step =
            [&](bool enabled,
                bool (*fn)(void*, const BootPrepareContext&) noexcept,
                void* hook_ctx) noexcept {
                if (!enabled || !fn) {
                    return true;
                }
                return fn(detail::select_hook_ctx(ctx, hook_ctx), prepare_ctx);
            };

        const auto& maintenance = boot.exec.maintenance;
        const auto& policy = boot.exec.policy;
        if (!invoke_prepare_step(policy.mask_cpu_exceptions,
                                 maintenance.mask_cpu_exceptions,
                                 maintenance.ctx) ||
            !invoke_prepare_step(policy.quiesce_interrupt_controller,
                                 maintenance.quiesce_interrupt_controller,
                                 maintenance.ctx) ||
            !invoke_prepare_step(policy.activate_payload_mapping,
                                 maintenance.activate_payload_mapping,
                                 maintenance.ctx) ||
            !invoke_prepare_step(policy.clean_data_cache,
                                 maintenance.clean_data_cache,
                                 maintenance.ctx) ||
            !invoke_prepare_step(policy.invalidate_instruction_cache,
                                 maintenance.invalidate_instruction_cache,
                                 maintenance.ctx) ||
            !invoke_prepare_step(policy.invalidate_tlb,
                                 maintenance.invalidate_tlb,
                                 maintenance.ctx) ||
            !invoke_prepare_step(policy.switch_exception_vectors,
                                 maintenance.switch_exception_vectors,
                                 maintenance.ctx) ||
            !invoke_prepare_step(policy.sync_context,
                                 maintenance.sync_context,
                                 maintenance.ctx)) {
            return false;
        }

        if (!boot.exec.prepare_jump) {
            return request.payload_base != 0 && request.entry_addr != 0;
        }

        return boot.exec.prepare_jump(
            detail::select_hook_ctx(ctx, boot.exec.ctx),
            prepare_ctx);
    }

    inline bool jump(void* ctx,
                     const BootExecRequest& request) noexcept {
        if (!ctx) {
            return false;
        }

        const auto& boot = *static_cast<const BootContext*>(ctx);
        const BootPrepareContext prepare_ctx{&boot, &request};
        if (!boot.exec.jump) {
            return false;
        }

        return boot.exec.jump(
            detail::select_hook_ctx(ctx, boot.exec.ctx),
            prepare_ctx);
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
