module;

export module init.barrier;

import init.plan;
import init.recipe;
import init.meta;
import init.node;
import util.core;
import util.error;

export namespace init {
    struct no_runtime {
    };

    inline util::Result<void> barrier_start(no_runtime&) noexcept {
        return {};
    }

    template <fixed_string Name,
              typename Requires,
              typename Provides,
              Phase PhaseV = Phase::service,
              util::u32 RunlevelMask = static_cast<util::u32>(Runlevel::all)>
    using barrier_recipe = recipe_desc<
        Name,
        PhaseV,
        RunlevelMask,
        Provides,
        Requires,
        no_runtime,
        &barrier_start>;

    template <typename Cap, typename PlanLike>
    constexpr auto ready_as(PlanLike value) noexcept {
        return export_plan<PlanLike, Cap>{value};
    }
}
