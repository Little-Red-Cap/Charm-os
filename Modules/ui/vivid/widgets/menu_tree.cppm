module;
#include <cstddef>
export module charm.widgets.menu_tree;

import charm.core.factory;
import charm.core.handle;
import charm.core.geometry;
import charm.core.string;
import charm.core.event;
import charm.widgets.menu;
import charm.widgets.menu_item;

export
class MenuTree {
public:
    static constexpr int kMaxNodes = 64;

    MenuTree() = default;

    void init(UiFactory& factory, WidgetHandle parent) {
        factory_ = &factory;
        parent_ = parent;
        menu_h_ = factory_->create_menu();
        if (menu_h_) {
            factory_->link(parent_, menu_h_);
        }
    }

    WidgetHandle handle() const noexcept { return menu_h_; }

    void set_rect(const Rect& r) noexcept {
        if (auto* menu = get_menu()) {
            menu->set_rect(r);
        }
    }

    void set_item_height(int h) noexcept {
        if (h > 0) {
            item_h_ = h;
        }
        rebuild();
    }

    void set_indent(int px) noexcept {
        if (px >= 0) {
            indent_px_ = px;
        }
        rebuild();
    }

    void set_on_select(Callback cb) noexcept { on_select_ = cb; }

    int selected() const noexcept { return selected_; }

    int add_item(int parent, const char* text) {
        if (node_count_ >= kMaxNodes) return -1;
        const int id = node_count_++;
        nodes_[id].parent = parent;
        nodes_[id].expanded = false;
        nodes_[id].has_children = false;
        nodes_[id].text.assign(text ? text : "");
        if (parent >= 0 && parent < node_count_) {
            nodes_[parent].has_children = true;
        }
        return id;
    }

    void set_expanded(int id, bool on) noexcept {
        if (id < 0 || id >= node_count_) return;
        nodes_[id].expanded = on;
        rebuild();
    }

    void rebuild() {
        auto* menu = get_menu();
        if (!menu) return;
        menu->clear_children();
        visible_count_ = 0;
        build_visible(-1, 0);
        const auto rect = menu->get_rect();

        for (int i = 0; i < visible_count_; ++i) {
            const int node_id = visible_[i].node;
            const int depth = visible_[i].depth;
            auto item_h = factory_->create_menu_item(nodes_[node_id].text.c_str());
            if (auto* item = factory_->get_menu_item(item_h)) {
                item->set_size(rect.w, item_h_);
                item->set_indent(depth * indent_px_);
                item->set_has_children(nodes_[node_id].has_children);
                item->set_expanded(nodes_[node_id].expanded);
                item->set_selected(node_id == selected_);
                hooks_[i] = ItemHook{this, node_id};
                item->set_on_click(Callback{&MenuTree::on_item_click, &hooks_[i]});
                factory_->link(menu_h_, item_h);
            }
        }
    }

private:
    struct Node {
        int parent{-1};
        bool expanded{false};
        bool has_children{false};
        StaticString<32> text{};
    };

    struct VisibleItem {
        int node{-1};
        int depth{0};
    };

    struct ItemHook {
        MenuTree* tree{nullptr};
        int node{-1};
    };

    static void on_item_click(void* ctx) {
        auto* hook = static_cast<ItemHook*>(ctx);
        if (!hook || !hook->tree) return;
        hook->tree->handle_click(hook->node);
    }

    void handle_click(int node_id) {
        if (node_id < 0 || node_id >= node_count_) return;
        if (nodes_[node_id].has_children) {
            nodes_[node_id].expanded = !nodes_[node_id].expanded;
            rebuild();
            return;
        }
        selected_ = node_id;
        if (on_select_) on_select_();
        rebuild();
    }

    void build_visible(int parent, int depth) {
        for (int i = 0; i < node_count_; ++i) {
            if (nodes_[i].parent != parent) continue;
            if (visible_count_ >= kMaxNodes) return;
            visible_[visible_count_++] = VisibleItem{i, depth};
            if (nodes_[i].has_children && nodes_[i].expanded) {
                build_visible(i, depth + 1);
            }
        }
    }

    Menu* get_menu() noexcept {
        if (!factory_) return nullptr;
        return factory_->get_menu(menu_h_);
    }

    UiFactory* factory_{nullptr};
    WidgetHandle parent_{};
    WidgetHandle menu_h_{};
    Node nodes_[kMaxNodes]{};
    VisibleItem visible_[kMaxNodes]{};
    ItemHook hooks_[kMaxNodes]{};
    int node_count_{0};
    int visible_count_{0};
    int item_h_{24};
    int indent_px_{12};
    int selected_{-1};
    Callback on_select_{};
};
