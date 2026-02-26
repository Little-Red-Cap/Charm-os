module;
#include <functional>
export module charm.widgets.tabview;

import charm.core.object;
import charm.gfx.color;
import charm.gfx.render;
import charm.core.event;
import charm.core.style;
import charm.core.style_sheet;
import charm.widgets.label;
import charm.core.string;
import charm.font.typography;

using namespace ui::render;

export
class TabView : public ObjectBase {
public:
    TabView() {
        set_size(260, 180);
        set_focusable(false);
    }

    // parent should link children pages manually; we store handles
    void add_tab(const char* title, WidgetHandle page) noexcept {
        if (tab_count_ >= max_tabs) return;
        titles_[tab_count_].assign(title ? title : "");
        pages_[tab_count_] = page;
        if (tab_count_ == 0) {
            set_active(0);
        }
        ++tab_count_;
    }

    void set_active(int idx) noexcept {
        if (idx < 0 || idx >= tab_count_) return;
        active_ = idx;
        for (int i = 0; i < tab_count_; ++i) {
            if (auto* p = resolver_(pages_[i])) {
                p->set_visible(i == active_);
            }
        }
    }

    int active() const noexcept { return active_; }

    void draw(CanvasBase& cvs) override {
        Style st = Theme::instance().get<TabView>();
        const auto r = get_rect();
        rgba bg{};
        rgba border{};
        rgba font{};
        const StyleState state = make_style_state(is_enabled(), has_state(State::Hovered), has_state(State::Pressed), has_state(State::Focused), style_variant());
        apply_style_sheet(WidgetKind::TabView, state, st);
        resolve_colors(st, state, bg, border, font);

        // tab bar
        const int tab_h = 26;
        draw_rect(cvs, r.x, r.y, r.w, tab_h, st.bg_color, true);
        draw_rect(cvs, r.x, r.y, r.w, tab_h, st.border_color, false);

        // content border
        draw_rect(cvs, r.x, r.y + tab_h, r.w, r.h - tab_h, st.border_color, false);

        // tab buttons
        int x = r.x + st.padding;
        for (int i = 0; i < tab_count_; ++i) {
            const bool on = (i == active_);
            const auto txt = titles_[i].c_str();
            Label lbl{txt};
            lbl.set_color(on ? st.font_color : st.font_color_disabled);
            lbl.set_font(resolve_font(st));
            const int btn_w = lbl.get_rect().w + st.padding * 2;
            const int btn_h = tab_h - 4;
            const int btn_x = x;
            const int btn_y = r.y + 2;
            rgba tbg = on ? st.bg_pressed : st.bg_color;
            rgba tborder = on ? st.border_pressed : st.border_color;
            draw_rect(cvs, btn_x, btn_y, btn_w, btn_h, tbg, true);
            draw_rect(cvs, btn_x, btn_y, btn_w, btn_h, tborder, false);
            const int baseline_y = btn_y + (btn_h - lbl.line_height()) / 2 + lbl.baseline();
            lbl.set_baseline_pos(btn_x + st.padding, baseline_y);
            lbl.draw(cvs);
            x += btn_w + st.padding;
        }
    }

    bool on_event(const Event& e) override {
        if (!is_enabled()) return false;
        if (e.type == Event::Type::Click) {
            const auto r = get_rect();
            const Style& st = Theme::instance().get<TabView>();
            const int tab_h = 26;
            if (e.y < r.y || e.y > r.y + tab_h) return false;
            int x = r.x + st.padding;
            for (int i = 0; i < tab_count_; ++i) {
                Label lbl{titles_[i].c_str()};
                lbl.set_font(resolve_font(st));
                const int btn_w = lbl.get_rect().w + st.padding * 2;
                const int btn_h = tab_h - 4;
                const int btn_x = x;
                const int btn_y = r.y + 2;
                if (e.x >= btn_x && e.x <= btn_x + btn_w && e.y >= btn_y && e.y <= btn_y + btn_h) {
                    set_active(i);
                    return true;
                }
                x += btn_w + st.padding;
            }
        }
        return false;
    }

    template<typename Resolver>
    void set_resolver(Resolver&& r) noexcept {
        resolver_ = r;
    }

private:
    static constexpr int max_tabs = 6;
    StaticString<32> titles_[max_tabs]{};
    WidgetHandle pages_[max_tabs]{};
    int tab_count_{0};
    int active_{0};
    // resolves WidgetHandle to ObjectBase*
    std::function<ObjectBase*(WidgetHandle)> resolver_ = [](WidgetHandle) -> ObjectBase* { return nullptr; };
};


