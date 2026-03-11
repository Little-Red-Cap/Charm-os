// app.runtime.cppm
// Shared runtime helpers for ink demo apps.

module;
#include <cstdint>

export module app.runtime;

import app.state;
import app.ui;
import app.logic_intent;
import gui.perf;
import gui.ui_input_policy;
import gui.input;
import input.router;

export namespace app
{
    struct RuntimeStats {
        int dirty_count{0};
        int dirty_area{0};
        bool dirty_full{false};
    };

    struct Runtime {
        AppState state{};
        ::input::Router router{};
        gui::input::RawSampler raw_sampler{};
        gui::ui::RouterIntentQueue<> router_queue{};
        RuntimeStats last_dirty{};

        void init(gui::perf::TickSource tick, bool consume = true) noexcept {
            state.init();
            app::set_tick_source(state, tick);

            router_queue.set_consume(consume);
            (void)router_queue.start(router);
            const auto policy = router_queue.policy();

            state.input_policies.set(gui::ui::InputPolicyId::Default, policy);
            state.input_policies.set(gui::ui::InputPolicyId::Encoder, policy);
            gui::ui::PolicyChain<2> chain{};
            chain.clear();
            chain.add(policy);
            state.input_policies.set(gui::ui::InputPolicyId::Custom, gui::ui::make_policy_chain(chain));
            state.input_policy_id = gui::ui::InputPolicyId::Default;
            state.input_policy = state.input_policies.get(state.input_policy_id);
        }

        template <class RawSource>
        void pump_raw(RawSource& raw, std::uint32_t now_ms) {
            while (auto ev = raw_sampler.poll(raw, now_ms)) {
                router.dispatch(*ev);
            }
        }

        void tick_ui(std::uint32_t now_ms) noexcept {
            state.now_ms = now_ms;
            state.fps_ui.update(state.tick);
        }

        void pump_app(std::uint32_t now_ms) noexcept {
            app::pump_input(state, now_ms);
        }

        void simulate_battery(std::uint32_t now_ms) noexcept {
            if (state.pages.current() == app::PageId::Main) {
                const std::uint32_t period = 6000;
                const std::uint32_t m = now_ms % period;
                int b = (m < period / 2) ? (100 - (int)(m * 100 / (period / 2)))
                                         : (int)((m - period / 2) * 100 / (period / 2));
                if (b < 0) b = 0;
                if (b > 100) b = 100;
                state.data.battery = b;
                state.data.progress_demo = (std::uint8_t)b;
            }
        }

        template <class Canvas, class FullFn, class RectFn>
        bool flush_canvas(Canvas& canvas, FullFn&& full_fn, RectFn&& rect_fn) {
            if (canvas.dirty_count() <= 0) {
                return false;
            }

            constexpr int kDirtyMaxRects = 4;
            constexpr int kDirtyAreaLimit = (Canvas::kWidth * Canvas::kHeight) / 2;
            const auto stats = canvas.dirty_stats();
            const bool too_many = (stats.count > kDirtyMaxRects);
            const bool too_big = (stats.area > kDirtyAreaLimit);
            const bool full = stats.full || too_many || too_big;

            last_dirty.dirty_count = stats.count;
            last_dirty.dirty_area = stats.area;
            last_dirty.dirty_full = full;

            if (full) {
                if (!full_fn()) {
                    return false;
                }
            } else {
                const int n = canvas.dirty_count();
                for (int i = 0; i < n; ++i) {
                    const auto dr = canvas.dirty_rect_at(i);
                    if (!rect_fn(dr)) {
                        return false;
                    }
                }
            }

            canvas.clear_dirty();
            return true;
        }
    };
}
