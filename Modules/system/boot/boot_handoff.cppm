export module boot_handoff;

import platform.board;

export import boot_board_exec;
export import boot_launch;
export import boot_plan;
export import boot_storage;

export namespace boot {
    struct BootHandoff {
        BootExecution execution{};
        bool rollback_prepared{false};
        bool ready_to_jump{false};

        constexpr explicit operator bool() const noexcept {
            return ready_to_jump && static_cast<bool>(execution);
        }
    };

    constexpr BootPlan& handoff_plan(BootHandoff& handoff) noexcept {
        return handoff.execution.image.load.target.plan;
    }

    constexpr const BootPlan& handoff_plan(const BootHandoff& handoff) noexcept {
        return handoff.execution.image.load.target.plan;
    }

    constexpr BootTarget& handoff_target(BootHandoff& handoff) noexcept {
        return handoff.execution.image.load.target;
    }

    constexpr const BootTarget& handoff_target(const BootHandoff& handoff) noexcept {
        return handoff.execution.image.load.target;
    }

    constexpr BootLoadPlan& handoff_load(BootHandoff& handoff) noexcept {
        return handoff.execution.image.load;
    }

    constexpr const BootLoadPlan& handoff_load(const BootHandoff& handoff) noexcept {
        return handoff.execution.image.load;
    }

    constexpr BootLoadedImage& handoff_image(BootHandoff& handoff) noexcept {
        return handoff.execution.image;
    }

    constexpr const BootLoadedImage& handoff_image(const BootHandoff& handoff) noexcept {
        return handoff.execution.image;
    }

    inline BootHandoff prepare_boot_handoff(const Storage& s, const BootConfig& cfg,
                                            BootPlan plan,
                                            const platform::board::BootLoadDesc& load_desc,
                                            const platform::board::BootExecDesc& exec_desc) noexcept {
        BootHandoff handoff{};
        handoff_plan(handoff) = plan;
        if (!handoff_plan(handoff)) {
            return handoff;
        }

        handoff_target(handoff) = resolve_boot_target(s, cfg, handoff_plan(handoff));
        if (!handoff_target(handoff)) {
            return handoff;
        }

        handoff_load(handoff) = make_boot_load_plan(handoff_target(handoff));
        if (!handoff_load(handoff)) {
            return handoff;
        }

        handoff_image(handoff) = resolve_boot_loaded_image(handoff_load(handoff), load_desc);
        if (!handoff_image(handoff).address_resolved ||
            !prepare_boot_loaded_image(handoff_image(handoff), load_desc)) {
            return handoff;
        }

        handoff.execution = resolve_boot_execution(handoff_image(handoff), exec_desc);
        if (!handoff.execution) {
            return handoff;
        }

        if (!prepare_boot_plan(s, cfg, handoff_plan(handoff))) {
            return handoff;
        }

        handoff.rollback_prepared = true;
        handoff.ready_to_jump = handoff.rollback_prepared && static_cast<bool>(handoff.execution);
        return handoff;
    }

    inline BootHandoff prepare_boot_handoff(const Storage& s, const BootConfig& cfg,
                                            BootPlan plan,
                                            const platform::board::BootBoardCaps& caps) noexcept {
        return prepare_boot_handoff(s, cfg, plan, caps.load, caps.exec);
    }

    inline BootHandoff prepare_boot_handoff(const Storage& s, const BootConfig& cfg,
                                            BootPlan plan,
                                            const platform::board::BoardCaps& caps) noexcept {
        return prepare_boot_handoff(s, cfg, plan, caps.boot);
    }

    inline BootHandoff prepare_boot_handoff(const Storage& s, const BootConfig& cfg,
                                            const Policy& policy,
                                            const platform::board::BootLoadDesc& load_desc,
                                            const platform::board::BootExecDesc& exec_desc) noexcept {
        return prepare_boot_handoff(s, cfg, decide_boot_policy(s, cfg, policy), load_desc, exec_desc);
    }

    inline BootHandoff prepare_boot_handoff(const Storage& s, const BootConfig& cfg,
                                            const Policy& policy,
                                            const platform::board::BootBoardCaps& caps) noexcept {
        return prepare_boot_handoff(s, cfg, decide_boot_policy(s, cfg, policy), caps);
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
                                     const platform::board::BootBoardCaps& caps) noexcept {
        return execute_boot_handoff(handoff, caps.exec);
    }

    inline bool execute_boot_handoff(BootHandoff& handoff,
                                     const platform::board::BoardCaps& caps) noexcept {
        return execute_boot_handoff(handoff, caps.boot);
    }
}
