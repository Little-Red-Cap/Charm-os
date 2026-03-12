module;
#include <cstddef>
#include <cstdint>
export module charm.widgets.dropdown_popup;

import charm.core.handle;
import charm.core.soa_factory;
import charm.core.event;
import charm.core.geometry;
import charm.core.string;

// Dropdown + Popup + ListItem ???????????
export
class DropdownPopup {
public:
    static constexpr int kMaxOptions = 16;
    static constexpr int kRowHeight = 24;
    static constexpr int kMaxVisible = 8;

    DropdownPopup(SoaFactory& factory, WidgetHandle host, WidgetHandle root)
        : factory_(factory), host_(host), root_(root) {
        popup_container_ = factory_.create_container();
        popup_list_ = factory_.create_list_view();
        factory_.kernel().set_list_row_height(popup_list_, kRowHeight);
        factory_.kernel().set_visible(popup_container_, false);
        factory_.kernel().set_visible(popup_list_, true);
        factory_.link(popup_container_, popup_list_);
        factory_.link(root_, popup_container_);
    }

    void set_options(const char* const* opts, int count) {
        option_count_ = 0;
        const int cap = (count < kMaxOptions) ? count : kMaxOptions;
        for (int i = 0; i < cap; ++i) {
            options_[i].assign(opts[i] ? opts[i] : "");
            ++option_count_;
        }
        if (selected_ >= option_count_) {
            selected_ = (option_count_ > 0) ? (option_count_ - 1) : 0;
        }
        sync_list_source();
    }

    void open() {
        Rect host = factory_.kernel().world_rect(host_);
        open_at(host.x, host.y + host.h, host.w);
    }

    void open_at(int x, int y, int w) {
        const int visible = (option_count_ < kMaxVisible) ? option_count_ : kMaxVisible;
        factory_.kernel().set_rect(popup_container_, {x, y, w, visible * kRowHeight});
        factory_.kernel().set_rect(popup_list_, {0, 0, w, visible * kRowHeight});
        factory_.kernel().set_visible(popup_container_, true);
        is_open_ = true;
        sync_list_source();
    }

    void close() {
        if (!is_open_) return;
        if (owns_input_state()) {
            factory_.kernel().input_request_cancel();
        }
        factory_.kernel().set_visible(popup_container_, false);
        is_open_ = false;
    }

    bool is_open() const noexcept {
        return is_open_;
    }

    void set_on_select(Callback cb) { on_select_ = cb; }
    void set_selection(int idx) {
        if (idx < 0 || idx >= option_count_) return;
        selected_ = idx;
        factory_.kernel().set_list_view_selected(popup_list_, selected_);
    }

    int selected() const noexcept { return selected_; }

    bool handle_event(const Event& e) {
        if (!is_open()) return false;
        const Rect r = factory_.kernel().rect(popup_container_);
        if (e.type == Event::Type::MouseDown) {
            if (!r.contains(e.x, e.y)) {
                close();
                return true;
            }
        } else if (e.type == Event::Type::Click) {
            if (r.contains(e.x, e.y)) {
                const int idx = index_from_pos(e.y);
                if (idx >= 0 && idx < option_count_) {
                    selected_ = idx;
                    factory_.kernel().set_list_view_selected(popup_list_, selected_);
                    if (on_select_) on_select_();
                }
                close();
                return true;
            } else {
                close();
                return true;
            }
        } else if (e.type == Event::Type::MouseMove) {
            if (r.contains(e.x, e.y)) {
                const int idx = index_from_pos(e.y);
                if (idx >= 0 && idx < option_count_) {
                    selected_ = idx;
                    factory_.kernel().set_list_view_selected(popup_list_, selected_);
                }
                return true;
            }
        } else if (e.type == Event::Type::MouseWheel) {
            if (r.contains(e.x, e.y)) {
                const int dir = (e.wheel_y < 0) ? 1 : -1;
                if (option_count_ > 0) {
                    selected_ = (selected_ + dir + option_count_) % option_count_;
                    factory_.kernel().set_list_view_selected(popup_list_, selected_);
                }
                return true;
            }
        } else if (e.type == Event::Type::KeyDown) {
            if (option_count_ == 0) return false;
            if (e.key_code == Event::Key::Down) {
                selected_ = (selected_ + 1) % option_count_;
                factory_.kernel().set_list_view_selected(popup_list_, selected_);
                return true;
            } else if (e.key_code == Event::Key::Up) {
                selected_ = (selected_ - 1 + option_count_) % option_count_;
                factory_.kernel().set_list_view_selected(popup_list_, selected_);
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
    void sync_list_source() {
        factory_.set_list_view_items(popup_list_, items_ptrs(), static_cast<std::uint16_t>(option_count_));
        factory_.kernel().set_list_view_selected(popup_list_, selected_);
    }

    int index_from_pos(int y) const noexcept {
        const Rect list_rect = factory_.kernel().world_rect(popup_list_);
        const int row_h = factory_.kernel().list_row_height(popup_list_);
        const int scroll = factory_.kernel().scroll_y(popup_list_);
        const int local_y = y - list_rect.y + scroll;
        if (local_y < 0 || row_h <= 0) return -1;
        return local_y / row_h;
    }

    const char* const* items_ptrs() noexcept {
        for (int i = 0; i < option_count_; ++i) {
            item_ptrs_[i] = options_[i].c_str();
        }
        return item_ptrs_;
    }

    bool owns_input_state() const noexcept {
        const auto& kernel = factory_.kernel();
        const WidgetHandle captured = kernel.input_captured();
        const WidgetHandle pressed = kernel.input_pressed();
        return captured == popup_container_
            || captured == popup_list_
            || pressed == popup_container_
            || pressed == popup_list_;
    }

    SoaFactory& factory_;
    WidgetHandle host_{};
    WidgetHandle root_{};
    WidgetHandle popup_container_{};
    WidgetHandle popup_list_{};
    StaticString<32> options_[kMaxOptions]{};
    const char* item_ptrs_[kMaxOptions]{};
    int option_count_{0};
    int selected_{0};
    bool is_open_{false};
    Callback on_select_{};
};
