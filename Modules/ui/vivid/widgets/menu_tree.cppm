module;
#include <cstddef>
#include <cstdint>
export module charm.widgets.menu_tree;

import charm.core.soa_factory;
import charm.core.handle;
import charm.core.geometry;
import charm.core.event;
import charm.core.structured_view;

export
class MenuTree {
public:
    using MenuProvider = StructuredMenuProvider;
    using MenuSelectionModel = StructuredMenuSelectionModel;

    MenuTree() = default;

    void init(SoaFactory& factory, WidgetHandle parent) {
        factory_ = &factory;
        parent_ = parent;

        menu_root_ = factory_->create_container();
        menu_panel_ = factory_->create_container();
        menu_list_ = factory_->create_list_view();
        submenu_panel_ = factory_->create_container();
        submenu_list_ = factory_->create_list_view();

        auto& kernel = factory_->kernel();
        kernel.set_visible(menu_root_, false);
        kernel.set_visible(menu_panel_, true);
        kernel.set_visible(menu_list_, true);
        kernel.set_visible(submenu_panel_, false);
        kernel.set_visible(submenu_list_, true);

        kernel.set_list_row_height(menu_list_, item_h_);
        kernel.set_list_row_height(submenu_list_, item_h_);
        kernel.set_scroll_step(menu_list_, item_h_);
        kernel.set_scroll_step(submenu_list_, item_h_);

        factory_->link(parent_, menu_root_);
        factory_->link(menu_root_, menu_panel_);
        factory_->link(menu_panel_, menu_list_);
        factory_->link(menu_root_, submenu_panel_);
        factory_->link(submenu_panel_, submenu_list_);
    }

    void set_provider(MenuProvider provider) noexcept { provider_ = provider; }
    void set_selection_model(MenuSelectionModel model) noexcept { selection_ = model; }
    void set_on_select(Callback cb) noexcept { on_select_ = cb; }

    void set_root_menu(int menu_id) noexcept { root_menu_id_ = menu_id; }

    void set_rect(const Rect& r) noexcept {
        if (!factory_) return;
        factory_->kernel().set_rect(menu_panel_, r);
    }

    WidgetHandle panel_handle() const noexcept { return menu_panel_; }

    void set_item_height(int h) noexcept {
        if (h <= 0 || !factory_) return;
        item_h_ = h;
        auto& kernel = factory_->kernel();
        kernel.set_list_row_height(menu_list_, item_h_);
        kernel.set_list_row_height(submenu_list_, item_h_);
        kernel.set_scroll_step(menu_list_, item_h_);
        kernel.set_scroll_step(submenu_list_, item_h_);
    }

    bool is_open() const noexcept { return is_open_; }

    void open(WidgetHandle host) {
        if (!factory_ || !provider_.count) return;
        if (root_menu_id_ < 0) return;
        host_ = host;
        const Rect host_rect = factory_->kernel().world_rect(host_);
        open_at(host_rect.x, host_rect.y + host_rect.h, host_rect.w);
    }

    void open_at(int x, int y, int w) {
        if (!factory_ || !provider_.count) return;
        if (root_menu_id_ < 0) return;
        const std::uint16_t count = provider_.count(provider_.ctx, root_menu_id_);
        const int visible = (count < kMaxVisible) ? count : kMaxVisible;
        auto& kernel = factory_->kernel();
        kernel.set_rect(menu_panel_, {x, y, w, visible * item_h_});
        kernel.set_rect(menu_list_, {0, 0, w, visible * item_h_});
        menu_scroll_.set_content(visible * item_h_, visible * item_h_);
        menu_scroll_.set_scroll(0);
        kernel.set_scroll_y(menu_list_, menu_scroll_.scroll_y);
        kernel.set_visible(menu_root_, true);
        kernel.set_visible(menu_panel_, true);
        kernel.set_visible(menu_list_, true);
        is_open_ = true;
        active_menu_id_ = root_menu_id_;
        sync_list_source(menu_list_, main_view_, selection_main_, active_menu_id_);
        close_submenu();
    }

