export module boot_exec;

import util.core;

export import boot_core;
export import boot_load;

export namespace boot {
    struct BootExecution {
        BootLoadedImage image{};
        util::usize payload_base{0};
        util::usize entry_addr{0};
        bool address_resolved{false};
        bool prepared{false};
        bool jumped{false};

        constexpr explicit operator bool() const noexcept {
            return static_cast<bool>(image) && address_resolved;
        }
    };

    struct BootExecOps {
        void* ctx{nullptr};
        bool (*prepare)(const BootExecution&, void* ctx) noexcept {nullptr};
        bool (*jump)(const BootExecution&, void* ctx) noexcept {nullptr};
    };

    inline BootExecution resolve_boot_execution(BootLoadedImage image) noexcept {
        BootExecution execution{};
        execution.image = image;
        if (!execution.image) {
            return execution;
        }

        execution.payload_base = execution.image.payload_base;
        execution.entry_addr = execution.image.entry_addr;
        execution.address_resolved = true;
        return execution;
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
