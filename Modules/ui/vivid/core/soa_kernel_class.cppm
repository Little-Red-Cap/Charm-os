module;

#include "vivid_features.generated.hpp"

#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <utility>

#if defined(CHARM_VIVID_UNSUPPORTED_WIDGET_DIAG)
extern "C" void charm_vivid_soa_unsupported_widget_kind(unsigned kind, unsigned caller) noexcept;
#endif

export module charm.core.soa_kernel:kernel_class;

import :types;
import :input;
import charm.core.handle;
import charm.core.geometry;
import charm.core.config;
import charm.core.event;
import charm.core.widget_registry;
import charm.core.soa_registry;

import charm.core.style;
import charm.core.style_sheet;
import charm.core.soa_payload;
import alg_list_scroll;
import charm.gfx.text_box;

export constexpr std::uint16_t kInvalidIndex = 0xFFFF;

struct ScrollBarTrackInfo;

namespace soa_detail {
    template <typename Tag>
    class CompactSlotId {
    public:
        using value_type = std::uint8_t;

        constexpr CompactSlotId() noexcept = default;

        [[nodiscard]] static constexpr CompactSlotId from_index(std::size_t index) noexcept {
            return CompactSlotId{static_cast<value_type>(index)};
        }

        [[nodiscard]] static constexpr std::size_t max_capacity() noexcept {
            return static_cast<std::size_t>(kInvalidValue);
        }

        [[nodiscard]] constexpr bool valid() const noexcept {
            return value_ != kInvalidValue;
        }

        [[nodiscard]] constexpr std::size_t index() const noexcept {
            return static_cast<std::size_t>(value_);
        }

        constexpr void reset() noexcept {
            value_ = kInvalidValue;
        }

    private:
        static constexpr value_type kInvalidValue = 0xFF;

        constexpr explicit CompactSlotId(value_type value) noexcept
            : value_{value} {}

        value_type value_{kInvalidValue};
    };

    struct StylePatchSlotTag {};
    struct SemanticSlotTag {};
    using StylePatchSlot = CompactSlotId<StylePatchSlotTag>;
    using SemanticSlot = CompactSlotId<SemanticSlotTag>;

    static_assert(sizeof(StylePatchSlot) == 1);
    static_assert(sizeof(SemanticSlot) == 1);
    static_assert(std::is_trivially_copyable_v<StylePatchSlot>);
    static_assert(std::is_trivially_copyable_v<SemanticSlot>);

    template <std::size_t Capacity>
    class StylePatchPool {
    public:
        static_assert(Capacity > 0);
        static_assert(Capacity <= StylePatchSlot::max_capacity());

        void reset() noexcept {
            for (std::size_t i = 0; i < Capacity; ++i) {
                patches_[i] = StylePatch{};
                kinds_[i] = StylePatchKind::None;
                free_next_[i] = (i + 1 < Capacity)
                    ? StylePatchSlot::from_index(i + 1)
                    : StylePatchSlot{};
            }
            free_head_ = StylePatchSlot::from_index(0);
            live_count_ = 0;
            peak_count_ = 0;
            alloc_fail_ = 0;
            overflowed_ = false;
        }

        [[nodiscard]] bool set(StylePatchSlot& slot,
                               const StylePatch& patch,
                               StylePatchKind kind) noexcept {
            assert(kind != StylePatchKind::None);
            if (!slot.valid()) {
                if (!free_head_.valid()) {
                    overflowed_ = true;
                    ++alloc_fail_;
                    return false;
                }
                slot = free_head_;
                free_head_ = free_next_[slot.index()];
                ++live_count_;
                if (live_count_ > peak_count_) peak_count_ = live_count_;
            }
            const std::size_t index = slot.index();
            assert(index < Capacity);
            patches_[index] = patch;
            kinds_[index] = kind;
            return true;
        }

        [[nodiscard]] bool clear(StylePatchSlot& slot) noexcept {
            if (!slot.valid()) return false;
            const std::size_t index = slot.index();
            assert(index < Capacity);
            kinds_[index] = StylePatchKind::None;
            free_next_[index] = free_head_;
            free_head_ = slot;
            slot.reset();
            assert(live_count_ > 0);
            --live_count_;
            return true;
        }

        [[nodiscard]] const StylePatch* get(StylePatchSlot slot) const noexcept {
            if (!slot.valid() || slot.index() >= Capacity) return nullptr;
            const std::size_t index = slot.index();
            return kinds_[index] == StylePatchKind::None ? nullptr : &patches_[index];
        }

        [[nodiscard]] StylePatchKind kind(StylePatchSlot slot) const noexcept {
            if (!slot.valid() || slot.index() >= Capacity) return StylePatchKind::None;
            return kinds_[slot.index()];
        }

        [[nodiscard]] bool overflowed() const noexcept { return overflowed_; }
        [[nodiscard]] std::size_t live_count() const noexcept { return live_count_; }
        [[nodiscard]] std::size_t peak_count() const noexcept { return peak_count_; }
        [[nodiscard]] std::uint32_t alloc_fail() const noexcept { return alloc_fail_; }
        [[nodiscard]] static constexpr std::size_t capacity() noexcept { return Capacity; }

    private:
        std::array<StylePatch, Capacity> patches_{};
        std::array<StylePatchSlot, Capacity> free_next_{};
        std::array<StylePatchKind, Capacity> kinds_{};
        StylePatchSlot free_head_{};
        std::uint16_t live_count_{0};
        std::uint16_t peak_count_{0};
        std::uint32_t alloc_fail_{0};
        bool overflowed_{false};
    };

    struct alignas(4) SemanticRecord {
        TextId id{};
        TextId label{};
        SemanticRole role{SemanticRole::None};
        SemanticActionMask actions{0};
    };

    static_assert(sizeof(SemanticRecord) <= 12);
    static_assert(std::is_trivially_copyable_v<SemanticRecord>);

    template <std::size_t Capacity>
    class SemanticPool {
    public:
        static_assert(Capacity > 0);
        static_assert(Capacity <= SemanticSlot::max_capacity());

        void reset() noexcept {
            for (std::size_t i = 0; i < Capacity; ++i) {
                records_[i] = SemanticRecord{};
                free_next_[i] = (i + 1 < Capacity)
                    ? SemanticSlot::from_index(i + 1)
                    : SemanticSlot{};
            }
            free_head_ = SemanticSlot::from_index(0);
            live_count_ = 0;
            peak_count_ = 0;
            alloc_fail_ = 0;
            overflowed_ = false;
        }

        [[nodiscard]] SemanticRecord* acquire(SemanticSlot& slot) noexcept {
            if (!slot.valid()) {
                if (!free_head_.valid()) {
                    overflowed_ = true;
                    ++alloc_fail_;
                    return nullptr;
                }
                slot = free_head_;
                free_head_ = free_next_[slot.index()];
                records_[slot.index()] = SemanticRecord{};
                ++live_count_;
                if (live_count_ > peak_count_) peak_count_ = live_count_;
            }
            const std::size_t index = slot.index();
            assert(index < Capacity);
            return &records_[index];
        }

        [[nodiscard]] bool clear(SemanticSlot& slot) noexcept {
            if (!slot.valid()) return false;
            const std::size_t index = slot.index();
            assert(index < Capacity);
            records_[index] = SemanticRecord{};
            free_next_[index] = free_head_;
            free_head_ = slot;
            slot.reset();
            assert(live_count_ > 0);
            --live_count_;
            return true;
        }

        [[nodiscard]] SemanticRecord* get(SemanticSlot slot) noexcept {
            return (!slot.valid() || slot.index() >= Capacity)
                ? nullptr
                : &records_[slot.index()];
        }

        [[nodiscard]] const SemanticRecord* get(SemanticSlot slot) const noexcept {
            return (!slot.valid() || slot.index() >= Capacity)
                ? nullptr
                : &records_[slot.index()];
        }

