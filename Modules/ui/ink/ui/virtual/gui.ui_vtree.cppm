//
// Minimal virtual tree + diff for declarative UI experiments.
//

module;
#include <cstdint>

export module gui.ui_vtree;

import gui.core;
import gui.font;
import gui.image_1bpp;
import gui.widgets;

export namespace gui::ui {
    using VNodeId = std::uint16_t;

    enum class VNodeKind : std::uint8_t {
        FillRect = 0,
        StrokeRect = 1,
        Text = 2,
        Line = 3,
        Image = 4,
        Custom = 5,
    };

    using VNodeDrawFn = void (*)(void* ctx, void* renderer) noexcept;

    struct VNode {
        VNodeId      id{0};
        VNodeKind    kind{VNodeKind::FillRect};
        Rect         rect{};
        const Font*  font{nullptr};
        const char*  text{nullptr};
        std::uint32_t text_hash{0};
        std::int16_t text_x{0};
        std::int16_t baseline_y{0};
        bool         on{true};
        VNodeDrawFn  draw{nullptr};
        void*        draw_ctx{nullptr};
        const void*  custom_key{nullptr};
        const Image1bpp* image{nullptr};
        std::int16_t img_x{0};
        std::int16_t img_y{0};
        std::int16_t line_x0{0};
        std::int16_t line_y0{0};
        std::int16_t line_x1{0};
        std::int16_t line_y1{0};
    };

    struct DirtyList {
        static constexpr int kMax = 8;
        Rect rects[kMax]{};
        int  count{0};
        bool full{false};

        inline void clear() noexcept { count = 0; full = false; }

        inline void add(Rect r) noexcept {
            if (full) return;
            if (r.w <= 0 || r.h <= 0) return;
            if (count >= kMax) {
                full = true;
                count = 0;
                return;
            }
            rects[count++] = r;
        }
    };

    template <int MaxNodes>
    struct VTree {
        VNode nodes[MaxNodes]{};
        int   count{0};

        inline void clear() noexcept { count = 0; }

        inline bool add_fill_rect(VNodeId id, const Rect& r, bool on) noexcept {
            if (count >= MaxNodes) return false;
            nodes[count++] = VNode{ id, VNodeKind::FillRect, r, nullptr, nullptr, 0, 0, 0, on, nullptr, nullptr, nullptr, nullptr, 0, 0, 0, 0, 0, 0 };
            return true;
        }

        inline bool add_stroke_rect(VNodeId id, const Rect& r, bool on) noexcept {
            if (count >= MaxNodes) return false;
            nodes[count++] = VNode{ id, VNodeKind::StrokeRect, r, nullptr, nullptr, 0, 0, 0, on, nullptr, nullptr, nullptr, nullptr, 0, 0, 0, 0, 0, 0 };
            return true;
        }

        inline bool add_text(VNodeId id,
                             const Rect& bounds,
                             const Font& font,
                             std::int16_t x,
                             std::int16_t baseline_y,
                             const char* text,
                             bool on) noexcept {
            if (count >= MaxNodes) return false;
            nodes[count++] = VNode{ id, VNodeKind::Text, bounds, &font, text, gui::hash_text(text), x, baseline_y, on, nullptr, nullptr, nullptr, nullptr, 0, 0, 0, 0, 0, 0 };
            return true;
        }

        inline bool add_line(VNodeId id,
                             const Rect& bounds,
                             std::int16_t x0,
                             std::int16_t y0,
                             std::int16_t x1,
                             std::int16_t y1,
                             bool on) noexcept {
            if (count >= MaxNodes) return false;
            VNode n{ id, VNodeKind::Line, bounds, nullptr, nullptr, 0, 0, 0, on, nullptr, nullptr, nullptr, nullptr, 0, 0, x0, y0, x1, y1 };
            nodes[count++] = n;
            return true;
        }

        inline bool add_image(VNodeId id,
                              const Rect& bounds,
                              const Image1bpp& img,
                              std::int16_t x,
                              std::int16_t y,
                              bool on) noexcept {
            if (count >= MaxNodes) return false;
            VNode n{ id, VNodeKind::Image, bounds, nullptr, nullptr, 0, 0, 0, on, nullptr, nullptr, nullptr, &img, x, y, 0, 0, 0, 0 };
            nodes[count++] = n;
            return true;
        }

