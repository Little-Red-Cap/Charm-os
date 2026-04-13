module;
#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <type_traits>

export module charm.core.soa_payload;

export import charm.core.handle;
export import charm.gfx.draw_cmd;
import charm.core.soa_pool_caps;

export namespace soa_detail {
    constexpr std::uint16_t kInvalidPayloadSlot = 0xFFFF;

    struct PayloadHandle {
        std::uint16_t slot{kInvalidPayloadSlot};
        std::uint16_t generation{0};
    };

    struct TextId {
        std::uint16_t offset{0};
        std::uint16_t length{0};
    };

    using TextSlotId = std::uint16_t;

    using ImageId = ui::draw_cmd::ImageId;

    constexpr ImageId invalid_image_id() noexcept {
        return ui::draw_cmd::invalid_image_id();
    }

    constexpr bool image_id_valid(ImageId id) noexcept {
        return ui::draw_cmd::image_id_valid(id);
    }

    constexpr PayloadHandle invalid_payload_handle() noexcept {
        return PayloadHandle{kInvalidPayloadSlot, 0};
    }

    constexpr bool payload_valid(PayloadHandle h) noexcept {
        return h.slot != kInvalidPayloadSlot;
    }

    constexpr TextId empty_text_id() noexcept {
        return TextId{0, 0};
    }

    constexpr std::uint16_t kTextKindMask = 0xC000;
    constexpr std::uint16_t kTextLenMask = 0x3FFF;

    enum class TextKind : std::uint16_t {
        Arena = 0,
        Static = 1,
        Slot = 2,
        Invalid = 3,
    };

    constexpr TextKind text_kind(TextId id) noexcept {
        if (id.length == 0) return TextKind::Invalid;
        return static_cast<TextKind>((id.length & kTextKindMask) >> 14);
    }

    constexpr std::uint16_t text_length(TextId id) noexcept {
        return static_cast<std::uint16_t>(id.length & kTextLenMask);
    }

    constexpr TextId make_text_id(TextKind kind, std::uint16_t offset, std::uint16_t length) noexcept {
        if (length == 0) return empty_text_id();
        const auto len = static_cast<std::uint16_t>(length & kTextLenMask);
        const auto kind_bits = static_cast<std::uint16_t>(static_cast<std::uint16_t>(kind) << 14);
        return TextId{offset, static_cast<std::uint16_t>(kind_bits | len)};
    }

    constexpr bool text_valid(TextId id) noexcept {
        return text_length(id) != 0;
    }

    constexpr std::size_t kTextArenaBytes =
        static_cast<std::size_t>(soa_text_arena_bytes());

    struct TextArena {
        std::array<char, kTextArenaBytes> data{};
        std::uint16_t used{1};
        bool overflowed{false};
        TextId overflow_id{};

        void reset() noexcept {
            used = 1;
            data[0] = '\0';
            overflowed = false;
            overflow_id = store_literal("<...>");
        }

        TextId alloc(const char* text) noexcept {
            if (!text) return empty_text_id();
            const std::size_t len = std::strlen(text);
            if (len == 0) return empty_text_id();
            if (len > kTextLenMask) {
                overflowed = true;
                return text_valid(overflow_id) ? overflow_id : empty_text_id();
            }
            if (len + 1 > (kTextArenaBytes - used)) {
                overflowed = true;
                return text_valid(overflow_id) ? overflow_id : empty_text_id();
            }
            const std::uint16_t offset = used;
            std::memcpy(data.data() + used, text, len);
            data[used + len] = '\0';
            used = static_cast<std::uint16_t>(used + len + 1);
            return make_text_id(TextKind::Arena, offset, static_cast<std::uint16_t>(len));
        }

        std::span<const char> span(TextId id) const noexcept {
            if (!text_valid(id) || text_kind(id) != TextKind::Arena) return {};
            const std::uint32_t len = text_length(id);
            const std::uint32_t end = static_cast<std::uint32_t>(id.offset) + len;
            if (end > used) return {};
            return std::span<const char>(data.data() + id.offset, len);
        }

        const char* c_str(TextId id) const noexcept {
            if (!text_valid(id) || text_kind(id) != TextKind::Arena) return "";
            const std::uint32_t len = text_length(id);
            const std::uint32_t end = static_cast<std::uint32_t>(id.offset) + len;
            if (end > used) return "";
            return data.data() + id.offset;
        }

