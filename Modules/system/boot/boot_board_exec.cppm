export module boot_board_exec;

import platform.board;

export import boot_board_load;
export import boot_exec;

export namespace boot {
    inline BootExecution resolve_boot_execution(BootLoadedImage image,
                                                const platform::board::BootExecDesc&) noexcept {
        return resolve_boot_execution(image);
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
                                               execution.image.load.target.header.payload_size,
                                               execution.image.load.target.header.flags);
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

    inline BootExecution resolve_boot_execution(BootLoadedImage image,
                                                const platform::board::BoardCaps& caps) noexcept {
        return resolve_boot_execution(image, caps.boot_exec);
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
