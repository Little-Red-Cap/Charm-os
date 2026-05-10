#include <cstdio>

import charm.core.geometry;
import charm.core.style;
import charm.core.theme_preset;
import charm.gfx.canvas;
import charm.ui.scene;

#include "../support/vivid_evidence_support.hpp"

namespace {
    constexpr vivid::evidence::RunLog kRunLog{"evl", "evidence_vocabulary_demo"};
    constexpr Rect kComponentBounds{4, 6, 40, 20};
}

int main() {
    auto run_log = kRunLog;
    run_log.begin();

    run_log.case_begin("state_delta_fields");
    const vivid::evidence::StateDeltaEvidence state_delta{
        .id = "demo.toggle",
        .key = "checked",
        .source = "programmatic",
        .old_value = 0,
        .new_value = 1,
    };
    vivid::evidence::print_state_delta(state_delta);
    std::printf("\n");
    if (!vivid::evidence::expect(state_delta.changed(), "state delta derives changed from old/new")) return 1;

    run_log.case_begin("invalidation_fields");
    const vivid::evidence::InvalidationEvidence invalidation{
        .kind = "paint_only",
        .dirty_scope = "component",
        .component_bounds = kComponentBounds,
        .layout_changed = false,
    };
    vivid::evidence::print_invalidation(invalidation);
    std::printf("\n");
    if (!vivid::evidence::expect(!invalidation.layout_changed, "paint_only keeps layout unchanged")) return 1;

    const vivid::evidence::RenderEvidence before{
        .cmd_count = 5,
        .cmd_bytes = 80,
        .exec_cmds = 5,
        .failed_cmds = 0,
        .dirty_count = 1,
        .dirty_hash = 101,
        .cmd_hash = 202,
        .pixel_hash = 303,
    };
    const vivid::evidence::RenderEvidence after{
        .cmd_count = 6,
        .cmd_bytes = 96,
        .exec_cmds = 6,
        .failed_cmds = 0,
        .dirty_count = 1,
        .dirty_hash = 101,
        .cmd_hash = 222,
        .pixel_hash = 333,
    };

    run_log.case_begin("render_fields");
    vivid::evidence::print_render_evidence("before", before);
    vivid::evidence::print_render_evidence("after", after);
    std::printf("\n");
    if (!vivid::evidence::expect(before.failed_cmds == 0 && after.failed_cmds == 0,
                                 "render evidence passes without failed commands")) {
        return 1;
    }

    const auto artifact_delta =
        vivid::evidence::make_render_artifact_delta(before, after, true);
    run_log.case_begin("artifact_delta_fields");
    vivid::evidence::print_render_artifact_delta(artifact_delta);
    std::printf("\n");
    if (!vivid::evidence::expect(artifact_delta.changed
                                 && artifact_delta.dirty_within_component
                                 && artifact_delta.single_dirty_rect,
                                 "artifact delta records changed bounded single-rect artifact")) {
        return 1;
    }

    const vivid::evidence::CausalChainEvidence chain{
        .name = "demo.toggle.activate",
        .request_ok = true,
        .state_delta_ok = state_delta.changed(),
        .invalidation_ok = !invalidation.layout_changed,
        .artifact_ok = artifact_delta.changed && artifact_delta.dirty_within_component,
        .rejected_no_mutation = true,
    };
    run_log.case_begin("causal_chain_fields");
    vivid::evidence::print_causal_chain(chain);
    std::printf("\n");
    if (!vivid::evidence::expect(chain.ok() && chain.rejected_no_mutation,
                                 "causal chain derives ok from segment verdicts")) {
        return 1;
    }

    run_log.end(true);
    std::puts("[evidence_vocabulary_demo] ok");
    return 0;
}