    private:
        TextId store_literal(const char* text) noexcept {
            if (!text) return empty_text_id();
            const std::size_t len = std::strlen(text);
            if (len == 0) return empty_text_id();
            if (len > kTextLenMask) return empty_text_id();
            if (len + 1 > (kTextArenaBytes - used)) return empty_text_id();
            const std::uint16_t offset = used;
            std::memcpy(data.data() + used, text, len);
            data[used + len] = '\0';
            used = static_cast<std::uint16_t>(used + len + 1);
            return make_text_id(TextKind::Arena, offset, static_cast<std::uint16_t>(len));
        }
    };

    constexpr TextSlotId kInvalidTextSlot = 0xFFFF;
    constexpr std::size_t kTextSlotCount = 64;
    constexpr std::size_t kTextSlotBytes = 128;
    constexpr std::size_t kTextStaticCount = 128;

    struct TextSlot {
        std::array<char, kTextSlotBytes> data{};
        std::uint16_t len{0};
        bool used{false};
    };

    struct PayloadPoolStats {
        std::uint16_t cap{0};
        std::uint16_t used{0};
        std::uint16_t peak{0};
        std::uint32_t alloc_fail{0};
    };

    struct PayloadStats {
        PayloadPoolStats label{};
        PayloadPoolStats button{};
        PayloadPoolStats image{};
        PayloadPoolStats text_input{};
        PayloadPoolStats text_area{};
        PayloadPoolStats number_input{};
        PayloadPoolStats segmented{};
        PayloadPoolStats toggle_group{};
        PayloadPoolStats checkbox{};
        PayloadPoolStats radio{};
        PayloadPoolStats list_item{};
        PayloadPoolStats text_list{};
        PayloadPoolStats list_view{};
        PayloadPoolStats table_view{};
        PayloadPoolStats tree_view{};
        PayloadPoolStats stepper{};
        PayloadPoolStats number_list{};
        PayloadPoolStats roller{};
        PayloadPoolStats switcher{};
        PayloadPoolStats slider{};
        PayloadPoolStats progress{};
        PayloadPoolStats scrollbar{};
        PayloadPoolStats list{};
        PayloadPoolStats scroll_container{};
        PayloadPoolStats spinner{};
        bool overflowed{false};
        bool text_overflowed{false};
    };

    struct LabelPayload {
        TextId text{};
    };

    struct ButtonPayload {
        TextId text{};
        ImageId icon{invalid_image_id()};
        std::uint8_t icon_size{0};
        std::uint8_t icon_reserved{0};
    };

    struct ImagePayload {
        ImageId image{invalid_image_id()};
    };

    struct TextInputPayload {
        TextId text{};
    };

    struct TextAreaPayload {
        TextId text{};
    };

    struct NumberInputPayload {
        TextId text{};
    };

    constexpr std::uint8_t kMaxSegments = 8;
    constexpr std::uint8_t kMaxStepperSteps = 8;

    struct SegmentedControlPayload {
        std::array<TextId, kMaxSegments> labels{};
        std::uint8_t count{0};
        std::uint8_t selected{0};
    };

    struct StepperPayload {
        std::array<TextId, kMaxStepperSteps> labels{};
        std::uint8_t count{0};
        std::uint8_t current{0};
    };

    struct ToggleGroupPayload {
        WidgetKind group_kind{WidgetKind::None};
    };

    struct CheckboxPayload {
        TextId text{};
        std::uint8_t checked{0};
    };

    struct RadioPayload {
        TextId text{};
        std::uint8_t checked{0};
    };

    struct ListItemPayload {
        TextId text{};
        std::uint8_t checked{0};
    };

    constexpr std::uint16_t kMaxTextListItems = 32;

    struct TextListPayload {
        std::array<TextId, kMaxTextListItems> items{};
        std::uint16_t count{0};
        std::uint16_t start{0};
        std::int16_t selected{-1};
        std::uint8_t follow_tail{0};
        std::uint8_t reserved{0};
        int scroll_y{0};
        int row_height{28};
        int wheel_step{1};
    };

    using ListViewTextFn = const char* (*)(const void*, std::uint16_t) noexcept;
    using ListViewSubtitleFn = const char* (*)(const void*, std::uint16_t) noexcept;
    using ListViewTailFn = const char* (*)(const void*, std::uint16_t) noexcept;
    using ListViewIconFn = ImageId (*)(const void*, std::uint16_t) noexcept;
    using TableViewTextFn = const char* (*)(const void*, std::uint16_t, std::uint8_t) noexcept;
    using TableViewHeaderFn = const char* (*)(const void*, std::uint8_t) noexcept;
    using TableViewColWidthFn = int (*)(const void*, std::uint8_t) noexcept;
    using TreeViewTextFn = const char* (*)(const void*, std::uint16_t) noexcept;
    using TreeViewIndentFn = std::uint8_t (*)(const void*, std::uint16_t) noexcept;
    using RollerTextFn = const char* (*)(const void*, std::uint16_t) noexcept;

