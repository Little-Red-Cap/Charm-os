export module boot_plan;

export import boot_core;
export import boot_flow;
export import boot_policy;
export import boot_storage;

export namespace boot {
    struct BootPlan {
        BootResult boot{};
        BootInfo info{};
        BootSelectionReason reason{BootSelectionReason::none};
        bool boot_info_loaded{false};
        bool prepare_required{false};
        bool confirm_required{false};
        bool prepared{false};

        constexpr explicit operator bool() const noexcept {
            return boot.status == BootStatus::ok;
        }
    };

    inline BootPlan make_boot_plan(const Storage& s, const BootConfig& cfg,
                                   BootInfo info, bool boot_info_loaded,
                                   const Policy& policy) noexcept {
        BootPlan plan{};
        plan.info = info;
        plan.boot_info_loaded = boot_info_loaded;

        const auto a_status = verify_partition_policy_status(s, cfg.slot_a, policy, plan.info);
        const auto b_status = verify_partition_policy_status(s, cfg.slot_b, policy, plan.info);
        const auto selection = select_slot_candidate(plan.info, a_status, b_status);
        plan.boot = selection.boot;
        plan.reason = selection.reason;

        if (plan.boot.status == BootStatus::ok) {
            plan.prepare_required = plan.reason == BootSelectionReason::pending_trial;
            plan.confirm_required = plan.boot.slot != plan.info.active;
            plan.prepared = !plan.prepare_required;
        }

        return plan;
    }

    inline BootPlan decide_boot_policy(const Storage& s, const BootConfig& cfg,
                                       BootInfo info, const Policy& policy) noexcept {
        return make_boot_plan(s, cfg, info, true, policy);
    }

    inline BootPlan decide_boot_policy(const Storage& s, const BootConfig& cfg,
                                       const Policy& policy) noexcept {
        BootInfo info{};
        const bool boot_info_loaded = read_boot_info(s, cfg.info, info);
        if (!boot_info_loaded) {
            info = {};
        }
        return make_boot_plan(s, cfg, info, boot_info_loaded, policy);
    }

    inline bool prepare_boot_plan(const Storage& s, const BootConfig& cfg,
                                  BootPlan& plan) noexcept {
        if (plan.boot.status != BootStatus::ok) {
            return false;
        }
        if (!plan.prepare_required) {
            plan.prepared = true;
            return true;
        }
        if (!prepare_boot(s, cfg, plan.info, plan.boot.slot)) {
            return false;
        }
        plan.boot_info_loaded = true;
        plan.prepare_required = false;
        plan.prepared = true;
        return true;
    }

    inline bool confirm_boot_plan(const Storage& s, const BootConfig& cfg,
                                  BootPlan& plan) noexcept {
        if (plan.boot.status != BootStatus::ok) {
            return false;
        }
        if (plan.prepare_required && !plan.prepared) {
            return false;
        }
        if (!mark_success(s, cfg, plan.info, plan.boot.slot)) {
            return false;
        }
        plan.boot_info_loaded = true;
        plan.prepare_required = false;
        plan.prepared = true;
        plan.confirm_required = false;
        return true;
    }
}