        [[nodiscard]] bool overflowed() const noexcept { return overflowed_; }
        [[nodiscard]] std::size_t live_count() const noexcept { return live_count_; }
        [[nodiscard]] std::size_t peak_count() const noexcept { return peak_count_; }
        [[nodiscard]] std::uint32_t alloc_fail() const noexcept { return alloc_fail_; }
        [[nodiscard]] static constexpr std::size_t capacity() noexcept { return Capacity; }

    private:
        std::array<SemanticRecord, Capacity> records_{};
        std::array<SemanticSlot, Capacity> free_next_{};
        SemanticSlot free_head_{};
        std::uint16_t live_count_{0};
        std::uint16_t peak_count_{0};
        std::uint32_t alloc_fail_{0};
        bool overflowed_{false};
    };

    struct NodeLayoutTextState {
        static constexpr std::uint8_t kValueMask = 0x03;
        static constexpr std::uint8_t kLayoutShift = 0;
        static constexpr std::uint8_t kAlignHShift = 2;
        static constexpr std::uint8_t kAlignVShift = 4;
        static constexpr std::uint8_t kLayoutMask = kValueMask << kLayoutShift;
        static constexpr std::uint8_t kAlignHMask = kValueMask << kAlignHShift;
        static constexpr std::uint8_t kAlignVMask = kValueMask << kAlignVShift;

        static_assert(static_cast<std::uint8_t>(SoaLayoutKind::List) <= kValueMask);
        static_assert(static_cast<std::uint8_t>(TextAlignH::Right) <= kValueMask);
        static_assert(static_cast<std::uint8_t>(TextAlignV::Bottom) <= kValueMask);

        constexpr void reset(SoaLayoutKind layout = SoaLayoutKind::None) noexcept {
            bits_ = static_cast<std::uint8_t>(
                static_cast<std::uint8_t>(TextAlignV::Center) << kAlignVShift);
            set_layout_kind(layout);
        }

        constexpr void set_layout_kind(SoaLayoutKind layout) noexcept {
            const auto value = static_cast<std::uint8_t>(layout);
            if (value > kValueMask) return;
            bits_ = static_cast<std::uint8_t>(
                (bits_ & static_cast<std::uint8_t>(~kLayoutMask))
                | static_cast<std::uint8_t>(value << kLayoutShift));
        }

        [[nodiscard]] constexpr SoaLayoutKind layout_kind() const noexcept {
            return static_cast<SoaLayoutKind>((bits_ & kLayoutMask) >> kLayoutShift);
        }

        constexpr void set_text_align(TextAlignH align_h, TextAlignV align_v) noexcept {
            const auto h = static_cast<std::uint8_t>(align_h);
            const auto v = static_cast<std::uint8_t>(align_v);
            if (h > kValueMask || v > kValueMask) return;
            constexpr auto align_mask = static_cast<std::uint8_t>(kAlignHMask | kAlignVMask);
            bits_ = static_cast<std::uint8_t>(
                (bits_ & static_cast<std::uint8_t>(~align_mask))
                | static_cast<std::uint8_t>(h << kAlignHShift)
                | static_cast<std::uint8_t>(v << kAlignVShift));
        }

        [[nodiscard]] constexpr TextAlignH text_align_h() const noexcept {
            return static_cast<TextAlignH>((bits_ & kAlignHMask) >> kAlignHShift);
        }

        [[nodiscard]] constexpr TextAlignV text_align_v() const noexcept {
            return static_cast<TextAlignV>((bits_ & kAlignVMask) >> kAlignVShift);
        }

    private:
        std::uint8_t bits_{
            static_cast<std::uint8_t>(TextAlignV::Center) << kAlignVShift};
    };

    static_assert(sizeof(NodeLayoutTextState) == 1);
    static_assert(std::is_trivially_copyable_v<NodeLayoutTextState>);

    class NodeSlotStorage {
    public:
        [[nodiscard]] constexpr std::uint16_t free_next() const noexcept {
            return value_;
        }

        constexpr void set_free_next(std::uint16_t next) noexcept {
            value_ = next;
        }

        [[nodiscard]] constexpr PayloadSlot payload_slot() const noexcept {
            return value_;
        }

        constexpr void set_payload_slot(PayloadSlot slot) noexcept {
            value_ = slot;
        }

    private:
        std::uint16_t value_{kInvalidIndex};
    };

    static_assert(sizeof(NodeSlotStorage) == sizeof(std::uint16_t));
    static_assert(std::is_trivially_copyable_v<NodeSlotStorage>);

    class NodeRuntimeState {
    public:
        constexpr void reset() noexcept {
            bits_ = 0;
        }

        [[nodiscard]] constexpr bool get(SoaNodeFlag flag) const noexcept {
            return (bits_ & static_cast<std::uint8_t>(flag)) != 0;
        }

        [[nodiscard]] constexpr bool get(SoaStateFlag flag) const noexcept {
            return (bits_ & static_cast<std::uint8_t>(flag)) != 0;
        }

        constexpr void set(SoaNodeFlag flag, bool on) noexcept {
            set_mask(static_cast<std::uint8_t>(flag), on);
        }

        constexpr void set(SoaStateFlag flag, bool on) noexcept {
            set_mask(static_cast<std::uint8_t>(flag), on);
        }

    private:
        constexpr void set_mask(std::uint8_t mask, bool on) noexcept {
            bits_ = on
                ? static_cast<std::uint8_t>(bits_ | mask)
                : static_cast<std::uint8_t>(bits_ & ~mask);
        }

        std::uint8_t bits_{0};
    };

    static_assert(sizeof(NodeRuntimeState) == 1);
    static_assert(std::is_trivially_copyable_v<NodeRuntimeState>);

    // ---- Storage / payload descriptor ----
    template <std::size_t N>
    struct CommonSoA {
        std::array<WidgetKind, N> kind{};
        std::array<std::uint16_t, N> generation{};
        std::array<NodeSlotStorage, N> storage_slot{};
        std::array<std::uint16_t, N> parent{};
        std::array<std::uint16_t, N> first_child{};
        std::array<std::uint16_t, N> next_sibling{};
        std::array<std::uint16_t, N> prev_sibling{};
        std::array<NodeRuntimeState, N> runtime_state{};
        std::array<Rect, N> rects{};
        std::array<NodeLayoutTextState, N> layout_text{};
        std::array<StylePatchSlot, N> style_patch_slot{};
        std::array<StyleClassId, N> style_class{};
        std::array<SemanticSlot, N> semantic_slot{};
#if CHARM_VIVID_DRAW_DETAIL_EVIDENCE
        std::array<std::uint16_t, N> draw_scope{};
#endif
    };

}

// ---- Kernel ----
export
class SoaKernel {
public:
    static constexpr std::size_t kMaxNodes = soa_max_nodes;
    static constexpr std::size_t kSemanticCapacity = semantic_slot_cap;
    static constexpr std::size_t kStylePatchCapacity = style_patch_slot_cap;
    static constexpr std::size_t kSemanticPoolBytes =
        sizeof(soa_detail::SemanticPool<kSemanticCapacity>);
    static constexpr std::size_t kStylePatchPoolBytes =
        sizeof(soa_detail::StylePatchPool<kStylePatchCapacity>);
    static constexpr std::size_t kNodeLayoutTextStateBytes =
        sizeof(soa_detail::NodeLayoutTextState);
    static constexpr std::size_t kNodeStorageSlotBytes =
        sizeof(soa_detail::NodeSlotStorage);
    static constexpr std::size_t kNodeRuntimeStateBytes =
        sizeof(soa_detail::NodeRuntimeState);
    static constexpr std::size_t kNodeStyleClassBytes = sizeof(StyleClassId);
    static constexpr std::size_t kNodeStylePatchSlotBytes =
        sizeof(soa_detail::StylePatchSlot);
    static constexpr std::size_t kNodeSemanticSlotBytes =
        sizeof(soa_detail::SemanticSlot);
    static_assert(kSemanticPoolBytes <= kSemanticCapacity * 16 + 32,
                  "SoA semantic pool exceeded its admitted capacity bound");