    struct ListViewPayload {
        const void* text_ctx{nullptr};
        ListViewTextFn text_fn{nullptr};
        const void* subtitle_ctx{nullptr};
        ListViewSubtitleFn subtitle_fn{nullptr};
        const void* tail_ctx{nullptr};
        ListViewTailFn tail_fn{nullptr};
        const void* tail_icon_ctx{nullptr};
        ListViewIconFn tail_icon_fn{nullptr};
        const void* icon_ctx{nullptr};
        ListViewIconFn icon_fn{nullptr};
        std::uint16_t count{0};
        std::int16_t selected{-1};
        std::int16_t active{-1};
        int scroll_y{0};
        int row_height{28};
        int wheel_step{24};
        std::uint8_t overscan{2};
        std::uint8_t tail_icon_size{0};
        std::uint8_t icon_corner_radius{0};
        std::uint8_t icon_size{0};
    };

    struct TableViewPayload {
        const void* text_ctx{nullptr};
        TableViewTextFn text_fn{nullptr};
        const void* header_ctx{nullptr};
        TableViewHeaderFn header_fn{nullptr};
        const void* col_width_ctx{nullptr};
        TableViewColWidthFn col_width_fn{nullptr};
        std::uint16_t row_count{0};
        std::uint8_t col_count{0};
        std::uint8_t overscan{2};
        int col_width{0};
        int header_height{0};
        int header_padding{0};
        std::uint8_t header_style{0};
        std::uint8_t header_divider{1};
        std::uint8_t col_dividers{3};
        int scroll_x{0};
        int scroll_y{0};
        int row_height{28};
        int wheel_step{24};
    };

    struct TreeViewPayload {
        const void* text_ctx{nullptr};
        TreeViewTextFn text_fn{nullptr};
        const void* indent_ctx{nullptr};
        TreeViewIndentFn indent_fn{nullptr};
        std::uint16_t count{0};
        std::uint8_t overscan{2};
        std::uint8_t indent_px{12};
        std::uint16_t max_indent_px{96};
        std::uint16_t min_text_avail_px{24};
        int scroll_y{0};
        int row_height{28};
        int wheel_step{24};
    };

    struct NumberListPayload {
        std::uint16_t count{0};
        std::int16_t selected{0};
        int start{0};
        int delta{1};
        int scroll_y{0};
        int row_height{28};
        int wheel_step{24};
    };

    struct RollerPayload {
        const void* text_ctx{nullptr};
        RollerTextFn text_fn{nullptr};
        std::uint16_t count{0};
        std::int16_t selected{0};
        int scroll_y{0};
        int row_height{24};
        int wheel_step{24};
    };

    struct SwitchPayload {
        std::uint8_t checked{0};
    };

    struct SliderPayload {
        int value{0};
        int min_value{0};
        int max_value{100};
    };

    struct ProgressPayload {
        int value{0};
        int min_value{0};
        int max_value{100};
    };

    struct SpinnerPayload {
        std::uint8_t phase{0};
        std::uint8_t reserved{0};
    };

    struct ScrollBarPayload {
        int value{0};
        int min_value{0};
        int max_value{100};
        std::uint8_t orientation{0};
        int page_size{0};
        WidgetHandle target{};
    };

    struct ListPayload {
        int scroll_y{0};
        int scroll_step{24};
        int row_height{28};
    };

    struct ScrollContainerPayload {
        int scroll_y{0};
        int scroll_step{24};
    };

    enum class PayloadKind : std::uint8_t {
        None,
#define VIVID_PAYLOAD_KIND(name, member, stats_field, cap_kind) name,
#include "widgets.payload.kinds.def"
#undef VIVID_PAYLOAD_KIND
    };

    template <typename T, std::size_t N>
    struct PayloadPool {
        static_assert(N <= 0xFFFF, "PayloadPool capacity exceeds slot range");
        std::array<T, N> items{};
        std::array<std::uint16_t, N> generation{};
        std::array<std::uint16_t, N> free_next{};
        std::uint16_t free_head{kInvalidPayloadSlot};
#if defined(VIVID_SOA_TRACE_INPUT)
        std::uint16_t used{0};
        std::uint16_t peak{0};
        std::uint32_t alloc_fail{0};
#endif
#ifndef NDEBUG
        std::array<std::uint16_t, N> owner{};
        std::array<WidgetKind, N> owner_kind{};
#endif

