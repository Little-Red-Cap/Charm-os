export module boot_exec;

import util.core;

export import boot_core;
export import boot_launch;

export namespace boot {
    struct BootExecution {
        BootTarget target{};
        util::usize payload_base{0};
        util::usize entry_addr{0};
        bool address_resolved{false};
        bool prepared{false};
        bool jumped{false};

        constexpr explicit operator bool() const noexcept {
            return static_cast<bool>(target) && address_resolved;
        }
    };

    struct BootExecOps {
        void* ctx{nullptr};
        util::usize (*resolve_payload_base)(const BootTarget&, void* ctx) noexcept {nullptr};
        bool (*prepare)(const BootExecution&, void* ctx) noexcept {nullptr};
        bool (*jump)(const BootExecution&, void* ctx) noexcept {nullptr};
    };

    inline BootExecution resolve_boot_execution(const BootTarget& target,
                                                const BootExecOps& ops) noexcept {
        BootExecution execution{};
        execution.target = target;
        if (!execution.target || !ops.resolve_payload_base) {
            return execution;
        }

        execution.payload_base = ops.resolve_payload_base(execution.target, ops.ctx);
        if (execution.payload_base == 0) {
            return execution;
        }

        execution.entry_addr = execution.payload_base + execution.target.header.entry_offset;
        execution.address_resolved = true;
        return execution;
    }

    inline BootExecution resolve_boot_execution(const Storage& s, const BootConfig& cfg,
                                                BootPlan plan, const BootExecOps& ops) noexcept {
        return resolve_boot_execution(resolve_boot_target(s, cfg, plan), ops);
    }

    inline bool prepare_boot_execution(BootExecution& execution,
                                       const BootExecOps& ops) noexcept {
        if (!execution) {
            return false;
        }
        if (!ops.prepare) {
            execution.prepared = true;
            return true;
        }
        execution.prepared = ops.prepare(execution, ops.ctx);
        return execution.prepared;
    }

    inline bool execute_boot_execution(BootExecution& execution,
                                       const BootExecOps& ops) noexcept {
        if (!execution || !ops.jump) {
            return false;
        }
        if (!execution.prepared && !prepare_boot_execution(execution, ops)) {
            return false;
        }
        execution.jumped = ops.jump(execution, ops.ctx);
        return execution.jumped;
    }
}