    struct TraversalFrame {
        static constexpr std::uint8_t kEntered = 1u << 0;
        static constexpr std::uint8_t kClipEnabled = 1u << 1;
        static constexpr std::uint8_t kClipPushed = 1u << 2;

        WidgetHandle h{};
        WidgetHandle child{};
        Rect clip_rect{};
        int offset_x{0};
        int offset_y{0};
        int child_offset_x{0};
        int child_offset_y{0};
        std::uint16_t depth{0};
        std::uint16_t draw_scope_id{0};
        std::uint8_t flags{0};

        constexpr bool entered() const noexcept { return (flags & kEntered) != 0; }
        constexpr bool clip_enabled() const noexcept { return (flags & kClipEnabled) != 0; }
        constexpr bool clip_pushed() const noexcept { return (flags & kClipPushed) != 0; }
        constexpr void set_entered() noexcept { flags |= kEntered; }
        constexpr void set_clip_enabled() noexcept { flags |= kClipEnabled; }
        constexpr void set_clip_pushed() noexcept { flags |= kClipPushed; }
    };

    enum class TraversalPhase : std::uint8_t {
        Idle,
        Layout,
        Render,
        HitTest,
        Focus,
        Semantic,
    };

    class TraversalLease {
        friend class SoaKernel;

    public:
        TraversalLease() noexcept = default;
        TraversalLease(const TraversalLease&) = delete;
        TraversalLease& operator=(const TraversalLease&) = delete;
        TraversalLease(TraversalLease&& other) noexcept
            : owner_(std::exchange(other.owner_, nullptr)), phase_(other.phase_) {}
        TraversalLease& operator=(TraversalLease&&) = delete;

        ~TraversalLease() noexcept {
            if (owner_) owner_->release_traversal(phase_);
        }

        explicit operator bool() const noexcept { return owner_ != nullptr; }

        std::array<TraversalFrame, kMaxNodes>& stack() const noexcept {
            assert(owner_ != nullptr);
            return owner_->traversal_stack_;
        }

    private:
        TraversalLease(const SoaKernel* owner, TraversalPhase phase) noexcept
            : owner_(owner), phase_(phase) {}

        const SoaKernel* owner_{nullptr};
        TraversalPhase phase_{TraversalPhase::Idle};
    };

    static_assert(sizeof(TraversalFrame) <= 64,
                  "SoA shared traversal frame exceeded its admitted per-node bound");
    static constexpr std::size_t kTraversalWorkspaceBytes =
        sizeof(std::array<TraversalFrame, kMaxNodes>);

    SoaKernel() noexcept;

    SoaKernel(const SoaKernel&) = delete;
    SoaKernel& operator=(const SoaKernel&) = delete;
    SoaKernel(SoaKernel&&) = delete;
    SoaKernel& operator=(SoaKernel&&) = delete;

    WidgetHandle create(WidgetKind kind) noexcept;

    void destroy(WidgetHandle h) noexcept;

    bool valid(WidgetHandle h) const noexcept;

    WidgetKind kind(WidgetHandle h) const noexcept;

    Rect rect(WidgetHandle h) const noexcept {
        const std::uint16_t idx = index_of(h);
        return (idx == kInvalidIndex) ? Rect{} : common_.rects[idx];
    }

    void set_rect(WidgetHandle h, const Rect& r) noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return;
        common_.rects[idx] = r;
        mark_layout_dirty();
    }

    void set_text_align(WidgetHandle h, TextAlignH align_h, TextAlignV align_v) noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return;
        common_.layout_text[idx].set_text_align(align_h, align_v);
    }

    TextAlignH text_align_h(WidgetHandle h) const noexcept {
        const std::uint16_t idx = index_of(h);
        return (idx == kInvalidIndex) ? TextAlignH::Left : common_.layout_text[idx].text_align_h();
    }

    TextAlignV text_align_v(WidgetHandle h) const noexcept {
        const std::uint16_t idx = index_of(h);
        return (idx == kInvalidIndex) ? TextAlignV::Center : common_.layout_text[idx].text_align_v();
    }

    bool link(WidgetHandle parent, WidgetHandle child) noexcept;

    bool unlink(WidgetHandle parent, WidgetHandle child) noexcept;

    WidgetHandle parent(WidgetHandle h) const noexcept;

    WidgetHandle first_child(WidgetHandle h) const noexcept;

    WidgetHandle last_child(WidgetHandle h) const noexcept;

    WidgetHandle next_sibling(WidgetHandle h) const noexcept;

    WidgetHandle prev_sibling(WidgetHandle h) const noexcept;

    std::size_t child_count(WidgetHandle h) const noexcept;

    void set_visible(WidgetHandle h, bool on) noexcept {
        set_flag(h, SoaNodeFlag::Visible, on);
    }

    bool visible(WidgetHandle h) const noexcept {
        return get_flag(h, SoaNodeFlag::Visible);
    }

    void set_enabled(WidgetHandle h, bool on) noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return;
        const bool prev = flag_raw(idx, SoaNodeFlag::Enabled);
        if (prev == on) return;
        common_.runtime_state[idx].set(SoaNodeFlag::Enabled, on);
        on_state_change(idx, SoaStateMask::Enabled);
    }

    bool enabled(WidgetHandle h) const noexcept {
        return get_flag(h, SoaNodeFlag::Enabled);
    }

    void set_focusable(WidgetHandle h, bool on) noexcept {
        set_flag(h, SoaNodeFlag::Focusable, on);
    }

    bool focusable(WidgetHandle h) const noexcept {
        return get_flag(h, SoaNodeFlag::Focusable);
    }

    void set_semantic(WidgetHandle h,
                      SemanticRole role,
                      const char* id,
                      const char* label) noexcept;

    void set_semantic_default(WidgetHandle h,
                              const char* id,
                              const char* label = nullptr) noexcept;

    void clear_semantic(WidgetHandle h) noexcept;

    void set_semantic_actions(WidgetHandle h, SemanticActionMask actions) noexcept;

    SemanticFocusSnapshot semantic_snapshot(WidgetHandle h) const noexcept;

    SemanticActionSnapshot semantic_action_snapshot(WidgetHandle h) const noexcept;

    SemanticFocusSnapshot semantic_focus_snapshot() const noexcept;

    SemanticIntentResolution resolve_semantic_intent(WidgetHandle root,
                                                     const char* id,
                                                     SemanticAction action) const noexcept;

    SemanticActionAdmission admit_semantic_action(WidgetHandle root,
                                                  const char* id,
                                                  SemanticAction action) const noexcept;

    SemanticActionRequest request_semantic_action(WidgetHandle root,
                                                  const char* id,
                                                  SemanticAction action) noexcept;

    SemanticFocusQuery query_semantic_focus(WidgetHandle root, const char* id) const noexcept;

    SemanticFocusAdmission admit_semantic_focus(WidgetHandle root, const char* id) const noexcept;

    SemanticFocusRequest request_semantic_focus(WidgetHandle root, const char* id) noexcept;

    SemanticTreeSnapshot semantic_tree_snapshot(
        WidgetHandle root,
        std::size_t max_nodes = kSemanticTreeMaxNodes) const noexcept;

    void set_hit_testable(WidgetHandle h, bool on) noexcept {
        set_flag(h, SoaNodeFlag::HitTest, on);
    }

    bool hit_testable(WidgetHandle h) const noexcept {
        return get_flag(h, SoaNodeFlag::HitTest);
    }

    void set_clip_children(WidgetHandle h, bool on) noexcept {
        set_flag(h, SoaNodeFlag::ClipChildren, on);
    }

    bool clip_children(WidgetHandle h) const noexcept {
        return get_flag(h, SoaNodeFlag::ClipChildren);
    }

    void set_hovered(WidgetHandle h, bool on) noexcept {
        input_guard_state_write("hovered");
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return;
        const bool prev = common_.runtime_state[idx].get(SoaStateFlag::Hovered);
        if (prev == on) return;
        set_state_flag(h, SoaStateFlag::Hovered, on);
        on_state_change(idx, SoaStateMask::Hovered);
    }

    void set_pressed(WidgetHandle h, bool on) noexcept {
        input_guard_state_write("pressed");
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return;
        const bool prev = common_.runtime_state[idx].get(SoaStateFlag::Pressed);
        if (prev == on) return;
        set_state_flag(h, SoaStateFlag::Pressed, on);
        on_state_change(idx, SoaStateMask::Pressed);
    }

    void set_focused(WidgetHandle h, bool on) noexcept {
        input_guard_state_write("focused");
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return;
        const bool prev = common_.runtime_state[idx].get(SoaStateFlag::Focused);
        if (prev == on) return;
        set_state_flag(h, SoaStateFlag::Focused, on);
        on_state_change(idx, SoaStateMask::Focused);
    }

    bool hovered(WidgetHandle h) const noexcept {
        return get_state_flag(h, SoaStateFlag::Hovered);
    }

    bool pressed(WidgetHandle h) const noexcept {
        return get_state_flag(h, SoaStateFlag::Pressed);
    }

    bool focused(WidgetHandle h) const noexcept {
        return get_state_flag(h, SoaStateFlag::Focused);
    }

    StateCompact state_compact(WidgetHandle h) const noexcept {
        const std::uint16_t idx = index_of(h);
        if (idx == kInvalidIndex) return {};
        std::uint8_t bits = 0;
        if (flag_raw(idx, SoaNodeFlag::Enabled)) {
            bits = static_cast<std::uint8_t>(SoaStateMask::Enabled);
        }
        const auto& runtime_state = common_.runtime_state[idx];
        if (runtime_state.get(SoaStateFlag::Hovered)) {
            bits = static_cast<std::uint8_t>(bits | static_cast<std::uint8_t>(SoaStateMask::Hovered));
        }
        if (runtime_state.get(SoaStateFlag::Pressed)) {
            bits = static_cast<std::uint8_t>(bits | static_cast<std::uint8_t>(SoaStateMask::Pressed));
        }
        if (runtime_state.get(SoaStateFlag::Focused)) {
            bits = static_cast<std::uint8_t>(bits | static_cast<std::uint8_t>(SoaStateMask::Focused));
        }
        return StateCompact{bits};
    }

    void set_input_root(WidgetHandle root) noexcept {
        input_.root = root;
    }

    WidgetHandle input_root() const noexcept {
        return input_.root;
    }

    WidgetHandle input_hovered() const noexcept {
        return input_.hovered;
    }

    WidgetHandle input_pressed() const noexcept {
        return input_.pressed;
    }

    WidgetHandle input_focused() const noexcept {
        return input_.focused;
    }

    WidgetHandle input_focus_scope() const noexcept {
        return input_.focus_scope;
    }

    WidgetHandle input_focus_scope_fallback() const noexcept {
        return input_.focus_scope_fallback;
    }

    bool input_focus_scope_trap() const noexcept {
        return input_.focus_scope_trap;
    }

    WidgetHandle input_captured() const noexcept {
        return input_.captured;
    }

    bool input_dragging() const noexcept {
        return input_.dragging;
    }

    void set_focus_scope(WidgetHandle scope,
                         WidgetHandle fallback = {},
                         bool trap = true) noexcept {
        input_.focus_scope = scope;
        input_.focus_scope_fallback = fallback;
        input_.focus_scope_trap = trap;
    }

    void clear_focus_scope() noexcept {
        input_.focus_scope = {};
        input_.focus_scope_fallback = {};
        input_.focus_scope_trap = false;
        input_focus_scope_stack_size_ = 0;
    }

    bool push_focus_scope(WidgetHandle scope,
                          WidgetHandle fallback = {},
                          bool trap = true) noexcept {
        if (input_focus_scope_stack_size_ >= kMaxFocusScopeStack) return false;
        input_focus_scope_stack_[input_focus_scope_stack_size_++] = FocusScopeFrame{
            input_.focus_scope,
            input_.focus_scope_fallback,
            input_.focus_scope_trap,
        };
        set_focus_scope(scope, fallback, trap);
        return true;
    }

    bool pop_focus_scope() noexcept {
        if (input_focus_scope_stack_size_ == 0) return false;
        const FocusScopeFrame frame = input_focus_scope_stack_[--input_focus_scope_stack_size_];
        input_.focus_scope = frame.scope;
        input_.focus_scope_fallback = frame.fallback;
        input_.focus_scope_trap = frame.trap;
        return true;
    }

    std::size_t input_focus_scope_stack_size() const noexcept {
        return input_focus_scope_stack_size_;
    }

    Rect world_rect(WidgetHandle h) const noexcept;
    void input_request_cancel() noexcept;

    void input_clear_events() noexcept {
        input_events_.clear();
    }

    std::size_t input_event_count() const noexcept {
        return input_events_.count;
    }

    const SoaInputEvent& input_event(std::size_t idx) const noexcept {
        assert(idx < input_events_.count);
        return input_events_.events[idx];
    }

    bool input_events_overflowed() const noexcept {
        return input_events_.overflowed;
    }

