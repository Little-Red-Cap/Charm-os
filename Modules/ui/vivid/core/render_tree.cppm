module;
#include <cstddef>
export module charm.core.render_tree;

export import charm.core.geometry;
export import charm.core.handle;

export
struct RenderNode {
    WidgetHandle handle{};
    Rect rect{};
    Rect draw_clip{};
    Rect clip{};
    bool clip_enabled{false};
    std::size_t parent{static_cast<std::size_t>(-1)};
    std::size_t first_child{0};
    std::size_t child_count{0};
};

export
template <std::size_t MaxNodes>
class RenderTree {
public:
    static constexpr std::size_t kMax = MaxNodes;

    void clear() noexcept {
        count_ = 0;
        overflow_ = false;
    }

    bool overflow() const noexcept { return overflow_; }
    std::size_t size() const noexcept { return count_; }
    const RenderNode& node(std::size_t idx) const noexcept { return nodes_[idx]; }
    RenderNode& node_mut(std::size_t idx) noexcept { return nodes_[idx]; }

    std::size_t push(const RenderNode& node) noexcept {
        if (count_ >= kMax) {
            overflow_ = true;
            return static_cast<std::size_t>(-1);
        }
        nodes_[count_] = node;
        return count_++;
    }

private:
    RenderNode nodes_[kMax]{};
    std::size_t count_{0};
    bool overflow_{false};
};