    void close() {
        if (!factory_ || !is_open_) return;
        if (owns_input_state()) {
            factory_->kernel().input_request_cancel();
        }
        auto& kernel = factory_->kernel();
        kernel.set_visible(menu_root_, false);
        kernel.set_visible(menu_panel_, false);
        kernel.set_visible(submenu_panel_, false);
        is_open_ = false;
        active_menu_id_ = -1;
        active_submenu_id_ = -1;
    }

    bool handle_event(const Event& e) {
        if (!is_open_) return false;
        if (!factory_) return false;

        if (e.type == Event::Type::Cancel) {
            close();
            return true;
        }
        if (e.type == Event::Type::MouseDown || e.type == Event::Type::Click) {
            if (!is_inside_any(e.x, e.y)) {
                close();
                return true;
            }
        }

        const bool submenu_open = (active_submenu_id_ >= 0) && factory_->kernel().visible(submenu_panel_);
        switch (e.type) {
        case Event::Type::MouseMove:
            if (submenu_open && point_in_submenu(e.x, e.y)) {
                handle_mouse_move_in_list(submenu_list_, active_submenu_id_, e.y, true);
            } else if (point_in_menu(e.x, e.y)) {
                handle_mouse_move_in_list(menu_list_, active_menu_id_, e.y, false);
            }
            return true;
        case Event::Type::Click:
            if (submenu_open && point_in_submenu(e.x, e.y)) {
                handle_click_in_list(submenu_list_, active_submenu_id_, e.y, true);
            } else if (point_in_menu(e.x, e.y)) {
                handle_click_in_list(menu_list_, active_menu_id_, e.y, false);
            } else {
                close();
            }
            return true;
        case Event::Type::KeyDown:
            return handle_key(e.key_code, submenu_open);
        default:
            break;
        }
        return false;
    }

private:
    static constexpr int kMaxVisible = 10;

    std::uint16_t count(int menu_id) const noexcept {
        if (!provider_.count) return 0;
        return provider_.count(provider_.ctx, menu_id);
    }

    const char* label(int menu_id, std::uint16_t index) const noexcept {
        if (!provider_.label) return "";
        return provider_.label(provider_.ctx, menu_id, index);
    }

    bool enabled(int menu_id, std::uint16_t index) const noexcept {
        if (!provider_.enabled) return true;
        return provider_.enabled(provider_.ctx, menu_id, index);
    }

    bool has_children(int menu_id, std::uint16_t index) const noexcept {
        if (!provider_.has_children) return false;
        return provider_.has_children(provider_.ctx, menu_id, index);
    }

    int child_menu_id(int menu_id, std::uint16_t index) const noexcept {
        if (!provider_.child_id) return -1;
        return provider_.child_id(provider_.ctx, menu_id, index);
    }

    int get_selected(int menu_id) const noexcept {
        if (selection_.selected) {
            return selection_.selected(selection_.ctx, menu_id);
        }
        return (menu_id == active_menu_id_) ? fallback_selected_ : -1;
    }

    void set_selected(int menu_id, int index) noexcept {
        if (selection_.set_selected) {
            selection_.set_selected(selection_.ctx, menu_id, index);
            return;
        }
        if (menu_id == active_menu_id_) {
            fallback_selected_ = index;
        }
    }

    void clear_selected(int menu_id) noexcept {
        if (selection_.clear) {
            selection_.clear(selection_.ctx, menu_id);
            return;
        }
        if (menu_id == active_menu_id_) {
            fallback_selected_ = -1;
        }
    }

    void sync_list_source(WidgetHandle list, StructuredMenuView& view, StructuredMenuSelectionView& selection, int menu_id) {
        if (!provider_.count || !factory_) return;
        view.provider = &provider_;
        view.menu_id = menu_id;
        selection.model = &selection_;
        selection.menu_id = menu_id;

        const StructuredDataProvider provider_view = view.to_provider();
        const StructuredSelectionModel selection_view = selection.to_selection();
        const std::uint16_t count = provider_view.size();
        factory_->set_list_view_source(list, count, &view, &StructuredMenuView::label_text);
        const int selected = selection_view.current();
        if (selected >= 0) {
            factory_->kernel().set_list_view_selected(list, selected);
        } else if (count > 0) {
            factory_->kernel().set_list_view_selected(list, 0);
            selection_view.set(0);
        }
    }