#if defined(VIVID_SOA_TRACE_INPUT)
    std::uint32_t input_guard_state_write_violations() const noexcept {
        return input_guard_state_write_violations_;
    }

    void input_test_request_capture(WidgetHandle h) noexcept {
        input_set_capture(h, input_.last_x, input_.last_y, input_.button, true);
        input_apply_actions();
    }

    void input_test_force_overflow() noexcept {
        input_events_.clear();
        if (!input_.root) return;
        for (std::size_t i = 0; i < (kMaxInputEvents + 4); ++i) {
            input_emit_event(input_.root, Event::mouse(Event::Type::MouseMove, input_.last_x, input_.last_y, 0, input_.last_ms));
        }
        if (input_events_.overflowed) {
            input_handle_overflow(false);
            input_events_.overflowed = true;
        }
    }
#endif

    void set_drag_threshold(int px) noexcept {
        input_.drag_threshold_sq = px * px;
    }

    void input_dispatch(const Event& e) noexcept {
        if (!input_.root) return;
        const auto input_phase_guard = input_phase_scope();
        input_events_.clear();
        input_actions_.clear();
        input_.last_ms = e.ms;
        switch (e.type) {
        case Event::Type::HoverEnter:
            break;
        case Event::Type::HoverLeave:
            break;
        case Event::Type::MouseMove:
            input_.last_x = e.x;
            input_.last_y = e.y;
            input_handle_hover(e.x, e.y, e.button);
            if (input_.pressed || input_.captured) {
                input_handle_drag(e.x, e.y, input_.button);
                const WidgetHandle drag_target = input_drag_target();
                if (drag_target) {
                    const SoaBehavior behavior = behavior_for_kind(kind(drag_target));
                    if (behavior.drag_behavior == SoaDragBehavior::UpdateValueFromPos
                        || behavior.drag_behavior == SoaDragBehavior::ScrollBarTrack) {
                        input_queue_update_slider_value(drag_target, e.x, e.y);
                    }
                }
            } else if (input_.hovered) {
                input_emit_event(input_.hovered, Event::mouse(Event::Type::MouseMove, e.x, e.y, e.button, e.ms));
            }
            break;
        case Event::Type::MouseDown:
            input_.last_x = e.x;
            input_.last_y = e.y;
            input_handle_press(e.x, e.y, e.button);
            break;
        case Event::Type::MouseUp:
            input_.last_x = e.x;
            input_.last_y = e.y;
            input_handle_release(e.x, e.y, e.button);
            break;
        case Event::Type::MouseWheel:
            input_.last_x = e.x;
            input_.last_y = e.y;
            input_handle_wheel(e.x, e.y, e.wheel_y);
            break;
        case Event::Type::Click:
            break;
        case Event::Type::DragStart:
            break;
        case Event::Type::DragMove:
            break;
        case Event::Type::DragEnd:
            break;
        case Event::Type::GestureSwipe:
            break;
        case Event::Type::GesturePinch:
            break;
        case Event::Type::FocusIn:
            break;
        case Event::Type::FocusOut:
            break;
        case Event::Type::KeyDown:
            input_handle_key_down(e.key_code);
            break;
        case Event::Type::KeyUp:
            break;
        case Event::Type::Cancel:
            input_.last_x = e.x;
            input_.last_y = e.y;
            input_handle_cancel(e.x, e.y, e.button);
            break;
        }
        if (input_events_.overflowed) {
            input_handle_overflow();
            input_actions_.clear();
            return;
        }
        if (input_actions_.overflowed) {
            input_handle_action_overflow();
            return;
        }
        input_apply_actions();
    }

    WidgetHandle input_hit_test(int x, int y) noexcept {
        if (!input_.root) return {};
        const auto workspace = acquire_traversal(TraversalPhase::HitTest);
        if (!workspace) return {};
        auto& stack = workspace.stack();
        std::size_t sp = 0;
        stack[sp++] = TraversalFrame{.h = input_.root};
        WidgetHandle result{};

        while (sp > 0) {
            TraversalFrame frame = stack[--sp];
            if (!valid(frame.h)) continue;
            if (!visible(frame.h)) continue;
            if (!enabled(frame.h)) continue;

            const Rect local = rect(frame.h);
            const Rect world{local.x + frame.offset_x, local.y + frame.offset_y, local.w, local.h};
            if (frame.clip_enabled() && !frame.clip_rect.contains(x, y)) {
                continue;
            }
            const bool inside = world.contains(x, y);

            if (inside && hit_testable(frame.h)) {
                result = frame.h;
            }

            if (clip_children(frame.h) && !inside) {
                continue;
            }

            int child_offset_x = frame.offset_x + local.x;
            int child_offset_y = frame.offset_y + local.y;
            if (input_is_scrollable_kind(kind(frame.h))) {
                child_offset_y -= scroll_y(frame.h);
            }
            Rect child_clip = frame.clip_rect;
            bool child_clip_enabled = frame.clip_enabled();
            if (clip_children(frame.h)) {
                child_clip = world;
                child_clip_enabled = true;
            }

            for (auto child = last_child(frame.h); child; child = prev_sibling(child)) {
                if (sp >= stack.size()) {
                    note_workspace_overflow();
                    break;
                }
                stack[sp++] = TraversalFrame{
                    .h = child,
                    .clip_rect = child_clip,
                    .offset_x = child_offset_x,
                    .offset_y = child_offset_y,
                    .flags = child_clip_enabled ? TraversalFrame::kClipEnabled : std::uint8_t{0},
                };
            }
        }
        return result;
    }

    void set_style_patch(WidgetHandle h, const StylePatch& patch) noexcept;
    void set_style_adjust(WidgetHandle h, const StylePatch& patch) noexcept;
    void set_style_override(WidgetHandle h, const StylePatch& patch) noexcept;
    void clear_style_patch(WidgetHandle h) noexcept;
    bool has_style_patch(WidgetHandle h) const noexcept;
    const StylePatch* style_patch(WidgetHandle h) const noexcept;
    StylePatchKind style_patch_kind(WidgetHandle h) const noexcept;
    void set_style_class(WidgetHandle h, StyleClassId id) noexcept;
    void clear_style_class(WidgetHandle h) noexcept;
    StyleClassId style_class(WidgetHandle h) const noexcept;
