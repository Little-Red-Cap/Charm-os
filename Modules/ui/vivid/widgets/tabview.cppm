module;
#include <type_traits>
export module charm.widgets.tabview;

import charm.core.object;
import charm.gfx.color;
import charm.gfx.render_style;
import charm.core.event;
import charm.core.style;
import charm.core.style_sheet;
import charm.widgets.label;
import charm.core.string;
import charm.font.typography;

using namespace ui::render;

export
class TabView : public WidgetBase<TabView> {
public:
    struct ResolverCallback {
        using stub_t = ObjectBase*(*)(void*, WidgetHandle) noexcept;

        void* ctx{};
        stub_t stub{};

        [[nodiscard]] constexpr ObjectBase* operator()(WidgetHandle handle) const noexcept {
            return stub ? stub(ctx, handle) : nullptr;
        }

        constexpr explicit operator bool() const noexcept {
            return stub != nullptr;
        }

        template <auto Method, class T>
        [[nodiscard]] static constexpr ResolverCallback bind(T& obj) noexcept
            requires(std::is_member_function_pointer_v<decltype(Method)> &&
                     std::is_nothrow_invocable_r_v<ObjectBase*, decltype(Method), T&, WidgetHandle>)
        {
            return ResolverCallback{
                &obj,
                [](void* self, WidgetHandle handle) noexcept -> ObjectBase* {
                    return (static_cast<T*>(self)->*Method)(handle);
                },
            };
        }

        template <auto Fn>
        [[nodiscard]] static constexpr ResolverCallback bind() noexcept
            requires(std::is_pointer_v<decltype(Fn)> &&
                     std::is_function_v<std::remove_pointer_t<decltype(Fn)>> &&
                     std::is_nothrow_invocable_r_v<ObjectBase*, decltype(Fn), WidgetHandle>)
        {
            return ResolverCallback{
                nullptr,
                [](void*, WidgetHandle handle) noexcept -> ObjectBase* {
                    return Fn(handle);
                },
            };
        }
    };

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

    void draw(CanvasBase& cvs) {
        const StyleState state = make_style_state(is_enabled(), has_state(State::Hovered), has_state(State::Pressed), has_state(State::Focused), style_variant());
        const Style& base = Theme::instance().get<TabView>();
        Style st_scratch;
        const Style& st = resolve_style(WidgetKind::TabView, state, base, st_scratch);
        const auto r = get_rect();
        rgba bg{};
        rgba border{};
        rgba font{};
        resolve_colors(st, state, bg, border, font);
        const rgba accent = resolve_accent(st, state);

        // tab bar
        const int tab_h = 26;
        draw_rect(cvs, r.x, r.y, r.w, tab_h, bg, true);
        draw_rect(cvs, r.x, r.y, r.w, tab_h, border, false);
        // content border
        draw_rect(cvs, r.x, r.y + tab_h, r.w, r.h - tab_h, border, false);

        // tab buttons
        int x = r.x + st.metrics.padding;
        for (int i = 0; i < tab_count_; ++i) {
            const bool on = (i == active_);
            const auto txt = titles_[i].c_str();
            Label lbl{txt};
            lbl.set_color(on ? font : st.colors.font_color_disabled);
            lbl.set_font(resolve_font(st));
            const int btn_w = lbl.get_rect().w + st.metrics.padding * 2;
            const int btn_h = tab_h - 4;
            const int btn_x = x;
            const int btn_y = r.y + 2;
            rgba tbg = on ? accent : bg;
            rgba tborder = on ? accent : border;
            draw_rect(cvs, btn_x, btn_y, btn_w, btn_h, tbg, true);
            draw_rect(cvs, btn_x, btn_y, btn_w, btn_h, tborder, false);
            const int baseline_y = btn_y + (btn_h - lbl.line_height()) / 2 + lbl.baseline();
            lbl.set_baseline_pos(btn_x + st.metrics.padding, baseline_y);
            lbl.draw(cvs);
            x += btn_w + st.metrics.padding;
        }
    }

    bool on_event(const Event& e) {
        if (!is_enabled()) return false;
        if (e.type == Event::Type::Click) {
            const auto r = get_rect();
            const StyleState state = make_style_state(is_enabled(), has_state(State::Hovered), has_state(State::Pressed), has_state(State::Focused), style_variant());
            const Style& base = Theme::instance().get<TabView>();
            Style st_scratch;
            const Style& st = resolve_style(WidgetKind::TabView, state, base, st_scratch);
            const int tab_h = 26;
            if (e.y < r.y || e.y > r.y + tab_h) return false;
            int x = r.x + st.metrics.padding;
            for (int i = 0; i < tab_count_; ++i) {
                Label lbl{titles_[i].c_str()};
                lbl.set_font(resolve_font(st));
                const int btn_w = lbl.get_rect().w + st.metrics.padding * 2;
                const int btn_h = tab_h - 4;
                const int btn_x = x;
                const int btn_y = r.y + 2;
                if (e.x >= btn_x && e.x <= btn_x + btn_w && e.y >= btn_y && e.y <= btn_y + btn_h) {
                    set_active(i);
                    return true;
                }
                x += btn_w + st.metrics.padding;
            }
        }
        return false;
    }

    void set_resolver(ResolverCallback resolver) noexcept {
        resolver_ = resolver;
    }

    template <auto Method, class T>
    void set_resolver(T& obj) noexcept {
        resolver_ = ResolverCallback::bind<Method>(obj);
    }

    template <auto Fn>
    void set_resolver() noexcept {
        resolver_ = ResolverCallback::bind<Fn>();
    }

private:
    static constexpr int max_tabs = 6;
    StaticString<32> titles_[max_tabs]{};
    WidgetHandle pages_[max_tabs]{};
    int tab_count_{0};
    int active_{0};
    // Same-domain handle resolver with no dynamic allocation.
    ResolverCallback resolver_{};
};