    StructuredViewportMapper make_mapper(WidgetHandle list) noexcept {
        auto& kernel = factory_->kernel();
        StructuredScrollModel& scroll = (list == submenu_list_) ? submenu_scroll_ : menu_scroll_;
        const int menu_id = (list == submenu_list_) ? active_submenu_id_ : active_menu_id_;
        const std::uint16_t count = provider_.count ? provider_.count(provider_.ctx, menu_id) : 0;
        const Rect rect = kernel.world_rect(list);
        scroll.set_content(static_cast<int>(count) * item_h_, rect.h);
        scroll.set_scroll(kernel.scroll_y(list));
        scroll.wheel_step = kernel.scroll_step(list);
        StructuredViewportMapper mapper{};
        mapper.rect = rect;
        mapper.row_height = kernel.list_row_height(list);
        mapper.scroll_y = scroll.scroll_y;
        return mapper;
    }

    void open_submenu(int index, int row_y) {
        if (!factory_ || !provider_.count) return;
        if (!has_children(active_menu_id_, static_cast<std::uint16_t>(index))) {
            close_submenu();
            return;
        }
        const int submenu_id = child_menu_id(active_menu_id_, static_cast<std::uint16_t>(index));
        if (submenu_id < 0) {
            close_submenu();
            return;
        }
        active_submenu_id_ = submenu_id;
        auto& kernel = factory_->kernel();
        const Rect panel = kernel.world_rect(menu_panel_);
        const std::uint16_t count = provider_.count(provider_.ctx, active_submenu_id_);
        const int visible = (count < kMaxVisible) ? count : kMaxVisible;
        const int w = panel.w;
        kernel.set_rect(submenu_panel_, {panel.x + panel.w, row_y, w, visible * item_h_});
        kernel.set_rect(submenu_list_, {0, 0, w, visible * item_h_});
        kernel.set_visible(submenu_panel_, true);
        submenu_scroll_.set_content(visible * item_h_, visible * item_h_);
        submenu_scroll_.set_scroll(0);
        kernel.set_scroll_y(submenu_list_, submenu_scroll_.scroll_y);
        sync_list_source(submenu_list_, submenu_view_, selection_sub_, active_submenu_id_);
    }

    void close_submenu() {
        if (!factory_) return;
        factory_->kernel().set_visible(submenu_panel_, false);
        active_submenu_id_ = -1;
    }

    void handle_mouse_move_in_list(WidgetHandle list, int menu_id, int y, bool is_submenu) {
        const StructuredViewportMapper mapper = make_mapper(list);
        const int count = provider_.count(provider_.ctx, menu_id);
        const int idx = mapper.index_at(y, count);
        if (idx < 0) return;
        set_selected(menu_id, idx);
        factory_->kernel().set_list_view_selected(list, idx);
        if (!is_submenu) {
            open_submenu(idx, mapper.y_of(idx));
        }
    }

    void handle_click_in_list(WidgetHandle list, int menu_id, int y, bool is_submenu) {
        const StructuredViewportMapper mapper = make_mapper(list);
        const int count = provider_.count(provider_.ctx, menu_id);
        const int idx = mapper.index_at(y, count);
        if (idx < 0) return;
        set_selected(menu_id, idx);
        factory_->kernel().set_list_view_selected(list, idx);
        if (has_children(menu_id, static_cast<std::uint16_t>(idx))) {
            if (!is_submenu) {
                open_submenu(idx, mapper.y_of(idx));
            }
            return;
        }
        if (on_select_) on_select_();
        close();
    }