#if CHARM_VIVID_DRAW_DETAIL_EVIDENCE
    void set_draw_scope(WidgetHandle h, std::uint16_t id) noexcept;
    std::uint16_t draw_scope(WidgetHandle h) const noexcept;
#else
    void set_draw_scope(WidgetHandle, std::uint16_t) noexcept {}
    std::uint16_t draw_scope(WidgetHandle) const noexcept { return 0; }
#endif

    soa_detail::TextSlotId alloc_text_slot() noexcept;
    void free_text_slot(soa_detail::TextSlotId slot) noexcept;
    void set_text(WidgetHandle h, const char* text) noexcept ;
    void set_text_static(WidgetHandle h, const char* text) noexcept;
    void set_text_slot(WidgetHandle h, soa_detail::TextSlotId slot, const char* text) noexcept;
    const char* text(WidgetHandle h) const noexcept ;
    void set_image(WidgetHandle h, soa_detail::ImageId image) noexcept ;
    soa_detail::ImageId image(WidgetHandle h) const noexcept ;
    void set_image_shape(WidgetHandle h, soa_detail::ImageShapeKind kind, std::uint8_t extent) noexcept ;
    soa_detail::ImageShapeKind image_shape_kind(WidgetHandle h) const noexcept ;
    std::uint8_t image_shape_extent(WidgetHandle h) const noexcept ;
    void set_image_rotation_deg(WidgetHandle h, std::int16_t degrees) noexcept ;
    std::int16_t image_rotation_deg(WidgetHandle h) const noexcept ;
    void set_button_icon(WidgetHandle h, soa_detail::ImageId icon) noexcept ;
    void set_button_icon_size(WidgetHandle h, std::uint8_t size) noexcept ;
    soa_detail::ImageId button_icon(WidgetHandle h) const noexcept ;
    std::uint8_t button_icon_size(WidgetHandle h) const noexcept ;
    void set_spinner_phase(WidgetHandle h, std::uint8_t phase) noexcept ;
    std::uint8_t spinner_phase(WidgetHandle h) const noexcept ;
    void set_segmented_count(WidgetHandle h, std::uint8_t count) noexcept ;
    void set_segmented_label(WidgetHandle h, std::uint8_t index, const char* text) noexcept ;
    void set_segmented_selected(WidgetHandle h, std::uint8_t index) noexcept ;
    void set_segmented_underline(WidgetHandle h, bool on) noexcept;
    std::uint8_t segmented_count(WidgetHandle h) const noexcept ;
    std::uint8_t segmented_selected(WidgetHandle h) const noexcept ;
    bool segmented_underline(WidgetHandle h) const noexcept;
    const char* segmented_label(WidgetHandle h, std::uint8_t index) const noexcept ;
    void set_stepper_count(WidgetHandle h, std::uint8_t count) noexcept ;
    void set_stepper_current(WidgetHandle h, std::uint8_t index) noexcept ;
    void set_stepper_label(WidgetHandle h, std::uint8_t index, const char* text) noexcept ;
    std::uint8_t stepper_count(WidgetHandle h) const noexcept ;
    std::uint8_t stepper_current(WidgetHandle h) const noexcept ;
    const char* stepper_label(WidgetHandle h, std::uint8_t index) const noexcept ;
    void set_text_list_count(WidgetHandle h, std::uint16_t count) noexcept ;
    void set_text_list_item(WidgetHandle h, std::uint16_t index, const char* text) noexcept ;
    std::uint16_t text_list_count(WidgetHandle h) const noexcept ;
    int text_list_selected(WidgetHandle h) const noexcept ;
    void set_text_list_selected(WidgetHandle h, int index) noexcept ;
    const char* text_list_item(WidgetHandle h, std::uint16_t index) const noexcept ;
    void set_number_list_count(WidgetHandle h, std::uint16_t count) noexcept ;
    void set_number_list_range(WidgetHandle h, int start, int delta) noexcept ;
    void set_number_list_selected(WidgetHandle h, int index) noexcept ;
    std::uint16_t number_list_count(WidgetHandle h) const noexcept ;
    int number_list_selected(WidgetHandle h) const noexcept ;
    int number_list_value(WidgetHandle h, int index) const noexcept ;
    void set_number_list_row_height(WidgetHandle h, int row_h) noexcept ;
    int number_list_row_height(WidgetHandle h) const noexcept ;
    void set_number_list_wheel_step(WidgetHandle h, int step) noexcept ;
    int number_list_wheel_step(WidgetHandle h) const noexcept ;
    void set_roller_selected(WidgetHandle h, int index) noexcept ;
    std::uint16_t roller_count(WidgetHandle h) const noexcept ;
    int roller_selected(WidgetHandle h) const noexcept ;
    const char* roller_item_text(WidgetHandle h, std::uint16_t index) const noexcept ;
    void set_roller_row_height(WidgetHandle h, int row_h) noexcept ;
    int roller_row_height(WidgetHandle h) const noexcept ;
    void set_roller_wheel_step(WidgetHandle h, int step) noexcept ;
    int roller_wheel_step(WidgetHandle h) const noexcept ;
    void set_roller_source(WidgetHandle h, std::uint16_t count,
        const void* ctx, soa_detail::RollerTextFn fn) noexcept ;
    void console_clear(WidgetHandle h) noexcept ;
    void set_console_follow_tail(WidgetHandle h, bool follow) noexcept ;
    void console_append(WidgetHandle h, const char* text) noexcept ;
    void set_list_view_count(WidgetHandle h, std::uint16_t count) noexcept ;
    std::uint16_t list_view_count(WidgetHandle h) const noexcept ;
    int list_view_selected(WidgetHandle h) const noexcept ;
    void set_list_view_selected(WidgetHandle h, int index) noexcept ;
    int list_view_active(WidgetHandle h) const noexcept ;
    void set_list_view_active(WidgetHandle h, int index) noexcept ;
    const char* list_view_item_text(WidgetHandle h, std::uint16_t index) const noexcept ;
    const char* list_view_item_subtitle(WidgetHandle h, std::uint16_t index) const noexcept ;
    const char* list_view_item_tail(WidgetHandle h, std::uint16_t index) const noexcept ;
    std::uint8_t list_view_item_row_flags(WidgetHandle h, std::uint16_t index) const noexcept ;
    soa_detail::ImageId list_view_item_tail_icon(WidgetHandle h, std::uint16_t index) const noexcept ;
    soa_detail::ImageId list_view_item_tail_action_icon(WidgetHandle h, std::uint16_t index) const noexcept ;
    soa_detail::ImageId list_view_item_icon(WidgetHandle h, std::uint16_t index) const noexcept ;
    std::uint8_t list_view_tail_icon_size(WidgetHandle h) const noexcept ;
    std::uint8_t list_view_tail_action_icon_size(WidgetHandle h) const noexcept ;
    std::uint8_t list_view_icon_corner_radius(WidgetHandle h) const noexcept ;
    std::uint8_t list_view_icon_size(WidgetHandle h) const noexcept ;
    std::uint8_t list_view_overscan(WidgetHandle h) const noexcept ;
    void set_list_view_source(WidgetHandle h, std::uint16_t count, const void* ctx,
        soa_detail::ListViewTextFn text_fn) noexcept ;
    void set_list_view_subtitle_source(WidgetHandle h, const void* ctx,
        soa_detail::ListViewSubtitleFn subtitle_fn) noexcept ;
    void set_list_view_tail_source(WidgetHandle h, const void* ctx,
        soa_detail::ListViewTailFn tail_fn) noexcept ;
    void set_list_view_row_flags_source(WidgetHandle h, const void* ctx,
        soa_detail::ListViewRowFlagsFn row_flags_fn) noexcept ;
    void set_list_view_tail_icon_source(WidgetHandle h, const void* ctx,
        soa_detail::ListViewIconFn icon_fn, std::uint8_t size) noexcept ;
    void set_list_view_tail_action_icon_source(WidgetHandle h, const void* ctx,
        soa_detail::ListViewIconFn icon_fn, std::uint8_t size) noexcept ;
    void set_list_view_icon_corner_radius(WidgetHandle h, std::uint8_t radius) noexcept ;
    void set_list_view_icon_source(WidgetHandle h, const void* ctx,
        soa_detail::ListViewIconFn icon_fn, std::uint8_t size) noexcept ;
    int consume_list_view_tail_action(WidgetHandle h) noexcept ;
    void set_table_view_header_height(WidgetHandle h, int height) noexcept ;
    void set_table_view_header_padding(WidgetHandle h, int padding) noexcept ;
    void set_table_view_header_style(WidgetHandle h, TableViewHeaderStyle style) noexcept ;
    void set_table_view_header_divider(WidgetHandle h, bool enabled) noexcept ;
    void set_table_view_col_dividers(WidgetHandle h, bool enabled) noexcept ;
    void set_table_view_col_divider_style(WidgetHandle h, TableViewColDividerStyle style) noexcept ;
    void set_table_view_count(WidgetHandle h, std::uint16_t rows) noexcept ;
    std::uint16_t table_view_row_count(WidgetHandle h) const noexcept ;
    bool table_view_has_header(WidgetHandle h) const noexcept ;
    int table_view_header_height(WidgetHandle h) const noexcept ;
    int table_view_header_padding(WidgetHandle h) const noexcept ;
    TableViewHeaderStyle table_view_header_style(WidgetHandle h) const noexcept ;
    bool table_view_header_divider(WidgetHandle h) const noexcept ;
    bool table_view_col_dividers(WidgetHandle h) const noexcept ;
    TableViewColDividerStyle table_view_col_divider_style(WidgetHandle h) const noexcept ;
    std::uint8_t table_view_col_count(WidgetHandle h) const noexcept ;
    const char* table_view_header_text(WidgetHandle h, std::uint8_t col) const noexcept ;
    bool table_view_has_col_width_fn(WidgetHandle h) const noexcept ;
    int table_view_col_width(WidgetHandle h) const noexcept ;
    int table_view_col_width_at(WidgetHandle h, std::uint8_t col) const noexcept ;
    int table_view_scroll_x(WidgetHandle h) const noexcept ;
    void set_table_view_col_width(WidgetHandle h, int col_width) noexcept ;
    void set_table_view_scroll_x(WidgetHandle h, int x) noexcept ;
    void set_table_view_source(WidgetHandle h, std::uint16_t rows, std::uint8_t cols,
        const void* ctx, soa_detail::TableViewTextFn text_fn) noexcept ;
    void set_table_view_header(WidgetHandle h, const void* ctx,
        soa_detail::TableViewHeaderFn header_fn) noexcept ;
    void set_table_view_col_width_fn(WidgetHandle h, const void* ctx,
        soa_detail::TableViewColWidthFn width_fn) noexcept ;
    std::uint8_t table_view_overscan(WidgetHandle h) const noexcept ;
    const char* table_view_cell_text(WidgetHandle h, std::uint16_t row, std::uint8_t col) const noexcept ;
    void set_tree_view_count(WidgetHandle h, std::uint16_t count) noexcept ;
    std::uint16_t tree_view_count(WidgetHandle h) const noexcept ;
    std::uint8_t tree_view_overscan(WidgetHandle h) const noexcept ;
    std::uint8_t tree_view_indent_px(WidgetHandle h) const noexcept ;
    int tree_view_max_indent_px(WidgetHandle h) const noexcept ;
    int tree_view_min_text_avail_px(WidgetHandle h) const noexcept ;
    void set_tree_view_indent_px(WidgetHandle h, std::uint8_t px) noexcept ;
    void set_tree_view_max_indent_px(WidgetHandle h, int px) noexcept ;
    void set_tree_view_min_text_avail_px(WidgetHandle h, int px) noexcept ;
    const char* tree_view_item_text(WidgetHandle h, std::uint16_t index) const noexcept ;
    std::uint8_t tree_view_item_indent(WidgetHandle h, std::uint16_t index) const noexcept ;
    void set_tree_view_source(WidgetHandle h, std::uint16_t count,
        const void* text_ctx, soa_detail::TreeViewTextFn text_fn,
        const void* indent_ctx, soa_detail::TreeViewIndentFn indent_fn) noexcept ;
    void set_toggle_group_kind(WidgetHandle h, WidgetKind group_kind) noexcept ;
    WidgetKind toggle_group_kind(WidgetHandle h) const noexcept ;
    void set_value(WidgetHandle h, int value) noexcept ;
    int value(WidgetHandle h) const noexcept ;
    void set_range(WidgetHandle h, int min_value, int max_value) noexcept ;
    int min_value(WidgetHandle h) const noexcept ;
    int max_value(WidgetHandle h) const noexcept ;
    void set_scrollbar_orientation(WidgetHandle h, ScrollBarOrientation orient) noexcept ;
    ScrollBarOrientation scrollbar_orientation(WidgetHandle h) const noexcept ;
    void set_scrollbar_page_size(WidgetHandle h, int page_size) noexcept ;
    int scrollbar_page_size(WidgetHandle h) const noexcept ;
    void set_scrollbar_target(WidgetHandle h, WidgetHandle target) noexcept ;
    WidgetHandle scrollbar_target(WidgetHandle h) const noexcept ;
    void set_checked(WidgetHandle h, bool on) noexcept ;
    bool checked(WidgetHandle h) const noexcept ;
    void set_scroll_y(WidgetHandle h, int y) noexcept ;
    void add_scroll_y(WidgetHandle h, int dy) noexcept ;
    int scroll_y(WidgetHandle h) const noexcept ;
    void set_scroll_step(WidgetHandle h, int step) noexcept ;
    int scroll_step(WidgetHandle h) const noexcept ;
    void set_list_row_height(WidgetHandle h, int row_h) noexcept ;
    int list_row_height(WidgetHandle h) const noexcept ;
    void apply_list_layout(WidgetHandle h, int padding) noexcept ;
    void set_layout_kind(WidgetHandle h, SoaLayoutKind kind) noexcept ;
    SoaLayoutKind layout_kind(WidgetHandle h) const noexcept ;
    void set_layout_state_influence(bool on) noexcept ;
    bool layout_state_influence() const noexcept ;
    std::uint8_t layout_state_influence_mask(WidgetKind kind) const noexcept ;
    std::uint32_t layout_dirty_version() const noexcept ;
    std::uint32_t paint_dirty_version() const noexcept ;
    bool payload_overflowed() const noexcept ;
    bool text_overflowed() const noexcept ;
    bool semantic_overflowed() const noexcept { return semantics_.overflowed(); }
    std::size_t semantic_live_count() const noexcept { return semantics_.live_count(); }
    std::size_t semantic_peak_count() const noexcept { return semantics_.peak_count(); }
    std::uint32_t semantic_alloc_fail() const noexcept { return semantics_.alloc_fail(); }
    bool style_patch_overflowed() const noexcept { return style_patches_.overflowed(); }
    std::size_t style_patch_live_count() const noexcept { return style_patches_.live_count(); }
    std::size_t style_patch_peak_count() const noexcept { return style_patches_.peak_count(); }
    std::uint32_t style_patch_alloc_fail() const noexcept { return style_patches_.alloc_fail(); }
    bool workspace_overflowed() const noexcept { return workspace_overflowed_; }
    bool traversal_phase_conflicted() const noexcept { return traversal_phase_conflicted_; }
    void clear_workspace_overflow() noexcept {
        workspace_overflowed_ = false;
        traversal_phase_conflicted_ = false;
    }
    void note_workspace_overflow() const noexcept { workspace_overflowed_ = true; }
    [[nodiscard]] TraversalLease acquire_traversal(TraversalPhase phase) const noexcept {
        assert(phase != TraversalPhase::Idle);
        if (phase == TraversalPhase::Idle || traversal_phase_ != TraversalPhase::Idle) {
            traversal_phase_conflicted_ = true;
            workspace_overflowed_ = true;
            return {};
        }
        traversal_phase_ = phase;
        return TraversalLease{this, phase};
    }
    soa_detail::PayloadStats payload_stats() const noexcept ;
    std::uint32_t layout_applied_version() const noexcept ;
    void set_layout_applied_version(std::uint32_t v) noexcept ;
    void layout_trace_reset() noexcept ;
    std::uint32_t layout_invalidated_count() const noexcept ;
    std::uint32_t layout_pass_count() const noexcept ;
    std::uint32_t paint_invalidated_count() const noexcept ;
    void layout_trace_on_pass() noexcept ;
    int compute_content_height(WidgetHandle h) const noexcept ;
    int max_scroll(WidgetHandle h) const noexcept ;
    int clamp_scroll_y(WidgetHandle h, int y) const noexcept ;
    int table_view_content_width(WidgetHandle h) const noexcept ;
    int max_scroll_x(WidgetHandle h) const noexcept ;
    int clamp_scroll_x(WidgetHandle h, int x) const noexcept ;
    void set_table_view_scroll_x_clamped(WidgetHandle h, int x) noexcept ;
    void set_scroll_y_clamped(WidgetHandle h, int y) noexcept ;
    soa_detail::CommonSoA<kMaxNodes> common_{};
    soa_detail::SemanticPool<kSemanticCapacity> semantics_{};
    soa_detail::StylePatchPool<kStylePatchCapacity> style_patches_{};
    soa_detail::PayloadManager payloads_{};
    std::uint16_t free_head_{kInvalidIndex};
    std::uint32_t layout_dirty_version_{0};
    std::uint32_t paint_dirty_version_{0};
    std::uint32_t layout_applied_version_{0};
    bool layout_state_influence_{true};
