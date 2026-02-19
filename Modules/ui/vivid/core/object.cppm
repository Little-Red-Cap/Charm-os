module;
#include <cstddef>
export module charm.core.object;

export import charm.core.geometry;
export import charm.core.handle;
export import charm.gfx.canvas;
export import charm.core.event;

export
class ObjectBase {
public:
    virtual ~ObjectBase() = default;

    Rect get_rect() const noexcept { return rect_; }
    void set_pos(int x, int y) noexcept { rect_.x = x; rect_.y = y; }
    void set_size(int w, int h) noexcept { rect_.w = w; rect_.h = h; }
    void set_rect(Rect r) noexcept { rect_ = r; }

    void set_visible(bool v) noexcept {
        visible_ = v;
        if (!v) {
            set_state(State::Hovered, false);
            set_state(State::Pressed, false);
        }
    }
    bool is_visible() const noexcept { return visible_; }

    enum class State : unsigned {
        None      = 0,
        Hovered   = 1 << 0,
        Pressed   = 1 << 1,
        Focused   = 1 << 2,
        Disabled  = 1 << 3
    };

    enum class LayoutMode : unsigned {
        Anchor,
        Flex,
        Flow,
        Grid
    };

    void set_state(State s, bool on) noexcept {
        if (on) state_ = static_cast<State>(static_cast<unsigned>(state_) | static_cast<unsigned>(s));
        else    state_ = static_cast<State>(static_cast<unsigned>(state_) & ~static_cast<unsigned>(s));
    }

    bool has_state(State s) const noexcept {
        return (static_cast<unsigned>(state_) & static_cast<unsigned>(s)) != 0;
    }

    void set_enabled(bool on) noexcept {
        set_state(State::Disabled, !on);
        if (!on) {
            set_state(State::Hovered, false);
            set_state(State::Pressed, false);
        }
    }
    bool is_enabled() const noexcept { return !has_state(State::Disabled); }

    void set_focusable(bool on) noexcept { focusable_ = on; }
    bool is_focusable() const noexcept { return focusable_; }

    void set_flex_layout(int flow, int main_align, int cross_align, int gap, int padding) noexcept {
        flex_enabled_ = true;
        layout_mode_ = LayoutMode::Flex;
        flex_flow_ = flow;
        flex_main_align_ = main_align;
        flex_cross_align_ = cross_align;
        flex_gap_ = gap;
        flex_padding_ = padding;
    }

    void set_flex_grow(int grow) noexcept { flex_grow_ = (grow > 0) ? grow : 0; }
    int flex_grow() const noexcept { return flex_grow_; }

    void set_flow_layout(int gap, int line_gap, int padding) noexcept {
        layout_mode_ = LayoutMode::Flow;
        flow_gap_ = gap;
        flow_line_gap_ = line_gap;
        flow_padding_ = padding;
    }

    int flow_gap() const noexcept { return flow_gap_; }
    int flow_line_gap() const noexcept { return flow_line_gap_; }
    int flow_padding() const noexcept { return flow_padding_; }

    void set_grid_layout(int columns, int cell_w, int cell_h, int gap, int padding) noexcept {
        layout_mode_ = LayoutMode::Grid;
        grid_cols_ = (columns > 0) ? columns : 1;
        grid_cell_w_ = cell_w;
        grid_cell_h_ = cell_h;
        grid_gap_ = gap;
        grid_padding_ = padding;
    }

    int grid_columns() const noexcept { return grid_cols_; }
    int grid_cell_width() const noexcept { return grid_cell_w_; }
    int grid_cell_height() const noexcept { return grid_cell_h_; }
    int grid_gap() const noexcept { return grid_gap_; }
    int grid_padding() const noexcept { return grid_padding_; }

    void set_anchor(int left, int top, int right, int bottom) noexcept {
        anchor_enabled_ = true;
        anchor_left_ = left;
        anchor_top_ = top;
        anchor_right_ = right;
        anchor_bottom_ = bottom;
    }

    void clear_anchor() noexcept { anchor_enabled_ = false; }
    bool has_anchor() const noexcept { return anchor_enabled_; }
    int anchor_left() const noexcept { return anchor_left_; }
    int anchor_top() const noexcept { return anchor_top_; }
    int anchor_right() const noexcept { return anchor_right_; }
    int anchor_bottom() const noexcept { return anchor_bottom_; }

    void set_percent_size(int w_percent, int h_percent) noexcept {
        percent_w_ = w_percent;
        percent_h_ = h_percent;
    }

    void clear_percent_size() noexcept { percent_w_ = -1; percent_h_ = -1; }
    int percent_width() const noexcept { return percent_w_; }
    int percent_height() const noexcept { return percent_h_; }
    bool has_percent_size() const noexcept { return percent_w_ >= 0 || percent_h_ >= 0; }

    void set_min_size(int w, int h) noexcept { min_w_ = w; min_h_ = h; }
    void clear_min_size() noexcept { min_w_ = 0; min_h_ = 0; }
    int min_width() const noexcept { return min_w_; }
    int min_height() const noexcept { return min_h_; }

    void set_max_size(int w, int h) noexcept { max_w_ = w; max_h_ = h; }
    void clear_max_size() noexcept { max_w_ = 0; max_h_ = 0; }
    int max_width() const noexcept { return max_w_; }
    int max_height() const noexcept { return max_h_; }

    void set_align(int h, int v) noexcept { align_h_ = h; align_v_ = v; }
    int align_h() const noexcept { return align_h_; }
    int align_v() const noexcept { return align_v_; }

