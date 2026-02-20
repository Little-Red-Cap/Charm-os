//
// List page helper: sync focus domain -> reduce viewport -> derive layout -> draw list.
// UI_PIPELINE_SSOT
// NO_PAGE_CALL_REDUCE_VIEWPORT
// NO_PAGE_CALL_DERIVE_LAYOUT
// LIST_PAGE_ONLY_NAV_KIND_LIST
// NO_PAGE_NAVMODE
// LIST_WRAP_SEMANTICS: affects only list index wrap (Clamp/Ring); does not affect viewport/scroll policy.
//

module;
#include <cassert>
#include <cstdint>

export module gui.ui_list_page;

import gui.core;
import gui.layout;
import alg_list_layout;
import gui.list_view;
import gui.ui_context;
import gui.ui_focus;
import gui.ui_list;
import gui.ui_scrollbar;
import gui.ui_tree;
import gui.ui_semantics;
import gui.theme;
import gui.ui_settings;
import gui.ui_list_shell;

export namespace gui::ui {

    enum class ListKind : std::uint8_t {
        Main = 0,
        Settings = 1,
    };

    enum class ListWrap : std::uint8_t {
        Clamp = 0,
        Ring = 1,
    };

    enum class ListPreset : std::uint8_t {
        Standard = 0,
        Clamped = 1,
        StandardWithScrollbar = 2,
    };

    struct ListPageSpec {
        NodeId               domain_id{kNullId};
        std::int16_t         item_count{0};
        std::int16_t         item_h{0};
        std::int16_t         gap{0};
        ViewportPolicy       viewport_policy{};
        gui::layout::Insets  insets{};
        bool                 enable_scrollbar{false};
        gui::ui::ScrollbarStyle scrollbar_style{};
        bool                 enable_pointer_focus{false};
        ListWrap             wrap{ListWrap::Ring};
        bool                 area_is_list_area{true};
        gui::ui::ListPageShell<8>* shell{nullptr};
        const char*          title{nullptr};
    };

    struct ListPageFrame {
        gui::Rect         list_area{};
        alg::list::Layout layout{};
        std::int16_t      focus_index{-1};
    };

    enum class ViewportPreset : std::uint8_t {
        Main = 0,
        Settings = 1,
        Fast = 2,
        Slow = 3,
    };

    [[nodiscard]] inline ViewportPolicy make_viewport_policy(const UiContext& ctx,
                                                             ViewportPreset preset) noexcept
    {
        ViewportPolicy policy{};
        policy.follow = ScrollFollow::KeepVisible;
        policy.allow_overscroll = false;
        policy.jump_ms_scale_pct = 50;

        std::uint16_t base = policy.base_ms;
        if (ctx.settings) {
            const auto& prof = ctx.settings->anim;
            base = gui::ui::is_on(ctx.settings->anim_enabled) ? prof.list.base_ms : 0;
            if (prof.list.min_ms > 0 && base < prof.list.min_ms) base = prof.list.min_ms;
            policy.jump_ms_scale_pct = prof.list.jump_scale_pct;
        }
        if (preset == ViewportPreset::Fast) {
            base = (base > 40) ? (std::uint16_t)(base / 2) : base;
        } else if (preset == ViewportPreset::Slow) {
            base = (std::uint16_t)(base * 2);
        }
        policy.base_ms = base;

        return policy;
    }

    [[nodiscard]] constexpr ListPageSpec default_list_spec(NodeId domain_id,
                                                           std::int16_t item_count) noexcept
    {
        ListPageSpec spec{};
        spec.domain_id = domain_id;
        spec.item_count = item_count;
        spec.wrap = ListWrap::Ring;
        spec.enable_scrollbar = false;
        spec.scrollbar_style = gui::ui::ScrollbarStyle{};
        spec.enable_pointer_focus = false;
        return spec;
    }

    [[nodiscard]] inline ListPageSpec default_list_spec(const UiContext& ctx,
                                                        NodeId domain_id,
                                                        std::int16_t item_count,
                                                        ListKind kind,
                                                        ListPreset preset) noexcept
    {
        ListPageSpec spec = default_list_spec(domain_id, item_count);
        spec.wrap = (preset == ListPreset::Clamped) ? ListWrap::Clamp : ListWrap::Ring;
        spec.enable_scrollbar = (preset == ListPreset::StandardWithScrollbar);
        ViewportPreset vp_preset = ViewportPreset::Main;
        if (kind == ListKind::Settings) vp_preset = ViewportPreset::Settings;
        spec.viewport_policy = make_viewport_policy(ctx, vp_preset);

        if (ctx.theme) {
            switch (kind) {
            case ListKind::Settings:
            case ListKind::Main:
            default:
                spec.item_h = (std::int16_t)ctx.theme->list_item_h;
                spec.gap = (std::int16_t)ctx.theme->list_gap;
                break;
            }
        }

        // Scrollbar style default tied to preset (pages stay zero-config).
        spec.scrollbar_style.track_w = (preset == ListPreset::StandardWithScrollbar) ? 3 : 2;
        spec.scrollbar_style.min_thumb_h = 3;
        spec.scrollbar_style.draw_rail = true;

        return spec;
    }

    [[nodiscard]] inline ListPageSpec default_list_spec(const UiContext& ctx,
                                                        NodeId domain_id,
                                                        std::int16_t item_count,
                                                        ListKind kind) noexcept
    {
        return default_list_spec(ctx, domain_id, item_count, kind, ListPreset::Standard);
    }