#if defined(VIVID_SOA_TRACE_INPUT)
    std::uint32_t layout_invalidated_count_{0};
    std::uint32_t layout_pass_count_{0};
    std::uint32_t paint_invalidated_count_{0};
#endif

    static void unsupported_kind(WidgetKind kind) noexcept {
#if defined(CHARM_VIVID_UNSUPPORTED_WIDGET_DIAG)
        charm_vivid_soa_unsupported_widget_kind(
            static_cast<unsigned>(kind),
            static_cast<unsigned>(reinterpret_cast<std::uintptr_t>(__builtin_return_address(0))));
#endif
#ifndef NDEBUG
        assert(false && "SoaKernel unsupported WidgetKind");
#else
        (void)kind;
#endif
    }

    soa_detail::PayloadSlot payload_alloc(WidgetKind kind, std::uint16_t owner_idx) noexcept {
        const auto desc = payload_descriptor(kind);
        if (!desc.supported) {
            return soa_detail::invalid_payload_slot();
        }
        return payloads_.alloc(desc.payload, kind, owner_idx);
    }

    void payload_free(WidgetKind kind, soa_detail::PayloadSlot slot, std::uint16_t owner_idx) noexcept {
        if (!soa_detail::payload_slot_valid(slot)) return;
        const auto desc = payload_descriptor(kind);
        if (!desc.supported) return;
        payloads_.free(desc.payload, kind, slot, owner_idx);
    }

    template <typename T>
    T* payload_get(std::uint16_t idx) noexcept {
        const auto slot = common_.storage_slot[idx].payload_slot();
        if (!soa_detail::payload_slot_valid(slot)) return nullptr;
        return payloads_.get<T>(slot, idx, common_.kind[idx]);
    }

    template <typename T>
    const T* payload_get(std::uint16_t idx) const noexcept {
        const auto slot = common_.storage_slot[idx].payload_slot();
        if (!soa_detail::payload_slot_valid(slot)) return nullptr;
        return payloads_.get<T>(slot, idx, common_.kind[idx]);
    }

