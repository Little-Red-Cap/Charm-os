module;

#include "armv7a_handoff_contract.hpp"

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

    namespace detail {
        constexpr Armv7aHandoffLoadKind
        to_handoff_load_kind(BootLoadKind kind) noexcept {
            switch (kind) {
            case BootLoadKind::xip:
                return Armv7aHandoffLoadKind::xip;
            case BootLoadKind::copy_to_ram:
            default:
                return Armv7aHandoffLoadKind::copy_to_ram;
            }
        }

        constexpr Armv7aHandoffExecRequest
        make_handoff_exec_request(const BootExecRequest& request) noexcept {
            return Armv7aHandoffExecRequest{
                .kind = to_handoff_load_kind(request.kind),
                .payload_base = request.payload_base,
                .entry_addr = request.entry_addr,
                .storage_payload_offset = request.storage_payload_offset,
                .storage_entry_offset = request.storage_entry_offset,
                .entry_offset = request.entry_offset,
                .payload_size = request.payload_size,
                .image_size = request.image_size,
                .image_flags = request.image_flags
            };
        }

        constexpr Armv7aHandoffPreparePolicy
        make_handoff_policy(const BootPreparePolicy& policy) noexcept {
            return Armv7aHandoffPreparePolicy{
                .mask_cpu_exceptions = policy.mask_cpu_exceptions,
                .quiesce_interrupt_controller = policy.quiesce_interrupt_controller,
                .activate_payload_mapping = policy.activate_payload_mapping,
                .clean_data_cache = policy.clean_data_cache,
                .invalidate_instruction_cache = policy.invalidate_instruction_cache,
                .invalidate_tlb = policy.invalidate_tlb,
                .switch_exception_vectors = policy.switch_exception_vectors,
                .sync_context = policy.sync_context
            };
        }

        constexpr Armv7aHandoffPrepareContext
        make_handoff_prepare_context(const BootPrepareContext& prepare) noexcept {
            return prepare
                ? Armv7aHandoffPrepareContext{
                      .exec = make_handoff_exec_request(prepare.exec()),
                      .vector_base = prepare.vector_base(),
                      .translation_table_base = prepare.translation_table_base(),
                      .image_load_base = prepare.exec().payload_base
                  }
                : Armv7aHandoffPrepareContext{};
        }

        struct BootPrepareBridge {
            const BootContext* boot{nullptr};
            const BootExecRequest* request{nullptr};
            void* hook_ctx{nullptr};
        };

        constexpr BootPrepareContext
        make_prepare_context(const BootPrepareBridge& bridge) noexcept {
            return BootPrepareContext{bridge.boot, bridge.request};
        }

        inline bool mask_cpu_exceptions_adapter(
            void* ctx, const Armv7aHandoffPrepareContext&) noexcept {
            const auto* bridge = static_cast<const BootPrepareBridge*>(ctx);
            if (!bridge || !bridge->boot || !bridge->request ||
                !bridge->boot->exec.maintenance.mask_cpu_exceptions) {
                return false;
            }

            return bridge->boot->exec.maintenance.mask_cpu_exceptions(
                bridge->hook_ctx,
                make_prepare_context(*bridge));
        }

        inline bool quiesce_interrupt_controller_adapter(
            void* ctx, const Armv7aHandoffPrepareContext&) noexcept {
            const auto* bridge = static_cast<const BootPrepareBridge*>(ctx);
            if (!bridge || !bridge->boot || !bridge->request ||
                !bridge->boot->exec.maintenance.quiesce_interrupt_controller) {
                return false;
            }

            return bridge->boot->exec.maintenance.quiesce_interrupt_controller(
                bridge->hook_ctx,
                make_prepare_context(*bridge));
        }

        inline bool activate_payload_mapping_adapter(
            void* ctx, const Armv7aHandoffPrepareContext&) noexcept {
            const auto* bridge = static_cast<const BootPrepareBridge*>(ctx);
            if (!bridge || !bridge->boot || !bridge->request ||
                !bridge->boot->exec.maintenance.activate_payload_mapping) {
                return false;
            }

            return bridge->boot->exec.maintenance.activate_payload_mapping(
                bridge->hook_ctx,
                make_prepare_context(*bridge));
        }

        inline bool clean_data_cache_adapter(
            void* ctx, const Armv7aHandoffPrepareContext&) noexcept {
            const auto* bridge = static_cast<const BootPrepareBridge*>(ctx);
            if (!bridge || !bridge->boot || !bridge->request ||
                !bridge->boot->exec.maintenance.clean_data_cache) {
                return false;
            }

            return bridge->boot->exec.maintenance.clean_data_cache(
                bridge->hook_ctx,
                make_prepare_context(*bridge));
        }

        inline bool invalidate_instruction_cache_adapter(
            void* ctx, const Armv7aHandoffPrepareContext&) noexcept {
            const auto* bridge = static_cast<const BootPrepareBridge*>(ctx);
            if (!bridge || !bridge->boot || !bridge->request ||
                !bridge->boot->exec.maintenance.invalidate_instruction_cache) {
                return false;
            }

            return bridge->boot->exec.maintenance.invalidate_instruction_cache(
                bridge->hook_ctx,
                make_prepare_context(*bridge));
        }

        inline bool invalidate_tlb_adapter(
            void* ctx, const Armv7aHandoffPrepareContext&) noexcept {
            const auto* bridge = static_cast<const BootPrepareBridge*>(ctx);
            if (!bridge || !bridge->boot || !bridge->request ||
                !bridge->boot->exec.maintenance.invalidate_tlb) {
                return false;
            }

            return bridge->boot->exec.maintenance.invalidate_tlb(
                bridge->hook_ctx,
                make_prepare_context(*bridge));
        }

        inline bool switch_exception_vectors_adapter(
            void* ctx, const Armv7aHandoffPrepareContext&) noexcept {
            const auto* bridge = static_cast<const BootPrepareBridge*>(ctx);
            if (!bridge || !bridge->boot || !bridge->request ||
                !bridge->boot->exec.maintenance.switch_exception_vectors) {
                return false;
            }

            return bridge->boot->exec.maintenance.switch_exception_vectors(
                bridge->hook_ctx,
                make_prepare_context(*bridge));
        }

        inline bool sync_context_adapter(
            void* ctx, const Armv7aHandoffPrepareContext&) noexcept {
            const auto* bridge = static_cast<const BootPrepareBridge*>(ctx);
            if (!bridge || !bridge->boot || !bridge->request ||
                !bridge->boot->exec.maintenance.sync_context) {
                return false;
            }

            return bridge->boot->exec.maintenance.sync_context(
                bridge->hook_ctx,
                make_prepare_context(*bridge));
        }
    }

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
        const auto& maintenance = boot.exec.maintenance;
        detail::BootPrepareBridge bridge{
            .boot = &boot,
            .request = &request,
            .hook_ctx = detail::select_hook_ctx(ctx, maintenance.ctx)
        };
        const auto report = armv7a_run_handoff_prepare(
            detail::make_handoff_prepare_context(prepare_ctx),
            Armv7aHandoffPrepareContract{
                .hooks =
                    Armv7aHandoffPrepareHooks{
                        .ctx = &bridge,
                        .mask_cpu_exceptions = maintenance.mask_cpu_exceptions
                            ? &detail::mask_cpu_exceptions_adapter
                            : nullptr,
                        .quiesce_interrupt_controller =
                            maintenance.quiesce_interrupt_controller
                                ? &detail::quiesce_interrupt_controller_adapter
                                : nullptr,
                        .activate_payload_mapping =
                            maintenance.activate_payload_mapping
                                ? &detail::activate_payload_mapping_adapter
                                : nullptr,
                        .clean_data_cache = maintenance.clean_data_cache
                            ? &detail::clean_data_cache_adapter
                            : nullptr,
                        .invalidate_instruction_cache =
                            maintenance.invalidate_instruction_cache
                                ? &detail::invalidate_instruction_cache_adapter
                                : nullptr,
                        .invalidate_tlb = maintenance.invalidate_tlb
                            ? &detail::invalidate_tlb_adapter
                            : nullptr,
                        .switch_exception_vectors =
                            maintenance.switch_exception_vectors
                                ? &detail::switch_exception_vectors_adapter
                                : nullptr,
                        .sync_context = maintenance.sync_context
                            ? &detail::sync_context_adapter
                            : nullptr
                    },
                .policy = detail::make_handoff_policy(boot.exec.policy)
            });
        if (!report) {
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