        void reset() noexcept {
            free_head = 0;
#if defined(VIVID_SOA_TRACE_INPUT)
            used = 0;
            peak = 0;
            alloc_fail = 0;
#endif
            for (std::uint16_t i = 0; i < N; ++i) {
                generation[i] = 1;
                free_next[i] = (i + 1 < N) ? static_cast<std::uint16_t>(i + 1) : kInvalidPayloadSlot;
                items[i] = T{};
#ifndef NDEBUG
                owner[i] = kInvalidPayloadSlot;
                owner_kind[i] = WidgetKind::None;
#endif
            }
        }

        PayloadHandle alloc(std::uint16_t owner_idx, WidgetKind kind) noexcept {
            if (free_head == kInvalidPayloadSlot) {
#if defined(VIVID_SOA_TRACE_INPUT)
                alloc_fail += 1;
#endif
                return invalid_payload_handle();
            }
            const std::uint16_t slot = free_head;
            free_head = free_next[slot];
            items[slot] = T{};
#if defined(VIVID_SOA_TRACE_INPUT)
            used = static_cast<std::uint16_t>(used + 1u);
            if (used > peak) peak = used;
#endif
#ifndef NDEBUG
            owner[slot] = owner_idx;
            owner_kind[slot] = kind;
#else
            (void)owner_idx;
            (void)kind;
#endif
            return PayloadHandle{slot, generation[slot]};
        }

        void free(PayloadHandle h, std::uint16_t owner_idx, WidgetKind kind) noexcept {
            if (!payload_valid(h) || h.slot >= N) return;
            const std::uint16_t slot = h.slot;
            if (generation[slot] != h.generation) return;
#ifndef NDEBUG
            if (owner[slot] != owner_idx) {
                assert(false && "PayloadPool owner mismatch");
            }
            if (owner_kind[slot] != kind) {
                assert(false && "PayloadPool kind mismatch");
            }
            owner[slot] = kInvalidPayloadSlot;
            owner_kind[slot] = WidgetKind::None;
#else
            (void)owner_idx;
            (void)kind;
#endif
            generation[slot] = static_cast<std::uint16_t>(generation[slot] + 1u);
            free_next[slot] = free_head;
            free_head = slot;
            items[slot] = T{};
#if defined(VIVID_SOA_TRACE_INPUT)
            if (used > 0) {
                used = static_cast<std::uint16_t>(used - 1u);
            }
#endif
        }

        T* get(PayloadHandle h, std::uint16_t owner_idx, WidgetKind kind) noexcept {
            if (!payload_valid(h) || h.slot >= N) return nullptr;
            const std::uint16_t slot = h.slot;
            if (generation[slot] != h.generation) return nullptr;
#ifndef NDEBUG
            if (owner[slot] != owner_idx) {
                assert(false && "PayloadPool owner mismatch");
                return nullptr;
            }
            if (owner_kind[slot] != kind) {
                assert(false && "PayloadPool kind mismatch");
                return nullptr;
            }
#else
            (void)owner_idx;
            (void)kind;
#endif
            return &items[slot];
        }

        const T* get(PayloadHandle h, std::uint16_t owner_idx, WidgetKind kind) const noexcept {
            if (!payload_valid(h) || h.slot >= N) return nullptr;
            const std::uint16_t slot = h.slot;
            if (generation[slot] != h.generation) return nullptr;
#ifndef NDEBUG
            if (owner[slot] != owner_idx) {
                assert(false && "PayloadPool owner mismatch");
                return nullptr;
            }
            if (owner_kind[slot] != kind) {
                assert(false && "PayloadPool kind mismatch");
                return nullptr;
            }
#else
            (void)owner_idx;
            (void)kind;
#endif
            return &items[slot];
        }

        PayloadPoolStats stats() const noexcept {
            PayloadPoolStats out{};
            out.cap = static_cast<std::uint16_t>(N);
#if defined(VIVID_SOA_TRACE_INPUT)
            out.used = used;
            out.peak = peak;
            out.alloc_fail = alloc_fail;
#endif
            return out;
        }
    };