private:
    mutable std::array<TraversalFrame, kMaxNodes> traversal_stack_{};
    mutable TraversalPhase traversal_phase_{TraversalPhase::Idle};
    mutable bool traversal_phase_conflicted_{false};
    mutable bool workspace_overflowed_{false};

    void release_traversal(TraversalPhase phase) const noexcept {
        assert(traversal_phase_ == phase);
        traversal_phase_ = TraversalPhase::Idle;
    }

    void mark_layout_dirty() noexcept;
    void mark_paint_dirty() noexcept;

    static bool text_equal(const char* lhs, const char* rhs) noexcept {
        if (!lhs || !rhs) return lhs == rhs;
        while (*lhs && *rhs) {
            if (*lhs != *rhs) return false;
            ++lhs;
            ++rhs;
        }
        return *lhs == *rhs;
    }

    void on_state_change(std::uint16_t idx, SoaStateMask bit) noexcept;
    static constexpr std::uint8_t layout_state_mask_for_kind(WidgetKind kind) noexcept;

    static StyleState input_make_state(const SoaKernel& kernel, WidgetHandle h) noexcept;
    static bool input_is_scrollable_kind(WidgetKind kind) noexcept;
    static bool input_is_checkable_kind(WidgetKind kind) noexcept;

    static constexpr std::size_t kMaxInputActions = 32;

    struct InputActionQueue {
        std::array<SoaInputAction, kMaxInputActions> actions{};
        std::size_t count{0};
        bool overflowed{false};

        void clear() noexcept {
            count = 0;
            overflowed = false;
        }
    };

    InputActionQueue input_actions_{};
    bool input_phase_{false};
    bool input_commit_phase_{false};

    struct InputPhaseScope {
        SoaKernel& kernel;
        explicit InputPhaseScope(SoaKernel& k) : kernel(k) {
            kernel.input_phase_ = true;
        }
        ~InputPhaseScope() {
            kernel.input_phase_ = false;
        }
    };

    struct InputCommitScope {
        SoaKernel& kernel;
        explicit InputCommitScope(SoaKernel& k) : kernel(k) {
            kernel.input_commit_phase_ = true;
        }
        ~InputCommitScope() {
            kernel.input_commit_phase_ = false;
        }
    };

    [[nodiscard]] InputPhaseScope input_phase_scope() noexcept {
        return InputPhaseScope{*this};
    }

    [[nodiscard]] InputCommitScope input_commit_scope() noexcept {
        return InputCommitScope{*this};
    }

    void input_guard_state_write(const char* what) noexcept {
        if (input_phase_ && !input_commit_phase_) {
#if defined(VIVID_SOA_TRACE_INPUT)
            ++input_guard_state_write_violations_;
#endif
#ifndef NDEBUG
            assert(false && "SoaKernel state write during input phase");
#endif
        }
        (void)what;
    }

    void input_emit_action(const SoaInputAction& action) noexcept;
    void input_handle_action_overflow() noexcept;
    void input_apply_action(const SoaInputAction& action) noexcept;
    void input_apply_actions() noexcept;

    static constexpr std::size_t kMaxInputEvents = 32;

    struct InputEventQueue {
        std::array<SoaInputEvent, kMaxInputEvents> events{};
        std::size_t count{0};
        bool overflowed{false};

        void clear() noexcept {
            count = 0;
            overflowed = false;
        }
    };

    struct InputState {
        WidgetHandle root{};
        WidgetHandle hovered{};
        WidgetHandle pressed{};
        WidgetHandle focused{};
        WidgetHandle captured{};
        WidgetHandle scroll_target{};
        WidgetHandle focus_scope{};
        WidgetHandle focus_scope_fallback{};
        int drag_start_x{0};
        int drag_start_y{0};
        int drag_last_x{0};
        int drag_last_y{0};
        int last_x{0};
        int last_y{0};
        std::uint32_t last_ms{0};
        int button{0};
        int drag_threshold_sq{25};
        bool dragging{false};
        bool focus_scope_trap{false};
    };

    InputEventQueue input_events_{};
    InputState input_{};
    static constexpr std::size_t kMaxFocusScopeStack = 4;
