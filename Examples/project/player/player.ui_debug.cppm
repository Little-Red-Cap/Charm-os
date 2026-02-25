module;
#include <array>
#include <cstdint>
#include <cstdio>
#include <string>

export module player.ui_debug;

import charm.core.style;
import charm.gfx.render;
import charm.widgets.table_view;
import charm.widgets.text;
import charm.widgets.tree_view;

using namespace ui::render;

export namespace player::ui_debug {
    struct TreeDemoNode {
        const char* label{nullptr};
        int depth{0};
        bool expanded{false};
        bool has_children{false};
    };

    struct TreeCacheEntry {
        int index{-1};
        int depth{0};
        std::string label{};
    };

    struct TreeDemo {
        static constexpr int kMaxNodes = 8;
        std::array<TreeDemoNode, kMaxNodes> nodes{};
        std::array<int, kMaxNodes> visible{};
        std::array<TreeCacheEntry, 32> cache{};
        int node_count{0};
        int visible_count{0};
    };

    inline TreeDemo g_tree_demo{
        .nodes = {{
            {"System", 0, true, true},
            {"Audio", 1, true, true},
            {"Player", 2, false, false},
            {"Codec", 2, false, false},
            {"UI", 1, true, true},
            {"Vivid", 2, false, false},
            {"Ink", 2, false, false},
            {"Boot", 0, false, false},
        }},
        .node_count = 8,
        .visible_count = 0,
    };

    inline void tree_rebuild_visible(TreeDemo& demo) noexcept {
        demo.visible_count = 0;
        std::array<bool, 8> open{};
        open.fill(true);
        for (int i = 0; i < demo.node_count; ++i) {
            const auto& node = demo.nodes[i];
            if (node.depth > 0 && !open[static_cast<std::size_t>(node.depth - 1)]) continue;
            demo.visible[demo.visible_count++] = i;
            open[static_cast<std::size_t>(node.depth)] = node.expanded || !node.has_children;
        }
    }

    inline int tree_row_count(void* ctx) noexcept {
        auto* demo = static_cast<TreeDemo*>(ctx);
        return demo ? demo->visible_count : 0;
    }

    inline TreeView::NodeInfo tree_node_info(void* ctx, int index) noexcept {
        auto* demo = static_cast<TreeDemo*>(ctx);
        if (!demo || index < 0 || index >= demo->visible_count) return {};
        const int node_index = demo->visible[index];
        const auto& node = demo->nodes[node_index];
        return TreeView::NodeInfo{node.depth, node.expanded, node.has_children, node.label};
    }

    inline void on_tree_draw(void* ctx, CanvasBase& cvs, const TreeView::DrawInfo& info) noexcept {
        auto* demo = static_cast<TreeDemo*>(ctx);
        const auto& st = Theme::instance().get<TreeView>();
        Rect text = info.rect;
        const char* label = info.node.label ? info.node.label : "";
        int depth = info.node.depth;
        if (demo && info.slot >= 0 && info.slot < static_cast<int>(demo->cache.size())) {
            const auto& entry = demo->cache[info.slot];
            if (entry.index == info.index && !entry.label.empty()) {
                label = entry.label.c_str();
                depth = entry.depth;
            }
        }
        text.x += st.padding + depth * 12;
        text.w -= st.padding * 2;
        if (text.w <= 0 || text.h <= 0) return;
        draw_text_box(cvs, text, label,
                      st.font_color, resolve_font(st),
                      TextAlignH::Left, TextAlignV::Center, TextWrap::None, TextEllipsis::End);
    }

    inline void on_tree_toggle(void* ctx, int index) noexcept {
        auto* demo = static_cast<TreeDemo*>(ctx);
        if (!demo || index < 0 || index >= demo->visible_count) return;
        const int node_index = demo->visible[index];
        auto& node = demo->nodes[node_index];
        if (!node.has_children) return;
        node.expanded = !node.expanded;
        tree_rebuild_visible(*demo);
    }

    inline void on_tree_pool_create(void* ctx, int slot) noexcept {
        auto* demo = static_cast<TreeDemo*>(ctx);
        if (!demo) return;
        if (slot < 0 || slot >= static_cast<int>(demo->cache.size())) return;
        demo->cache[slot].index = -1;
        demo->cache[slot].depth = 0;
        demo->cache[slot].label.clear();
    }

    inline void on_tree_pool_bind(void* ctx, int slot, int index, const TreeView::NodeInfo& info) noexcept {
        auto* demo = static_cast<TreeDemo*>(ctx);
        if (!demo) return;
        if (slot < 0 || slot >= static_cast<int>(demo->cache.size())) return;
        auto& entry = demo->cache[slot];
        entry.index = index;
        entry.depth = info.depth;
        entry.label = info.label ? info.label : "";
    }

    inline void on_tree_pool_recycle(void* ctx, int slot, int index) noexcept {
        (void)index;
        auto* demo = static_cast<TreeDemo*>(ctx);
        if (!demo) return;
        if (slot < 0 || slot >= static_cast<int>(demo->cache.size())) return;
        auto& entry = demo->cache[slot];
        entry.index = -1;
        entry.depth = 0;
        entry.label.clear();
    }

    struct TableDemoRow {
        const char* name{nullptr};
        int value{0};
        int delta{0};
    };

    struct TableDemo {
        static constexpr int kRows = 6;
        std::array<TableDemoRow, kRows> rows{};
        std::array<int, kRows> order{};
        bool sort_asc{true};
    };

    inline TableDemo g_table_demo{
        .rows = {{
            {"Buffer", 64, 2},
            {"Underrun", 0, 0},
            {"Latency", 120, -3},
            {"Seek", 4, 1},
            {"Decode", 7, 0},
            {"Render", 12, -1},
        }},
        .order = {},
        .sort_asc = true,
    };

    inline void table_rebuild_order(TableDemo& demo) noexcept {
        for (int i = 0; i < TableDemo::kRows; ++i) {
            demo.order[i] = demo.sort_asc ? i : (TableDemo::kRows - 1 - i);
        }
    }

    inline int table_row_count(void* ctx) noexcept {
        auto* demo = static_cast<TableDemo*>(ctx);
        return demo ? TableDemo::kRows : 0;
    }

    inline int table_col_count(void* ctx) noexcept {
        (void)ctx;
        return 3;
    }

    inline int table_col_width(void* ctx, int col) noexcept {
        (void)ctx;
        return (col == 0) ? 140 : 80;
    }

    inline void on_table_draw(void* ctx, CanvasBase& cvs, const TableView::CellInfo& info) noexcept {
        auto* demo = static_cast<TableDemo*>(ctx);
        if (!demo || info.row < 0 || info.row >= TableDemo::kRows) return;
        const int row = demo->order[info.row];
        const auto& item = demo->rows[row];
        char buf[32]{};
        const char* text = "";
        if (info.col == 0) {
            text = item.name ? item.name : "";
        } else if (info.col == 1) {
            std::snprintf(buf, sizeof(buf), "%d", item.value);
            text = buf;
        } else if (info.col == 2) {
            std::snprintf(buf, sizeof(buf), "%+d", item.delta);
            text = buf;
        }

        const auto& st = Theme::instance().get<TableView>();
        Rect text_box = info.rect;
        text_box.x += st.padding;
        text_box.w -= st.padding * 2;
        draw_text_box(cvs, text_box, text, st.font_color, resolve_font(st),
                      TextAlignH::Left, TextAlignV::Center, TextWrap::None, TextEllipsis::End);
    }

    inline void on_table_select(void* ctx, int row, int col) noexcept {
        (void)ctx;
        (void)row;
        (void)col;
    }
}