    struct PayloadManager {
        void reset() noexcept {
            #define VIVID_PAYLOAD_KIND(name, member, stats_field, cap_kind) member.reset();
            #include "widgets.payload.kinds.def"
            #undef VIVID_PAYLOAD_KIND
            text_arena_.reset();
            for (auto& slot : text_slots_) {
                slot.used = false;
                slot.len = 0;
                if (!slot.data.empty()) {
                    slot.data[0] = '\0';
                }
            }
            static_used_.fill(false);
            static_ptrs_.fill(nullptr);
            static_lens_.fill(0);
            overflowed_ = false;
        }

        PayloadHandle alloc(PayloadKind payload,
                            WidgetKind kind,
                            std::uint16_t owner_idx) noexcept {
            switch (payload) {
                case PayloadKind::None:
                    return invalid_payload_handle();
#define VIVID_PAYLOAD_KIND(name, member, stats_field, cap_kind) \
                case PayloadKind::name: \
                    return handle_or_overflow(member.alloc(owner_idx, kind), payload);
#include "widgets.payload.kinds.def"
#undef VIVID_PAYLOAD_KIND
            }
            return invalid_payload_handle();
        }

        void free(PayloadKind payload,
                  WidgetKind kind,
                  PayloadHandle handle,
                  std::uint16_t owner_idx) noexcept {
            switch (payload) {
                case PayloadKind::None:
                    return;
#define VIVID_PAYLOAD_KIND(name, member, stats_field, cap_kind) \
                case PayloadKind::name: \
                    member.free(handle, owner_idx, kind); \
                    break;
#include "widgets.payload.kinds.def"
#undef VIVID_PAYLOAD_KIND
            }
        }

        template <typename T>
        T* get(PayloadHandle handle, std::uint16_t owner_idx, WidgetKind kind) noexcept {
            return pool_for<T>().get(handle, owner_idx, kind);
        }

        template <typename T>
        const T* get(PayloadHandle handle, std::uint16_t owner_idx, WidgetKind kind) const noexcept {
            return pool_for<T>().get(handle, owner_idx, kind);
        }

        TextId store_text(const char* text) noexcept {
            return text_arena_.alloc(text);
        }

        TextId store_text_static(const char* text) noexcept {
            if (!text) return empty_text_id();
            std::size_t len = std::strlen(text);
            if (len == 0) return empty_text_id();
            if (len > kTextLenMask) {
                len = kTextLenMask;
            }
            for (std::size_t i = 0; i < kTextStaticCount; ++i) {
                if (static_used_[i] && static_ptrs_[i] == text) {
                    return make_text_id(TextKind::Static,
                                        static_cast<std::uint16_t>(i),
                                        static_cast<std::uint16_t>(static_lens_[i]));
                }
            }
            for (std::size_t i = 0; i < kTextStaticCount; ++i) {
                if (!static_used_[i]) {
                    static_used_[i] = true;
                    static_ptrs_[i] = text;
                    static_lens_[i] = static_cast<std::uint16_t>(len);
                    return make_text_id(TextKind::Static,
                                        static_cast<std::uint16_t>(i),
                                        static_cast<std::uint16_t>(len));
                }
            }
            return text_arena_.alloc(text);
        }

        TextSlotId alloc_text_slot() noexcept {
            for (std::size_t i = 0; i < kTextSlotCount; ++i) {
                if (!text_slots_[i].used) {
                    text_slots_[i].used = true;
                    text_slots_[i].len = 0;
                    text_slots_[i].data[0] = '\0';
                    return static_cast<TextSlotId>(i);
                }
            }
            return kInvalidTextSlot;
        }

        void free_text_slot(TextSlotId slot) noexcept {
            if (slot == kInvalidTextSlot || slot >= kTextSlotCount) return;
            auto& text_slot = text_slots_[slot];
            text_slot.used = false;
            text_slot.len = 0;
            text_slot.data[0] = '\0';
        }

        TextId store_text_slot(TextSlotId slot, const char* text) noexcept {
            if (slot == kInvalidTextSlot || slot >= kTextSlotCount) {
                return text_arena_.alloc(text);
            }
            auto& text_slot = text_slots_[slot];
            if (!text_slot.used) {
                text_slot.used = true;
            }
            if (!text) {
                text_slot.len = 0;
                text_slot.data[0] = '\0';
                return empty_text_id();
            }
            std::size_t len = std::strlen(text);
            if (len == 0) {
                text_slot.len = 0;
                text_slot.data[0] = '\0';
                return empty_text_id();
            }
            if (len >= kTextSlotBytes) {
                len = kTextSlotBytes - 1;
            }
            if (len > kTextLenMask) {
                len = kTextLenMask;
            }
            std::memcpy(text_slot.data.data(), text, len);
            text_slot.data[len] = '\0';
            text_slot.len = static_cast<std::uint16_t>(len);
            return make_text_id(TextKind::Slot, slot, static_cast<std::uint16_t>(len));
        }