    bool handle_key(Event::Key key, bool submenu_open) {
        WidgetHandle list = submenu_open ? submenu_list_ : menu_list_;
        int menu_id = submenu_open ? active_submenu_id_ : active_menu_id_;
        const std::uint16_t count = provider_.count(provider_.ctx, menu_id);
        if (count == 0) return false;

        int selected = get_selected(menu_id);
        if (selected < 0) selected = 0;
        switch (key) {
        case Event::Key::Down:
            if (selected + 1 < static_cast<int>(count)) selected += 1;
            set_selected(menu_id, selected);
            factory_->kernel().set_list_view_selected(list, selected);
            if (!submenu_open) {
                const StructuredViewportMapper mapper = make_mapper(menu_list_);
                open_submenu(selected, mapper.y_of(selected));
            }
            return true;
        case Event::Key::Up:
            if (selected - 1 >= 0) selected -= 1;
            set_selected(menu_id, selected);
            factory_->kernel().set_list_view_selected(list, selected);
            if (!submenu_open) {
                const StructuredViewportMapper mapper = make_mapper(menu_list_);
                open_submenu(selected, mapper.y_of(selected));
            }
            return true;
        case Event::Key::Right:
            if (!submenu_open && has_children(menu_id, static_cast<std::uint16_t>(selected))) {
                const StructuredViewportMapper mapper = make_mapper(menu_list_);
                open_submenu(selected, mapper.y_of(selected));
                return true;
            }
            return false;
        case Event::Key::Left:
        case Event::Key::Escape:
            if (submenu_open) {
                close_submenu();
            } else {
                close();
            }
            return true;
        case Event::Key::Enter:
        case Event::Key::Space:
            if (has_children(menu_id, static_cast<std::uint16_t>(selected))) {
                const StructuredViewportMapper mapper = make_mapper(menu_list_);
                open_submenu(selected, mapper.y_of(selected));
                return true;
            }
            if (on_select_) on_select_();
            close();
            return true;
        default:
            break;
        }
        return false;
    }

    bool owns_input_state() const noexcept {
        const auto& kernel = factory_->kernel();
        const WidgetHandle captured = kernel.input_captured();
        const WidgetHandle pressed = kernel.input_pressed();
        return captured == menu_panel_
            || captured == menu_list_
            || captured == submenu_panel_
            || captured == submenu_list_
            || pressed == menu_panel_
            || pressed == menu_list_
            || pressed == submenu_panel_
            || pressed == submenu_list_;
    }

    bool is_inside_any(int x, int y) const noexcept {
        const Rect main_rect = factory_->kernel().world_rect(menu_panel_);
        if (main_rect.contains(x, y)) return true;
        if (factory_->kernel().visible(submenu_panel_)) {
            const Rect sub_rect = factory_->kernel().world_rect(submenu_panel_);
            if (sub_rect.contains(x, y)) return true;
        }
        return false;
    }

    bool point_in_menu(int x, int y) const noexcept {
        return factory_->kernel().world_rect(menu_panel_).contains(x, y);
    }

    bool point_in_submenu(int x, int y) const noexcept {
        if (!factory_->kernel().visible(submenu_panel_)) return false;
        return factory_->kernel().world_rect(submenu_panel_).contains(x, y);
    }

    SoaFactory* factory_{nullptr};
    WidgetHandle parent_{};
    WidgetHandle host_{};
    WidgetHandle menu_root_{};
    WidgetHandle menu_panel_{};
    WidgetHandle menu_list_{};
    WidgetHandle submenu_panel_{};
    WidgetHandle submenu_list_{};

    MenuProvider provider_{};
    MenuSelectionModel selection_{};
    StructuredMenuView main_view_{};
    StructuredMenuView submenu_view_{};
    StructuredMenuSelectionView selection_main_{};
    StructuredMenuSelectionView selection_sub_{};

    int root_menu_id_{-1};
    int active_menu_id_{-1};
    int active_submenu_id_{-1};
    int fallback_selected_{-1};
    int item_h_{24};
    bool is_open_{false};
    Callback on_select_{};
    StructuredScrollModel menu_scroll_{};
    StructuredScrollModel submenu_scroll_{};
};
