module;
#include <cstddef>
#include <span>
#include <type_traits>
export module charm.widgets.tabview;

import charm.core.object;
import charm.gfx.color;
import charm.gfx.render_style;
import charm.core.event;
import charm.core.style;
import charm.core.style_sheet;
import charm.widgets.label;
import charm.font.typography;

using namespace ui::render;

export
class TabView : public WidgetBase<TabView> {
public:
    struct Tab {
        const char* title{nullptr};
        WidgetHandle page{};
    };

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

    ~TabView() noexcept {
        detach_tab_storage();
    }

    TabView(const TabView&) = delete;
    TabView& operator=(const TabView&) = delete;
    TabView(TabView&&) = delete;
    TabView& operator=(TabView&&) = delete;

    void attach_tab_storage(std::span<Tab> storage) noexcept {
        detach_tab_storage();
        for (auto& tab : storage) tab = {};
        tabs_ = storage;
    }

    void detach_tab_storage() noexcept {
        clear_tabs();
        tabs_ = {};
    }

    [[nodiscard]] std::size_t tab_storage_capacity() const noexcept {
        return tabs_.size();
    }

    [[nodiscard]] int tab_count() const noexcept {
        return tab_count_;
    }

    // Parent links page objects separately; TabView only keeps their handles.
    [[nodiscard]] bool add_tab(const char* title, WidgetHandle page) noexcept {
        if (static_cast<std::size_t>(tab_count_) >= tabs_.size()) return false;
        tabs_[static_cast<std::size_t>(tab_count_)] = Tab{title ? title : "", page};
        ++tab_count_;
        if (tab_count_ == 1) active_ = 0;
        sync_page_visibility();
        return true;
    }

    void set_active(int idx) noexcept {
        if (idx < 0 || idx >= tab_count_) return;
        active_ = idx;
        sync_page_visibility();
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
            const auto& tab = tab_at(i);
            const auto txt = tab.title ? tab.title : "";
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
                const auto& tab = tab_at(i);
                Label lbl{tab.title ? tab.title : ""};
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
        sync_page_visibility();
    }

    template <auto Method, class T>
    void set_resolver(T& obj) noexcept {
        set_resolver(ResolverCallback::bind<Method>(obj));
    }

    template <auto Fn>
    void set_resolver() noexcept {
        set_resolver(ResolverCallback::bind<Fn>());
    }

private:
    [[nodiscard]] Tab& tab_at(int index) noexcept {
        return tabs_[static_cast<std::size_t>(index)];
    }

    [[nodiscard]] const Tab& tab_at(int index) const noexcept {
        return tabs_[static_cast<std::size_t>(index)];
    }

    void clear_tabs() noexcept {
        for (int i = 0; i < tab_count_; ++i) tab_at(i) = {};
        tab_count_ = 0;
        active_ = 0;
    }

    void sync_page_visibility() noexcept {
        if (!resolver_) return;
        for (int i = 0; i < tab_count_; ++i) {
            if (auto* page = resolver_(tab_at(i).page)) {
                page->set_visible(i == active_);
            }
        }
    }

    std::span<Tab> tabs_{};
    int tab_count_{0};
    int active_{0};
    // Same-domain handle resolver with no dynamic allocation.
    ResolverCallback resolver_{};
};

static_assert(std::is_trivially_copyable_v<TabView::Tab>);
static_assert(sizeof(TabView)
              <= sizeof(ObjectBase) + sizeof(std::span<TabView::Tab>)
                  + sizeof(int) * 2 + sizeof(TabView::ResolverCallback)
                  + alignof(std::span<TabView::Tab>),
              "TabView must not regain a fixed tab table");
static_assert(!std::is_copy_constructible_v<TabView>);
static_assert(!std::is_move_constructible_v<TabView>);