        std::span<const char> text_span(TextId id) const noexcept {
            if (!text_valid(id)) return {};
            const auto kind = text_kind(id);
            if (kind == TextKind::Arena) {
                return text_arena_.span(id);
            }
            const auto len = text_length(id);
            if (kind == TextKind::Static) {
                const auto idx = id.offset;
                if (idx >= kTextStaticCount || !static_used_[idx] || !static_ptrs_[idx]) return {};
                return std::span<const char>(static_ptrs_[idx], len);
            }
            if (kind == TextKind::Slot) {
                const auto idx = id.offset;
                if (idx >= kTextSlotCount || !text_slots_[idx].used) return {};
                return std::span<const char>(text_slots_[idx].data.data(), len);
            }
            return {};
        }

        const char* text_c_str(TextId id) const noexcept {
            if (!text_valid(id)) return "";
            const auto kind = text_kind(id);
            if (kind == TextKind::Arena) {
                return text_arena_.c_str(id);
            }
            if (kind == TextKind::Static) {
                const auto idx = id.offset;
                if (idx >= kTextStaticCount || !static_used_[idx] || !static_ptrs_[idx]) return "";
                return static_ptrs_[idx];
            }
            if (kind == TextKind::Slot) {
                const auto idx = id.offset;
                if (idx >= kTextSlotCount || !text_slots_[idx].used) return "";
                return text_slots_[idx].data.data();
            }
            return "";
        }

        bool overflowed() const noexcept {
            return overflowed_;
        }

        bool text_overflowed() const noexcept {
            return text_arena_.overflowed;
        }

        PayloadStats stats() const noexcept {
            PayloadStats out{};
#define VIVID_PAYLOAD_KIND(name, member, stats_field, cap_kind) \
            out.stats_field = member.stats();
#include "widgets.payload.kinds.def"
#undef VIVID_PAYLOAD_KIND
            out.overflowed = overflowed_;
            out.text_overflowed = text_arena_.overflowed;
            return out;
        }

    private:
        template <typename T>
        static constexpr bool always_false_v = false;

        PayloadHandle handle_or_overflow(PayloadHandle handle, PayloadKind payload) noexcept {
            if (payload != PayloadKind::None && !payload_valid(handle)) {
                overflowed_ = true;
#ifndef NDEBUG
                assert(false && "PayloadPool capacity exceeded");
#endif
            }
            return handle;
        }

#define VIVID_PAYLOAD_KIND(name, member, stats_field, cap_kind) \
        using name##Pool = PayloadPool<name##Payload, pool_cap(WidgetKind::cap_kind)>;
#include "widgets.payload.kinds.def"
#undef VIVID_PAYLOAD_KIND

        template <typename T>
        auto& pool_for() noexcept {
#define VIVID_PAYLOAD_KIND(name, member, stats_field, cap_kind) \
            if constexpr (std::is_same_v<T, name##Payload>) { \
                return member; \
            } else
#include "widgets.payload.kinds.def"
#undef VIVID_PAYLOAD_KIND
            {
                static_assert(always_false_v<T>, "Unknown payload type");
            }
        }

        template <typename T>
        const auto& pool_for() const noexcept {
#define VIVID_PAYLOAD_KIND(name, member, stats_field, cap_kind) \
            if constexpr (std::is_same_v<T, name##Payload>) { \
                return member; \
            } else
#include "widgets.payload.kinds.def"
#undef VIVID_PAYLOAD_KIND
            {
                static_assert(always_false_v<T>, "Unknown payload type");
            }
        }

#define VIVID_PAYLOAD_KIND(name, member, stats_field, cap_kind) \
        name##Pool member{};
#include "widgets.payload.kinds.def"
#undef VIVID_PAYLOAD_KIND
        TextArena text_arena_{};
        std::array<TextSlot, kTextSlotCount> text_slots_{};
        std::array<const char*, kTextStaticCount> static_ptrs_{};
        std::array<std::uint16_t, kTextStaticCount> static_lens_{};
        std::array<bool, kTextStaticCount> static_used_{};
        bool overflowed_{false};
    };
} // namespace soa_detail
