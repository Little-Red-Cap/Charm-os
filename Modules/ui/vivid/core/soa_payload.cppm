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

    constexpr bool text_valid(TextId id) noexcept {
        return id.length != 0;
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
            if (len + 1 > (kTextArenaBytes - used)) {
                overflowed = true;
                return text_valid(overflow_id) ? overflow_id : empty_text_id();
            }
            const std::uint16_t offset = used;
            std::memcpy(data.data() + used, text, len);
            data[used + len] = '\0';
            used = static_cast<std::uint16_t>(used + len + 1);
            return TextId{offset, static_cast<std::uint16_t>(len)};
        }

        std::span<const char> span(TextId id) const noexcept {
            if (!text_valid(id)) return {};
            const std::uint32_t end = static_cast<std::uint32_t>(id.offset) + id.length;
            if (end > used) return {};
            return std::span<const char>(data.data() + id.offset, id.length);
        }

        const char* c_str(TextId id) const noexcept {
            if (!text_valid(id)) return "";
            const std::uint32_t end = static_cast<std::uint32_t>(id.offset) + id.length;
            if (end > used) return "";
            return data.data() + id.offset;
        }

    private:
        TextId store_literal(const char* text) noexcept {
            if (!text) return empty_text_id();
            const std::size_t len = std::strlen(text);
            if (len == 0) return empty_text_id();
            if (len + 1 > (kTextArenaBytes - used)) return empty_text_id();
            const std::uint16_t offset = used;
            std::memcpy(data.data() + used, text, len);
            data[used + len] = '\0';
            used = static_cast<std::uint16_t>(used + len + 1);
            return TextId{offset, static_cast<std::uint16_t>(len)};
        }
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

    struct SegmentedControlPayload {
        std::array<TextId, kMaxSegments> labels{};
        std::uint8_t count{0};
        std::uint8_t selected{0};
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
    using ListViewIconFn = ImageId (*)(const void*, std::uint16_t) noexcept;
    using TableViewTextFn = const char* (*)(const void*, std::uint16_t, std::uint8_t) noexcept;
    using TableViewHeaderFn = const char* (*)(const void*, std::uint8_t) noexcept;
    using TableViewColWidthFn = int (*)(const void*, std::uint8_t) noexcept;
    using TreeViewTextFn = const char* (*)(const void*, std::uint16_t) noexcept;
    using TreeViewIndentFn = std::uint8_t (*)(const void*, std::uint16_t) noexcept;

    struct ListViewPayload {
        const void* text_ctx{nullptr};
        ListViewTextFn text_fn{nullptr};
        const void* icon_ctx{nullptr};
        ListViewIconFn icon_fn{nullptr};
        std::uint16_t count{0};
        std::int16_t selected{-1};
        int scroll_y{0};
        int row_height{28};
        int wheel_step{24};
        std::uint8_t overscan{2};
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
        Label,
        Button,
        Image,
        TextInput,
        TextArea,
        NumberInput,
        SegmentedControl,
        ToggleGroup,
        Checkbox,
        Radio,
        ListItem,
        TextList,
        ListView,
        TableView,
        TreeView,
        Switch,
        Slider,
        ScrollBar,
        Progress,
        List,
        ScrollContainer,
        Spinner
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
            label_.reset();
            button_.reset();
            image_.reset();
            text_input_.reset();
            text_area_.reset();
            number_input_.reset();
            segmented_.reset();
            toggle_group_.reset();
            checkbox_.reset();
            radio_.reset();
            list_item_.reset();
            text_list_.reset();
            list_view_.reset();
            table_view_.reset();
            tree_view_.reset();
            switch_.reset();
            slider_.reset();
            progress_.reset();
            scrollbar_.reset();
            list_.reset();
            scroll_container_.reset();
            spinner_.reset();
            text_arena_.reset();
            overflowed_ = false;
        }

        PayloadHandle alloc(PayloadKind payload,
                            WidgetKind kind,
                            std::uint16_t owner_idx) noexcept {
            switch (payload) {
                case PayloadKind::None:
                    return invalid_payload_handle();
                case PayloadKind::Label:
                    return handle_or_overflow(label_.alloc(owner_idx, kind), payload);
                case PayloadKind::Button:
                    return handle_or_overflow(button_.alloc(owner_idx, kind), payload);
                case PayloadKind::Image:
                    return handle_or_overflow(image_.alloc(owner_idx, kind), payload);
                case PayloadKind::TextInput:
                    return handle_or_overflow(text_input_.alloc(owner_idx, kind), payload);
                case PayloadKind::TextArea:
                    return handle_or_overflow(text_area_.alloc(owner_idx, kind), payload);
                case PayloadKind::NumberInput:
                    return handle_or_overflow(number_input_.alloc(owner_idx, kind), payload);
                case PayloadKind::SegmentedControl:
                    return handle_or_overflow(segmented_.alloc(owner_idx, kind), payload);
                case PayloadKind::ToggleGroup:
                    return handle_or_overflow(toggle_group_.alloc(owner_idx, kind), payload);
                case PayloadKind::Checkbox:
                    return handle_or_overflow(checkbox_.alloc(owner_idx, kind), payload);
                case PayloadKind::Radio:
                    return handle_or_overflow(radio_.alloc(owner_idx, kind), payload);
                case PayloadKind::ListItem:
                    return handle_or_overflow(list_item_.alloc(owner_idx, kind), payload);
                case PayloadKind::TextList:
                    return handle_or_overflow(text_list_.alloc(owner_idx, kind), payload);
                case PayloadKind::ListView:
                    return handle_or_overflow(list_view_.alloc(owner_idx, kind), payload);
                case PayloadKind::TableView:
                    return handle_or_overflow(table_view_.alloc(owner_idx, kind), payload);
                case PayloadKind::TreeView:
                    return handle_or_overflow(tree_view_.alloc(owner_idx, kind), payload);
                case PayloadKind::Switch:
                    return handle_or_overflow(switch_.alloc(owner_idx, kind), payload);
                case PayloadKind::Slider:
                    return handle_or_overflow(slider_.alloc(owner_idx, kind), payload);
                case PayloadKind::ScrollBar:
                    return handle_or_overflow(scrollbar_.alloc(owner_idx, kind), payload);
                case PayloadKind::Progress:
                    return handle_or_overflow(progress_.alloc(owner_idx, kind), payload);
                case PayloadKind::List:
                    return handle_or_overflow(list_.alloc(owner_idx, kind), payload);
                case PayloadKind::ScrollContainer:
                    return handle_or_overflow(scroll_container_.alloc(owner_idx, kind), payload);
                case PayloadKind::Spinner:
                    return handle_or_overflow(spinner_.alloc(owner_idx, kind), payload);
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
                case PayloadKind::Label:
                    label_.free(handle, owner_idx, kind);
                    break;
                case PayloadKind::Button:
                    button_.free(handle, owner_idx, kind);
                    break;
                case PayloadKind::Image:
                    image_.free(handle, owner_idx, kind);
                    break;
                case PayloadKind::TextInput:
                    text_input_.free(handle, owner_idx, kind);
                    break;
                case PayloadKind::TextArea:
                    text_area_.free(handle, owner_idx, kind);
                    break;
                case PayloadKind::NumberInput:
                    number_input_.free(handle, owner_idx, kind);
                    break;
                case PayloadKind::SegmentedControl:
                    segmented_.free(handle, owner_idx, kind);
                    break;
                case PayloadKind::ToggleGroup:
                    toggle_group_.free(handle, owner_idx, kind);
                    break;
                case PayloadKind::Checkbox:
                    checkbox_.free(handle, owner_idx, kind);
                    break;
                case PayloadKind::Radio:
                    radio_.free(handle, owner_idx, kind);
                    break;
                case PayloadKind::ListItem:
                    list_item_.free(handle, owner_idx, kind);
                    break;
                case PayloadKind::TextList:
                    text_list_.free(handle, owner_idx, kind);
                    break;
                case PayloadKind::ListView:
                    list_view_.free(handle, owner_idx, kind);
                    break;
                case PayloadKind::TableView:
                    table_view_.free(handle, owner_idx, kind);
                    break;
                case PayloadKind::TreeView:
                    tree_view_.free(handle, owner_idx, kind);
                    break;
                case PayloadKind::Switch:
                    switch_.free(handle, owner_idx, kind);
                    break;
                case PayloadKind::Slider:
                    slider_.free(handle, owner_idx, kind);
                    break;
                case PayloadKind::ScrollBar:
                    scrollbar_.free(handle, owner_idx, kind);
                    break;
                case PayloadKind::Progress:
                    progress_.free(handle, owner_idx, kind);
                    break;
                case PayloadKind::List:
                    list_.free(handle, owner_idx, kind);
                    break;
                case PayloadKind::ScrollContainer:
                    scroll_container_.free(handle, owner_idx, kind);
                    break;
                case PayloadKind::Spinner:
                    spinner_.free(handle, owner_idx, kind);
                    break;
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

        std::span<const char> text_span(TextId id) const noexcept {
            return text_arena_.span(id);
        }

        const char* text_c_str(TextId id) const noexcept {
            return text_arena_.c_str(id);
        }

        bool overflowed() const noexcept {
            return overflowed_;
        }

        bool text_overflowed() const noexcept {
            return text_arena_.overflowed;
        }

        PayloadStats stats() const noexcept {
            PayloadStats out{};
            out.label = label_.stats();
            out.button = button_.stats();
            out.image = image_.stats();
            out.text_input = text_input_.stats();
            out.text_area = text_area_.stats();
            out.number_input = number_input_.stats();
            out.segmented = segmented_.stats();
            out.toggle_group = toggle_group_.stats();
            out.checkbox = checkbox_.stats();
            out.radio = radio_.stats();
            out.list_item = list_item_.stats();
            out.text_list = text_list_.stats();
            out.list_view = list_view_.stats();
            out.table_view = table_view_.stats();
            out.tree_view = tree_view_.stats();
            out.switcher = switch_.stats();
            out.slider = slider_.stats();
            out.progress = progress_.stats();
            out.scrollbar = scrollbar_.stats();
            out.list = list_.stats();
            out.scroll_container = scroll_container_.stats();
            out.spinner = spinner_.stats();
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

        using LabelPool = PayloadPool<LabelPayload, pool_cap(WidgetKind::Label)>;
        using ButtonPool = PayloadPool<ButtonPayload, pool_cap(WidgetKind::Button)>;
        using ImagePool = PayloadPool<ImagePayload, pool_cap(WidgetKind::Image)>;
        using TextInputPool = PayloadPool<TextInputPayload, pool_cap(WidgetKind::TextInput)>;
        using TextAreaPool = PayloadPool<TextAreaPayload, pool_cap(WidgetKind::TextArea)>;
        using NumberInputPool = PayloadPool<NumberInputPayload, pool_cap(WidgetKind::NumberInput)>;
        using SegmentedPool = PayloadPool<SegmentedControlPayload, pool_cap(WidgetKind::SegmentedControl)>;
        using ToggleGroupPool = PayloadPool<ToggleGroupPayload, pool_cap(WidgetKind::ToggleGroup)>;
        using CheckboxPool = PayloadPool<CheckboxPayload, pool_cap(WidgetKind::Checkbox)>;
        using RadioPool = PayloadPool<RadioPayload, pool_cap(WidgetKind::Radio)>;
        using ListItemPool = PayloadPool<ListItemPayload, pool_cap(WidgetKind::ListItem)>;
        using TextListPool = PayloadPool<TextListPayload, pool_cap(WidgetKind::TextList)>;
        using ListViewPool = PayloadPool<ListViewPayload, pool_cap(WidgetKind::ListView)>;
        using TableViewPool = PayloadPool<TableViewPayload, pool_cap(WidgetKind::TableView)>;
        using TreeViewPool = PayloadPool<TreeViewPayload, pool_cap(WidgetKind::TreeView)>;
        using SwitchPool = PayloadPool<SwitchPayload, pool_cap(WidgetKind::Switch)>;
        using SliderPool = PayloadPool<SliderPayload, pool_cap(WidgetKind::Slider)>;
        using ProgressPool = PayloadPool<ProgressPayload, pool_cap(WidgetKind::Progress)>;
        using ScrollBarPool = PayloadPool<ScrollBarPayload, pool_cap(WidgetKind::ScrollBar)>;
        using ListPool = PayloadPool<ListPayload, pool_cap(WidgetKind::List)>;
        using ScrollContainerPool = PayloadPool<ScrollContainerPayload, pool_cap(WidgetKind::ScrollContainer)>;
        using SpinnerPool = PayloadPool<SpinnerPayload, pool_cap(WidgetKind::Spinner)>;

        template <typename T>
        auto& pool_for() noexcept {
            if constexpr (std::is_same_v<T, LabelPayload>) {
                return label_;
            } else if constexpr (std::is_same_v<T, ButtonPayload>) {
                return button_;
            } else if constexpr (std::is_same_v<T, ImagePayload>) {
                return image_;
            } else if constexpr (std::is_same_v<T, TextInputPayload>) {
                return text_input_;
            } else if constexpr (std::is_same_v<T, TextAreaPayload>) {
                return text_area_;
            } else if constexpr (std::is_same_v<T, NumberInputPayload>) {
                return number_input_;
            } else if constexpr (std::is_same_v<T, SegmentedControlPayload>) {
                return segmented_;
            } else if constexpr (std::is_same_v<T, ToggleGroupPayload>) {
                return toggle_group_;
            } else if constexpr (std::is_same_v<T, CheckboxPayload>) {
                return checkbox_;
            } else if constexpr (std::is_same_v<T, RadioPayload>) {
                return radio_;
            } else if constexpr (std::is_same_v<T, ListItemPayload>) {
                return list_item_;
            } else if constexpr (std::is_same_v<T, TextListPayload>) {
                return text_list_;
            } else if constexpr (std::is_same_v<T, ListViewPayload>) {
                return list_view_;
            } else if constexpr (std::is_same_v<T, TableViewPayload>) {
                return table_view_;
            } else if constexpr (std::is_same_v<T, TreeViewPayload>) {
                return tree_view_;
            } else if constexpr (std::is_same_v<T, SwitchPayload>) {
                return switch_;
            } else if constexpr (std::is_same_v<T, SliderPayload>) {
                return slider_;
            } else if constexpr (std::is_same_v<T, ProgressPayload>) {
                return progress_;
            } else if constexpr (std::is_same_v<T, ScrollBarPayload>) {
                return scrollbar_;
            } else if constexpr (std::is_same_v<T, ListPayload>) {
                return list_;
            } else if constexpr (std::is_same_v<T, ScrollContainerPayload>) {
                return scroll_container_;
            } else if constexpr (std::is_same_v<T, SpinnerPayload>) {
                return spinner_;
            } else {
                static_assert(always_false_v<T>, "Unknown payload type");
            }
        }

        template <typename T>
        const auto& pool_for() const noexcept {
            if constexpr (std::is_same_v<T, LabelPayload>) {
                return label_;
            } else if constexpr (std::is_same_v<T, ButtonPayload>) {
                return button_;
            } else if constexpr (std::is_same_v<T, ImagePayload>) {
                return image_;
            } else if constexpr (std::is_same_v<T, TextInputPayload>) {
                return text_input_;
            } else if constexpr (std::is_same_v<T, TextAreaPayload>) {
                return text_area_;
            } else if constexpr (std::is_same_v<T, NumberInputPayload>) {
                return number_input_;
            } else if constexpr (std::is_same_v<T, SegmentedControlPayload>) {
                return segmented_;
            } else if constexpr (std::is_same_v<T, ToggleGroupPayload>) {
                return toggle_group_;
            } else if constexpr (std::is_same_v<T, CheckboxPayload>) {
                return checkbox_;
            } else if constexpr (std::is_same_v<T, RadioPayload>) {
                return radio_;
            } else if constexpr (std::is_same_v<T, ListItemPayload>) {
                return list_item_;
            } else if constexpr (std::is_same_v<T, TextListPayload>) {
                return text_list_;
            } else if constexpr (std::is_same_v<T, ListViewPayload>) {
                return list_view_;
            } else if constexpr (std::is_same_v<T, TableViewPayload>) {
                return table_view_;
            } else if constexpr (std::is_same_v<T, TreeViewPayload>) {
                return tree_view_;
            } else if constexpr (std::is_same_v<T, SwitchPayload>) {
                return switch_;
            } else if constexpr (std::is_same_v<T, SliderPayload>) {
                return slider_;
            } else if constexpr (std::is_same_v<T, ProgressPayload>) {
                return progress_;
            } else if constexpr (std::is_same_v<T, ScrollBarPayload>) {
                return scrollbar_;
            } else if constexpr (std::is_same_v<T, ListPayload>) {
                return list_;
            } else if constexpr (std::is_same_v<T, ScrollContainerPayload>) {
                return scroll_container_;
            } else if constexpr (std::is_same_v<T, SpinnerPayload>) {
                return spinner_;
            } else {
                static_assert(always_false_v<T>, "Unknown payload type");
            }
        }

        LabelPool label_{};
        ButtonPool button_{};
        ImagePool image_{};
        TextInputPool text_input_{};
        TextAreaPool text_area_{};
        NumberInputPool number_input_{};
        SegmentedPool segmented_{};
        ToggleGroupPool toggle_group_{};
        CheckboxPool checkbox_{};
        RadioPool radio_{};
        ListItemPool list_item_{};
        TextListPool text_list_{};
        ListViewPool list_view_{};
        TableViewPool table_view_{};
        TreeViewPool tree_view_{};
        SwitchPool switch_{};
        SliderPool slider_{};
        ProgressPool progress_{};
        ScrollBarPool scrollbar_{};
        ListPool list_{};
        ScrollContainerPool scroll_container_{};
        SpinnerPool spinner_{};
        TextArena text_arena_{};
        bool overflowed_{false};
    };
} // namespace soa_detail