    [[nodiscard]] inline ListPageSpec default_list_spec(const UiContext& ctx,
                                                        NodeId domain_id,
                                                        std::int16_t item_count,
                                                        ListKind kind,
                                                        ListPreset preset,
                                                        gui::ui::ListPageShell<8>& shell,
                                                        const char* title) noexcept
    {
        auto spec = default_list_spec(ctx, domain_id, item_count, kind, preset);
        spec.shell = &shell;
        spec.title = title;
        return spec;
    }

    [[nodiscard]] inline ListPageSpec default_list_spec(const UiContext& ctx,
                                                        NodeId domain_id,
                                                        std::int16_t item_count,
                                                        ListKind kind,
                                                        ListPreset preset,
                                                        gui::ui::ListPageShell<8>& shell) noexcept
    {
        auto spec = default_list_spec(ctx, domain_id, item_count, kind, preset);
        spec.shell = &shell;
        switch (kind) {
        case ListKind::Settings:
            spec.title = "Settings";
            break;
        case ListKind::Main:
        default:
            spec.title = "Main";
            break;
        }
        return spec;
    }

    [[nodiscard]] constexpr ListPageSpec with_wrap(ListPageSpec spec, ListWrap wrap) noexcept
    {
        spec.wrap = wrap;
        return spec;
    }

    [[nodiscard]] constexpr ListPageSpec with_scrollbar(ListPageSpec spec, bool on = true) noexcept
    {
        spec.enable_scrollbar = on;
        return spec;
    }

    [[nodiscard]] constexpr ListPageSpec with_pointer_focus(ListPageSpec spec, bool on = true) noexcept
    {
        spec.enable_pointer_focus = on;
        return spec;
    }

    [[nodiscard]] constexpr ListPageSpec with_shell(ListPageSpec spec,
                                                    gui::ui::ListPageShell<8>* shell,
                                                    const char* title = nullptr) noexcept
    {
        spec.shell = shell;
        spec.title = title;
        return spec;
    }

    namespace detail {
        template<class FocusListT, class ListWidgetT>
        ListPageFrame run_pipeline(UiContext& ctx,
                                   const gui::Rect& area,
                                   const ListPageSpec& spec,
                                   const FocusListT& domain,
                                   ListWidgetT& list) noexcept
        {
            ListPageFrame frame{};
            if (!ctx.sem || !ctx.viewport) return frame;

            frame.list_area = spec.area_is_list_area ? area : gui::layout::inset_rect(area, spec.insets);

            auto& sem = *ctx.sem;
            (void)sync_focus_domain(spec.domain_id, domain, sem.focus);
            frame.focus_index = sem.focus.index;

            *ctx.viewport = gui::ui::reduce_viewport(*ctx.viewport,
                                                     frame.list_area.h,
                                                     spec.item_h,
                                                     spec.gap,
                                                     spec.item_count,
                                                     frame.focus_index,
                                                     sem.focus.last_dir,
                                                     sem.focus.last_jump,
                                                     spec.viewport_policy,
                                                     ctx.now_ms);

            frame.layout = alg::list::derive_layout(frame.list_area.h,
                                                    spec.item_h,
                                                    spec.gap,
                                                    spec.item_count,
                                                    ctx.viewport->scroll_y);

            list.draw_scrollbar = spec.enable_scrollbar;
            list.scrollbar_style = spec.scrollbar_style;
            list.settings = ctx.settings;
            if (frame.layout.row_count > 0) {
                list.layout_rows(frame.list_area, spec.item_h, frame.layout.row_count, spec.gap);
            }
            return frame;
        }
    } // namespace detail

    template<class R, class AppStateT, class FocusListT, class RowFn, class PressedFn, class HighlightFn, class ListWidgetT>
    ListPageFrame draw_list_page(
        R& r,
        AppStateT& s,
        UiContext& ctx,
        const gui::Rect& area,
        const ListPageSpec& spec,
        const FocusListT& domain,
        ListWidgetT& list,
        RowFn row_fn,
        PressedFn pressed_fn,
        HighlightFn highlight_fn
    ) noexcept
    {
        // Grid/Free 请走专用 helper，不要扩展 list pipeline。
#ifndef NDEBUG
        assert(ctx.sem != nullptr);
        assert(spec.item_count >= 0);
        assert(spec.wrap == ListWrap::Ring || spec.wrap == ListWrap::Clamp);
#endif
        const auto frame = detail::run_pipeline(ctx, area, spec, domain, list);
        if (!ctx.sem || !ctx.viewport) return frame;

        if (spec.shell && ctx.settings && ctx.theme) {
            const auto drawer = spec.area_is_list_area ? area : gui::layout::inset_rect(area, spec.insets);
            spec.shell->draw_chrome(r, drawer, ctx.settings->list_chrome, *ctx.theme->font_default, spec.title, false);
        }

        const std::int16_t pressed_index = pressed_fn(frame.focus_index);
        const int highlight_y = highlight_fn(frame.layout, frame.focus_index);

        auto get_item = [&](std::int16_t i) -> const gui::ListItem<AppStateT>* {
            return row_fn(i);
        };

        r.push_clip(frame.list_area);
        list.draw(r,
                  get_item,
                  spec.item_count,
                  frame.layout,
                  frame.focus_index,
                  pressed_index,
                  ctx.now_ms,
                  s,
                  highlight_y);
        r.pop_clip();

        return frame;
    }

} // namespace gui::ui
