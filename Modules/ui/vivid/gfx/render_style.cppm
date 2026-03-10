module;
export module charm.gfx.render_style;
export import charm.gfx.render;
import charm.core.style;

namespace ui::render {

export inline void draw_focus_ring(CanvasBase& cvs, const Rect& rect, const Style& st, bool focused,
                                   int inset = 0, int radius = -1) noexcept {
    draw_focus_ring(cvs, rect, st.colors.border_focus, st.metrics.corner_radius, focused, inset, radius);
}

} // namespace ui::render
