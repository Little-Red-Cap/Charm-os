export module boot_handoff;

import platform.board;

export import boot_board_exec;
export import boot_launch;
export import boot_plan;
export import boot_storage;

export namespace boot {
    struct BootHandoff {
        BootPlan plan{};
        BootTarget target{};
        BootExecution execution{};
        bool rollback_prepared{false};
        bool ready_to_jump{false};

        constexpr explicit operator bool() const noexcept {
            return ready_to_jump && static_cast<bool>(execution);
        }
    };

    inline BootHandoff prepare_boot_handoff(const Storage& s, const BootConfig& cfg,
                                            BootPlan plan,
                                            const platform::board::BootExecDesc& desc) noexcept {
        BootHandoff handoff{};
        handoff.plan = plan;
        if (!handoff.plan) {
            return handoff;
        }

        handoff.target = resolve_boot_target(s, cfg, handoff.plan);
        if (!handoff.target) {
            return handoff;
        }

        handoff.execution = resolve_boot_execution(handoff.target, desc);
        if (!handoff.execution) {
            return handoff;
        }

        if (!prepare_boot_plan(s, cfg, handoff.plan)) {
            return handoff;
        }

        handoff.rollback_prepared = true;
        handoff.target.plan = handoff.plan;
        handoff.execution.target.plan = handoff.plan;
        handoff.ready_to_jump = handoff.rollback_prepared && static_cast<bool>(handoff.execution);
        return handoff;
    }

    inline BootHandoff prepare_boot_handoff(const Storage& s, const BootConfig& cfg,
                                            BootPlan plan,
                                            const platform::board::BoardCaps& caps) noexcept {
        return prepare_boot_handoff(s, cfg, plan, caps.boot_exec);
    }

    inline BootHandoff prepare_boot_handoff(const Storage& s, const BootConfig& cfg,
                                            const Policy& policy,
                                            const platform::board::BootExecDesc& desc) noexcept {
        return prepare_boot_handoff(s, cfg, decide_boot_policy(s, cfg, policy), desc);
    }

    inline BootHandoff prepare_boot_handoff(const Storage& s, const BootConfig& cfg,
                                            const Policy& policy,
                                            const platform::board::BoardCaps& caps) noexcept {
        return prepare_boot_handoff(s, cfg, decide_boot_policy(s, cfg, policy), caps);
    }

    inline bool execute_boot_handoff(BootHandoff& handoff,
                                     const platform::board::BootExecDesc& desc) noexcept {
        if (!handoff.ready_to_jump) {
            return false;
        }
        return execute_boot_execution(handoff.execution, desc);
    }

    inline bool execute_boot_handoff(BootHandoff& handoff,
                                     const platform::board::BoardCaps& caps) noexcept {
        return execute_boot_handoff(handoff, caps.boot_exec);
    }
}
