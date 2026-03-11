module;
#include <cstddef>
export module charm.widgets.dropdown_popup;

import charm.core.handle;
import charm.core.object;
import charm.core.soa_factory;
import charm.widgets.list;
import charm.widgets.list_utils;
import charm.widgets.scroll_container;
import charm.widgets.popup_layer;
import charm.core.event;
import charm.gfx.render_style;
import charm.core.string;

using namespace ui::render;

// Dropdown + Popup + ListItem ???????????
export
class DropdownPopup {
public:
    static constexpr int kMaxOptions = 16;

    DropdownPopup(UiFactory& factory, WidgetHandle host, WidgetHandle root)
        : factory_(factory), host_(host), root_(root) {
        popup_h_ = factory_.create_popup_layer();
        if (auto* p = factory_.get_container(popup_h_)) {
            p->set_visible(false);
            p->set_background({255, 255, 255, 240});
            factory_.link(root_, popup_h_);
        }
    }

    void set_options(const char* const* opts, int count) {
        option_count_ = 0;
        const int cap = (count < kMaxOptions) ? count : kMaxOptions;
        for (int i = 0; i < cap; ++i) {
            options_[i].assign(opts[i] ? opts[i] : "");
            ++option_count_;
        }
        rebuild_list();
    }

    void open(int x, int y, int w) {
        auto* popup = factory_.get_container(popup_h_);
        if (!popup) return;
        const int max_visible = 8;
        const int item_h = 24;
        const int visible = (option_count_ < max_visible) ? option_count_ : max_visible;
        popup->set_rect({x, y, w, visible * item_h + 8});
        popup->clear_children();
        rebuild_list();
        popup->set_visible(true);
        factory_.set_overlay(popup_h_);
    }

    void close() {
        if (auto* popup = factory_.get_container(popup_h_)) {
            popup->set_visible(false);
            popup->clear_children();
        }
        factory_.clear_overlay(popup_h_);
    }

    bool is_open() const noexcept {
        auto* popup = factory_.get_container(popup_h_);
        return popup && popup->is_visible();
    }

    void set_on_select(Callback cb) { on_select_ = cb; }
    void set_selection(int idx) {
        if (idx < 0 || idx >= option_count_) return;
        selected_ = idx;
        set_list_selection([&](WidgetHandle h){ return factory_.get(h); },
                           [&](WidgetHandle h){ return factory_.get_list_item(h); },
                           list_h_, selected_);
    }

    int selected() const noexcept { return selected_; }

    bool handle_event(const Event& e) {
        if (!is_open()) return false;
        auto* popup = factory_.get_container(popup_h_);
        if (!popup) return false;
        const auto r = popup->get_rect();
        if (e.type == Event::Type::MouseDown) {
            if (!r.contains(e.x, e.y)) {
                close();
                return true;
            }
        } else if (e.type == Event::Type::Click) {
            if (r.contains(e.x, e.y)) {
                const int idx = (e.y - r.y - 2) / 26;
                if (idx >= 0 && idx < option_count_) {
                    selected_ = idx;
                    set_list_selection([&](WidgetHandle h){ return factory_.get(h); },
                                       [&](WidgetHandle h){ return factory_.get_list_item(h); },
                                       list_h_, selected_);
                    if (on_select_) on_select_();
                    close();
                }
                return true;
            } else {
                close();
                return true;
            }
        } else if (e.type == Event::Type::MouseMove) {
            if (r.contains(e.x, e.y)) {
                const int idx = (e.y - r.y - 2) / 26;
                if (idx >= 0 && idx < option_count_) {
                    selected_ = idx;
                    set_list_selection([&](WidgetHandle h){ return factory_.get(h); },
                                       [&](WidgetHandle h){ return factory_.get_list_item(h); },
                                       list_h_, selected_);
                }
                return true;
            }
        } else if (e.type == Event::Type::MouseWheel) {
            if (r.contains(e.x, e.y)) {
                const int dir = (e.wheel_y < 0) ? 1 : -1;
                if (option_count_ > 0) {
                    selected_ = (selected_ + dir + option_count_) % option_count_;
                    set_list_selection([&](WidgetHandle h){ return factory_.get(h); },
                                       [&](WidgetHandle h){ return factory_.get_list_item(h); },
                                       list_h_, selected_);
                }
                return true;
            }
        } else if (e.type == Event::Type::KeyDown) {
            if (option_count_ == 0) return false;
            if (e.key_code == Event::Key::Down) {
                selected_ = (selected_ + 1) % option_count_;
                set_list_selection([&](WidgetHandle h){ return factory_.get(h); },
                                   [&](WidgetHandle h){ return factory_.get_list_item(h); },
                                   list_h_, selected_);
                return true;
            } else if (e.key_code == Event::Key::Up) {
                selected_ = (selected_ - 1 + option_count_) % option_count_;
                set_list_selection([&](WidgetHandle h){ return factory_.get(h); },
                                   [&](WidgetHandle h){ return factory_.get_list_item(h); },
                                   list_h_, selected_);
                return true;
            } else if (e.key_code == Event::Key::Enter || e.key_code == Event::Key::Space) {
                if (on_select_) on_select_();
                close();
                return true;
            } else if (e.key_code == Event::Key::Escape) {
                close();
                return true;
            }
        }
        return false;
    }

private:
    void rebuild_list() {
        auto* popup = factory_.get_container(popup_h_);
        if (!popup) return;
        popup->clear_children();

        const int max_visible = 8;
        const int item_h = 24;
        const int visible = (option_count_ < max_visible) ? option_count_ : max_visible;

        scroll_h_ = factory_.create_scroll_container();
        if (auto* sc = factory_.get_scroll_container(scroll_h_)) {
            sc->set_rect({2, 2, popup->get_rect().w - 4, visible * item_h + 4});
            factory_.link(popup_h_, scroll_h_);
        }

        list_h_ = factory_.create_list();
        if (auto* list = factory_.get_list(list_h_)) {
            list->set_rect({0, 0, popup->get_rect().w - 4, option_count_ * item_h});
            list->set_flex_layout(1, 0, 0, 0, 0);
            factory_.link(scroll_h_, list_h_);

            for (int i = 0; i < option_count_; ++i) {
                auto item_hndl = factory_.create_list_item(options_[i].c_str());
                if (auto* item = factory_.get_list_item(item_hndl)) {
                    item->set_size(list->get_rect().w, item_h);
                    factory_.link(list_h_, item_hndl);
                }
            }
            if (auto* sc = factory_.get_scroll_container(scroll_h_)) {
                sc->sync_child_bases([&](WidgetHandle h){ return factory_.get(h); });
            }
            set_list_selection([&](WidgetHandle h){ return factory_.get(h); },
                               [&](WidgetHandle h){ return factory_.get_list_item(h); },
                               list_h_, selected_);
        }
    }

    UiFactory& factory_;
    WidgetHandle host_{};
    WidgetHandle root_{};
    WidgetHandle popup_h_{};
    WidgetHandle scroll_h_{};
    WidgetHandle list_h_{};
    StaticString<32> options_[kMaxOptions]{};
    int option_count_{0};
    int selected_{0};
    Callback on_select_{};
};
