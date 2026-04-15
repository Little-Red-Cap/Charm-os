export module boot_board_exec;

import platform.board;

export import boot_exec;

export namespace boot {
    inline BootExecution resolve_boot_execution(const BootTarget& target,
                                                const platform::board::BootExecDesc& desc) noexcept {
        BootExecution execution{};
        execution.target = target;
        if (!execution.target || !desc.resolve_payload_base) {
            return execution;
        }

        execution.payload_base = desc.resolve_payload_base(
            desc.ctx,
            target.payload_offset,
            target.storage_entry_offset,
            target.header.entry_offset,
            target.header.payload_size,
            target.header.image_size,
            target.header.flags);
        if (execution.payload_base == 0) {
            return execution;
        }

        execution.entry_addr = execution.payload_base + target.header.entry_offset;
        execution.address_resolved = true;
        return execution;
    }

    inline BootExecution resolve_boot_execution(const Storage& s, const BootConfig& cfg,
                                                BootPlan plan,
                                                const platform::board::BootExecDesc& desc) noexcept {
        return resolve_boot_execution(resolve_boot_target(s, cfg, plan), desc);
    }

    inline bool prepare_boot_execution(BootExecution& execution,
                                       const platform::board::BootExecDesc& desc) noexcept {
        if (!execution) {
            return false;
        }
        if (!desc.prepare_jump) {
            execution.prepared = true;
            return true;
        }
        execution.prepared = desc.prepare_jump(desc.ctx,
                                               execution.payload_base,
                                               execution.entry_addr,
                                               execution.target.header.payload_size,
                                               execution.target.header.flags);
        return execution.prepared;
    }

    inline bool execute_boot_execution(BootExecution& execution,
                                       const platform::board::BootExecDesc& desc) noexcept {
        if (!execution || !desc.jump) {
            return false;
        }
        if (!execution.prepared && !prepare_boot_execution(execution, desc)) {
            return false;
        }
        execution.jumped = desc.jump(desc.ctx, execution.payload_base, execution.entry_addr);
        return execution.jumped;
    }

    inline BootExecution resolve_boot_execution(const BootTarget& target,
                                                const platform::board::BoardCaps& caps) noexcept {
        return resolve_boot_execution(target, caps.boot_exec);
    }

    inline BootExecution resolve_boot_execution(const Storage& s, const BootConfig& cfg,
                                                BootPlan plan,
                                                const platform::board::BoardCaps& caps) noexcept {
        return resolve_boot_execution(s, cfg, plan, caps.boot_exec);
    }

    inline bool prepare_boot_execution(BootExecution& execution,
                                       const platform::board::BoardCaps& caps) noexcept {
        return prepare_boot_execution(execution, caps.boot_exec);
    }

    inline bool execute_boot_execution(BootExecution& execution,
                                       const platform::board::BoardCaps& caps) noexcept {
        return execute_boot_execution(execution, caps.boot_exec);
    }
}
