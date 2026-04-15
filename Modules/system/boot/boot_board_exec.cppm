export module boot_board_exec;

import platform.board;

export import boot_board_load;
export import boot_exec;

export namespace boot {
    constexpr platform::board::BootExecRequest
    make_board_boot_exec_request(const BootExecution& execution) noexcept {
        return platform::board::BootExecRequest{
            .payload_base = execution.payload_base,
            .entry_addr = execution.entry_addr,
            .payload_size = execution.image.load.target.header.payload_size,
            .image_flags = execution.image.load.target.header.flags
        };
    }

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
        const auto request = make_board_boot_exec_request(execution);
        execution.prepared = desc.prepare_jump(desc.ctx, request);
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
        const auto request = make_board_boot_exec_request(execution);
        execution.jumped = desc.jump(desc.ctx, request);
        return execution.jumped;
    }

    inline BootExecution resolve_boot_execution(BootLoadedImage image,
                                                const platform::board::BootBoardCaps& caps) noexcept {
        return resolve_boot_execution(image, caps.exec);
    }

    inline bool prepare_boot_execution(BootExecution& execution,
                                       const platform::board::BootBoardCaps& caps) noexcept {
        return prepare_boot_execution(execution, caps.exec);
    }

    inline bool execute_boot_execution(BootExecution& execution,
                                       const platform::board::BootBoardCaps& caps) noexcept {
        return execute_boot_execution(execution, caps.exec);
    }

    inline BootExecution resolve_boot_execution(BootLoadedImage image,
                                                const platform::board::BoardCaps& caps) noexcept {
        return resolve_boot_execution(image, caps.boot);
    }

    inline bool prepare_boot_execution(BootExecution& execution,
                                       const platform::board::BoardCaps& caps) noexcept {
        return prepare_boot_execution(execution, caps.boot);
    }

    inline bool execute_boot_execution(BootExecution& execution,
                                       const platform::board::BoardCaps& caps) noexcept {
        return execute_boot_execution(execution, caps.boot);
    }
}