        inline bool add_custom(VNodeId id,
                               const Rect& bounds,
                               VNodeDrawFn draw,
                               void* ctx,
                               const void* key) noexcept {
            if (count >= MaxNodes) return false;
            nodes[count++] = VNode{ id, VNodeKind::Custom, bounds, nullptr, nullptr, 0, 0, 0, true, draw, ctx, key, nullptr, 0, 0, 0, 0, 0, 0 };
            return true;
        }
    };

    [[nodiscard]] inline bool rect_eq(const Rect& a, const Rect& b) noexcept {
        return a.x == b.x && a.y == b.y && a.w == b.w && a.h == b.h;
    }

    [[nodiscard]] inline bool node_eq(const VNode& a, const VNode& b) noexcept {
        if (a.id != b.id || a.kind != b.kind || a.on != b.on) return false;
        if (!rect_eq(a.rect, b.rect)) return false;
        if (a.kind == VNodeKind::Text) {
            if (a.font != b.font) return false;
            if (a.text_x != b.text_x || a.baseline_y != b.baseline_y) return false;
            if (a.text_hash != b.text_hash) return false;
        }
        if (a.kind == VNodeKind::Line) {
            if (a.line_x0 != b.line_x0 || a.line_y0 != b.line_y0 ||
                a.line_x1 != b.line_x1 || a.line_y1 != b.line_y1) return false;
        }
        if (a.kind == VNodeKind::Image) {
            if (a.image != b.image || a.img_x != b.img_x || a.img_y != b.img_y) return false;
        }
        if (a.kind == VNodeKind::Custom) {
            if (a.draw != b.draw || a.draw_ctx != b.draw_ctx || a.custom_key != b.custom_key) return false;
        }
        return true;
    }

    template <int MaxNodes>
    [[nodiscard]] DirtyList diff_tree(const VTree<MaxNodes>& prev, const VTree<MaxNodes>& next) noexcept {
        DirtyList dirty{};
        for (int i = 0; i < prev.count; ++i) {
            const VNode& a = prev.nodes[i];
            const VNode* b = nullptr;
            for (int j = 0; j < next.count; ++j) {
                if (next.nodes[j].id == a.id) {
                    b = &next.nodes[j];
                    break;
                }
            }
            if (!b) {
                dirty.add(a.rect);
                continue;
            }
            if (!node_eq(a, *b)) {
                dirty.add(a.rect);
                dirty.add(b->rect);
            }
        }
        for (int j = 0; j < next.count; ++j) {
            const VNode& b = next.nodes[j];
            bool found = false;
            for (int i = 0; i < prev.count; ++i) {
                if (prev.nodes[i].id == b.id) { found = true; break; }
            }
            if (!found) dirty.add(b.rect);
        }
        return dirty;
    }

    template <class R, int MaxNodes>
    void draw_tree(R& r, const VTree<MaxNodes>& tree) noexcept {
        for (int i = 0; i < tree.count; ++i) {
            const VNode& n = tree.nodes[i];
            switch (n.kind) {
            case VNodeKind::FillRect:
                r.fillRect(n.rect, n.on);
                break;
            case VNodeKind::StrokeRect:
                r.drawRect(n.rect, n.on);
                break;
            case VNodeKind::Text:
                if (n.font && n.text) {
                    r.drawText(*n.font, n.text_x, n.baseline_y, n.text, n.on);
                }
                break;
            case VNodeKind::Line:
                gui::draw_line(r, n.line_x0, n.line_y0, n.line_x1, n.line_y1, n.on);
                break;
            case VNodeKind::Image:
                if (n.image) {
                    gui::draw_image_1bpp(r, n.img_x, n.img_y, *n.image, n.on);
                }
                break;
            case VNodeKind::Custom:
                if (n.draw) {
                    n.draw(n.draw_ctx, &r);
                }
                break;
            }
        }
    }
} // namespace gui::ui
