module;
#include <cstddef>
#include <cstdint>
export module charm.widgets.dropdown_popup;

import charm.core.handle;
import charm.core.soa_factory;
import charm.core.event;
import charm.core.geometry;
import charm.core.string;
import service.signal;
import service.state;

// Dropdown popup backed by a list view.
export
class DropdownPopup {
public:
    static constexpr int kMaxOptions = 16;
    static constexpr int kRowHeight = 24;
    static constexpr int kItemHeight = kRowHeight;
    static constexpr int kMaxVisible = 8;

    using selected_state_type = service::state<int, 4>;
    using selected_slot_type = typename selected_state_type::slot_type;
    using selected_connection = typename selected_state_type::connection;
    using select_signal_type = service::signal<void(const int&), 4>;
    using select_slot_type = typename select_signal_type::slot_type;
    using select_connection = typename select_signal_type::connection;

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
        int selected = selected_.get();
        if (selected >= option_count_) {
            selected = (option_count_ > 0) ? (option_count_ - 1) : 0;
            (void)selected_.set(selected);
        }
        highlighted_ = selected;
        sync_list_source();
    }

    void open() {
        const Rect host = factory_.kernel().world_rect(host_);
        open_at(host.x, host.y + host.h, host.w);
    }

    void open(int x, int y, int w) {
        open_at(x, y, w);
    }

    void open_at(int x, int y, int w) {
        const int visible = (option_count_ < kMaxVisible) ? option_count_ : kMaxVisible;
        factory_.kernel().set_rect(popup_container_, {x, y, w, visible * kRowHeight});
        factory_.kernel().set_rect(popup_list_, {0, 0, w, visible * kRowHeight});
        factory_.kernel().set_visible(popup_container_, true);
        is_open_ = true;
        highlighted_ = selected_.get();
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
        (void)selected_.set(idx);
        highlighted_ = idx;
        sync_list_selection();
    }

    [[nodiscard]] int selected() const noexcept {
        return selected_.get();
    }

    [[nodiscard]] auto observe_selected(selected_slot_type slot) noexcept {
        return selected_.connect(slot);
    }

    [[nodiscard]] bool unobserve_selected(selected_connection c) noexcept {
        return selected_.disconnect(c);
    }

    [[nodiscard]] auto observe_select(select_slot_type slot) noexcept {
        return selected_edge_.connect(slot);
    }

    [[nodiscard]] bool unobserve_select(select_connection c) noexcept {
        return selected_edge_.disconnect(c);
    }

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
                    commit_selection(idx);
                }
                close();
                return true;
            }
            close();
            return true;
        } else if (e.type == Event::Type::MouseMove) {
            if (r.contains(e.x, e.y)) {
                const int idx = index_from_pos(e.y);
                if (idx >= 0 && idx < option_count_) {
                    set_highlighted(idx);
                }
                return true;
            }
        } else if (e.type == Event::Type::MouseWheel) {
            if (r.contains(e.x, e.y)) {
                if (e.wheel_y == 0) {
                    return true;
                }
                const int dir = (e.wheel_y < 0) ? 1 : -1;
                if (option_count_ > 0) {
                    const int current = normalized_highlighted();
                    set_highlighted((current + dir + option_count_) % option_count_);
                }
                return true;
            }
        } else if (e.type == Event::Type::KeyDown) {
            if (option_count_ == 0) return false;
            if (e.key_code == Event::Key::Down) {
                const int current = normalized_highlighted();
                set_highlighted((current + 1) % option_count_);
                return true;
            }
            if (e.key_code == Event::Key::Up) {
                const int current = normalized_highlighted();
                set_highlighted((current - 1 + option_count_) % option_count_);
                return true;
            }
            if (e.key_code == Event::Key::Enter || e.key_code == Event::Key::Space) {
                commit_selection(normalized_highlighted());
                close();
                return true;
            }
            if (e.key_code == Event::Key::Escape) {
                close();
                return true;
            }
        }
        return false;
    }

private:
    void sync_list_source() {
        factory_.set_list_view_items(popup_list_, items_ptrs(), static_cast<std::uint16_t>(option_count_));
        sync_list_selection();
    }

    void sync_list_selection() {
        if (option_count_ <= 0) {
            factory_.kernel().set_list_view_selected(popup_list_, -1);
            return;
        }
        factory_.kernel().set_list_view_selected(popup_list_, normalized_highlighted());
    }

    [[nodiscard]] int normalized_highlighted() const noexcept {
        if (option_count_ <= 0) return 0;
        if (highlighted_ < 0) return 0;
        if (highlighted_ >= option_count_) return option_count_ - 1;
        return highlighted_;
    }

    void set_highlighted(int idx) noexcept {
        if (idx < 0 || idx >= option_count_) return;
        highlighted_ = idx;
        sync_list_selection();
    }

    void commit_selection(int idx) noexcept {
        if (idx < 0 || idx >= option_count_) return;
        highlighted_ = idx;
        (void)selected_.set(idx);
        sync_list_selection();
        const int committed = selected_.get();
        (void)selected_edge_.emit(committed);
        if (on_select_) on_select_();
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
    selected_state_type selected_{0};
    int highlighted_{0};
    bool is_open_{false};
    select_signal_type selected_edge_{};
    Callback on_select_{};
};