#if defined(VIVID_SOA_TRACE_INPUT)
    std::uint32_t input_guard_state_write_violations_{0};
#endif

    struct FocusScopeFrame {
        WidgetHandle scope{};
        WidgetHandle fallback{};
        bool trap{false};
    };

    std::array<FocusScopeFrame, kMaxFocusScopeStack> input_focus_scope_stack_{};
    std::size_t input_focus_scope_stack_size_{0};

    static int clamp_int(int v, int lo, int hi) noexcept ;
    static int div_floor(int num, int den) noexcept ;
    void input_emit_event(WidgetHandle target, const Event& e) noexcept ;
    bool input_is_invalid(WidgetHandle node) const noexcept ;
    bool input_is_descendant(WidgetHandle node, WidgetHandle ancestor) const noexcept ;
    void clear_scrollbar_targets(WidgetHandle h) noexcept ;
    void input_set_capture(WidgetHandle h, int x, int y, int button, bool emit_cancel) ;
    void input_set_dragging(bool on) ;
    void input_on_destroy(WidgetHandle h) ;
    void input_handle_overflow(bool assert_on_overflow = true) ;
    void input_handle_hover(int x, int y, int button) ;
    void input_handle_drag(int x, int y, int button) ;
    void input_handle_press(int x, int y, int button) ;
    void input_handle_release(int x, int y, int button) ;
    void input_handle_wheel(int x, int y, int dy) ;
    void input_handle_cancel(int x, int y, int button) ;
    void input_handle_key_down(Event::Key key) ;
    void input_handle_click(WidgetHandle h, int x, int y) ;
    bool scrollbar_track_info(WidgetHandle h, const ResolvedMetrics* metrics, ScrollBarTrackInfo& info) ;
    bool input_scrollbar_page_click(WidgetHandle h, int x, int y, const ResolvedMetrics* metrics) ;
    void input_clear_sibling_checks(WidgetHandle h, WidgetKind kind) ;
    int input_segmented_index_from_pos(WidgetHandle h, int x) const noexcept ;
    int input_text_list_index_from_pos(WidgetHandle h, int y) const noexcept ;
    int input_list_view_index_from_pos(WidgetHandle h, int y) const noexcept ;
    int input_stepper_index_from_pos(WidgetHandle h, int x) const noexcept ;
    int input_number_list_index_from_pos(WidgetHandle h, int y) const noexcept ;
    int input_roller_index_from_pos(WidgetHandle h, int y) const noexcept ;
    void input_queue_update_slider_value(WidgetHandle h, int x, int y) ;
    void input_apply_update_slider_value(WidgetHandle h, int x, int y) ;
    Rect input_world_rect(WidgetHandle h) const noexcept ;
    WidgetHandle input_find_scroll_target(WidgetHandle hit) noexcept ;
    WidgetHandle input_find_scroll_ancestor(WidgetHandle h) const noexcept ;
    WidgetHandle input_find_toggle_group_ancestor(WidgetHandle h) const noexcept ;
    void input_scroll_by(WidgetHandle h, int dy, int dx = 0) ;
    SoaWheelAxisPolicy input_wheel_axis_override(WidgetHandle hit, WidgetHandle target,
        SoaWheelAxisPolicy fallback, int x, int y) const noexcept ;
    void input_apply_scroll_by(WidgetHandle h, int dy, int dx) ;
    bool input_is_focus_candidate(WidgetHandle h) const noexcept ;
    WidgetHandle input_first_focus_candidate(WidgetHandle root) const noexcept ;
    WidgetHandle input_next_focus_candidate(WidgetHandle root, WidgetHandle current, bool reverse) const noexcept ;
    WidgetHandle input_spatial_focus_candidate(WidgetHandle root, WidgetHandle current, Event::Key key) const noexcept ;
    WidgetHandle input_resolve_focus_request(WidgetHandle h) const noexcept ;
    void input_set_focus(WidgetHandle h) ;
    WidgetHandle input_drag_target() const noexcept ;
    std::uint16_t index_of(WidgetHandle h) const noexcept ;
    WidgetHandle handle_from_index(std::uint16_t idx) const noexcept ;
    std::uint16_t last_child_index(std::uint16_t parent) const noexcept ;
    std::uint16_t prev_sibling_index(std::uint16_t idx) const noexcept ;
    void detach_from_parent(std::uint16_t idx) noexcept ;
    void detach_children(std::uint16_t idx) noexcept ;
    bool creates_cycle(std::uint16_t parent, std::uint16_t child) const noexcept ;
    bool flag_raw(std::uint16_t idx, SoaNodeFlag flag) const noexcept ;
    bool get_flag(WidgetHandle h, SoaNodeFlag flag) const noexcept ;
    void set_flag(WidgetHandle h, SoaNodeFlag flag, bool on) noexcept ;
    bool get_state_flag(WidgetHandle h, SoaStateFlag flag) const noexcept ;
    void set_state_flag(WidgetHandle h, SoaStateFlag flag, bool on) noexcept ;

};

static_assert(!std::is_copy_constructible_v<SoaKernel>
              && !std::is_copy_assignable_v<SoaKernel>
              && !std::is_move_constructible_v<SoaKernel>
              && !std::is_move_assignable_v<SoaKernel>,
              "SoaKernel must retain one stable handle and workspace identity");