    bool has_flex_layout() const noexcept { return flex_enabled_; }
    int flex_flow() const noexcept { return flex_flow_; }
    int flex_main_align() const noexcept { return flex_main_align_; }
    int flex_cross_align() const noexcept { return flex_cross_align_; }
    int flex_gap() const noexcept { return flex_gap_; }
    int flex_padding() const noexcept { return flex_padding_; }
    LayoutMode layout_mode() const noexcept { return layout_mode_; }

    bool add_child(WidgetHandle child) noexcept {
        if (child_count_ >= kMaxChildren) return false;
        children_[child_count_++] = child;
        return true;
    }

    void clear_children() noexcept {
        for (std::size_t i = 0; i < child_count_; ++i) {
            children_[i] = {};
        }
        child_count_ = 0;
    }

    bool remove_child(WidgetHandle child) noexcept {
        for (std::size_t i = 0; i < child_count_; ++i) {
            if (children_[i] == child) {
                for (std::size_t j = i + 1; j < child_count_; ++j) {
                    children_[j - 1] = children_[j];
                }
                children_[child_count_ - 1] = {};
                --child_count_;
                return true;
            }
        }
        return false;
    }

    bool insert_child_before(WidgetHandle child, WidgetHandle before) noexcept {
        if (child_count_ >= kMaxChildren) return false;
        std::size_t idx = child_count_;
        for (std::size_t i = 0; i < child_count_; ++i) {
            if (children_[i] == before) { idx = i; break; }
        }
        if (idx == child_count_) {
            return add_child(child);
        }
        for (std::size_t i = child_count_; i > idx; --i) {
            children_[i] = children_[i - 1];
        }
        children_[idx] = child;
        ++child_count_;
        return true;
    }

    bool insert_child_after(WidgetHandle child, WidgetHandle after) noexcept {
        if (child_count_ >= kMaxChildren) return false;
        std::size_t idx = child_count_;
        for (std::size_t i = 0; i < child_count_; ++i) {
            if (children_[i] == after) { idx = i + 1; break; }
        }
        if (idx >= child_count_) {
            return add_child(child);
        }
        for (std::size_t i = child_count_; i > idx; --i) {
            children_[i] = children_[i - 1];
        }
        children_[idx] = child;
        ++child_count_;
        return true;
    }

    bool move_child_to_front(WidgetHandle child) noexcept {
        if (child_count_ <= 1) return true;
        std::size_t idx = child_count_;
        for (std::size_t i = 0; i < child_count_; ++i) {
            if (children_[i] == child) { idx = i; break; }
        }
        if (idx == child_count_ || idx == child_count_ - 1) return true;
        auto temp = children_[idx];
        for (std::size_t i = idx + 1; i < child_count_; ++i) {
            children_[i - 1] = children_[i];
        }
        children_[child_count_ - 1] = temp;
        return true;
    }

    bool move_child_to_back(WidgetHandle child) noexcept {
        if (child_count_ <= 1) return true;
        std::size_t idx = child_count_;
        for (std::size_t i = 0; i < child_count_; ++i) {
            if (children_[i] == child) { idx = i; break; }
        }
        if (idx == child_count_ || idx == 0) return true;
        auto temp = children_[idx];
        for (std::size_t i = idx; i > 0; --i) {
            children_[i] = children_[i - 1];
        }
        children_[0] = temp;
        return true;
    }

    void set_parent(WidgetHandle parent) noexcept { parent_ = parent; }
    WidgetHandle parent() const noexcept { return parent_; }

    std::size_t child_count() const noexcept { return child_count_; }
    WidgetHandle child_at(std::size_t i) const noexcept { return (i < child_count_) ? children_[i] : WidgetHandle{}; }
    bool has_child(WidgetHandle child) const noexcept { return child_index(child) < child_count_; }
    std::size_t child_index(WidgetHandle child) const noexcept {
        for (std::size_t i = 0; i < child_count_; ++i) {
            if (children_[i] == child) return i;
        }
        return child_count_;
    }

    virtual void draw(DefaultCanvas& cvs) = 0;

    virtual bool on_event(const Event&) { return false; }

    virtual bool should_draw_child(const ObjectBase&) const noexcept { return true; }

protected:
    static constexpr std::size_t kMaxChildren = 64;

    Rect rect_{};
    bool visible_{true};
    State state_{State::None};
    bool focusable_{false};
    bool flex_enabled_{false};
    int flex_flow_{0};
    int flex_main_align_{0};
    int flex_cross_align_{0};
    int flex_gap_{0};
    int flex_padding_{0};
    int flex_grow_{0};
    LayoutMode layout_mode_{LayoutMode::Anchor};
    int flow_gap_{0};
    int flow_line_gap_{0};
    int flow_padding_{0};
    int grid_cols_{1};
    int grid_cell_w_{0};
    int grid_cell_h_{0};
    int grid_gap_{0};
    int grid_padding_{0};
    bool anchor_enabled_{false};
    int anchor_left_{0};
    int anchor_top_{0};
    int anchor_right_{0};
    int anchor_bottom_{0};
    int percent_w_{-1};
    int percent_h_{-1};
    int min_w_{0};
    int min_h_{0};
    int max_w_{0};
    int max_h_{0};
    int align_h_{0};
    int align_v_{0};
    WidgetHandle parent_{};
    WidgetHandle children_[kMaxChildren]{};
    std::size_t child_count_{0};
};

export
constexpr ObjectBase::State operator|(ObjectBase::State a, ObjectBase::State b) noexcept {
    return static_cast<ObjectBase::State>(static_cast<unsigned>(a) | static_cast<unsigned>(b));
}
