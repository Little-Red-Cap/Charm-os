//
// List page shell: declarative chrome + diff-based clearing.
//

module;
#include <cstdint>

export module gui.ui_list_shell;

import gui.core;
import gui.font;
import gui.layout;
import gui.ui_settings;
import gui.ui_vtree;

export namespace gui::ui {
    template <class R>
    struct ListChrome {
        Rect drawer{};
        Rect top{};
        Rect bottom{};
        Rect left{};
        Rect right{};
    };

    template <class R>
    [[nodiscard]] inline ListChrome<R> make_list_chrome(const Rect& drawer) noexcept
    {
        ListChrome<R> out{};
        out.drawer = drawer;
        out.top = Rect{0, 0, R::kWidth, drawer.y};
        out.bottom = Rect{
            0,
            (std::int16_t)(drawer.y + drawer.h),
            R::kWidth,
            (std::int16_t)(R::kHeight - (drawer.y + drawer.h))
        };
        out.left = Rect{0, drawer.y, drawer.x, drawer.h};
        out.right = Rect{
            (std::int16_t)(drawer.x + drawer.w),
            drawer.y,
            (std::int16_t)(R::kWidth - (drawer.x + drawer.w)),
            drawer.h
        };
        return out;
    }

    template <class R, int MaxNodes>
    inline void add_list_chrome(VTree<MaxNodes>& tree, const ListChrome<R>& c) noexcept
    {
        tree.add_fill_rect(1, c.drawer, false);
        tree.add_fill_rect(2, c.top, false);
        tree.add_fill_rect(3, c.bottom, false);
        tree.add_fill_rect(4, c.left, false);
        tree.add_fill_rect(5, c.right, false);
    }

    template <class R, int MaxNodes>
    inline void add_list_title(VTree<MaxNodes>& tree,
                               const Rect& drawer,
                               const Font& font,
                               const ListChromeStyle& style,
                               const char* label,
                               std::uint16_t node_id) noexcept
    {
        if (!style.show_title || !label || label[0] == '\0') return;
        const int base = gui::layout::baseline_from_top(font, (int)(drawer.y + style.title_dy));
        const int w = gui::layout::text_width(font, label);
        const Rect rc{
            (std::int16_t)(drawer.x + style.title_dx),
            (std::int16_t)(base - font.baseline),
            (std::int16_t)w,
            (std::int16_t)font.line_height
        };
        tree.add_text(node_id, rc, font, (std::int16_t)(drawer.x + style.title_dx), (std::int16_t)base, label, true);
    }

    template <int MaxNodes>
    struct ListPageShell {
        VTree<MaxNodes> prev{};
        bool valid{false};

        inline void reset() noexcept { prev.clear(); valid = false; }

        template <class R>
        void draw_chrome(R& r,
                         const Rect& drawer,
                         const ListChromeStyle& style,
                         const Font& font,
                         const char* title,
                         bool reset_full) noexcept
        {
            VTree<MaxNodes> cur{};
            const auto chrome = make_list_chrome<R>(drawer);
            add_list_chrome<R>(cur, chrome);
            add_list_title<R>(cur, drawer, font, style, title, 6);

            if (!valid || reset_full) {
                r.clear(false);
            } else {
                const auto dirty = diff_tree(prev, cur);
                if (dirty.full) {
                    r.clear(false);
                } else {
                    for (int i = 0; i < dirty.count; ++i) {
                        r.fillRect(dirty.rects[i], false);
                    }
                }
            }
            draw_tree(r, cur);
            prev = cur;
            valid = true;
        }
    };
} // namespace gui::ui
