// gui.ui_list_builder.cppm
// Fixed-capacity list builder (zero alloc, fluent API).

module;
#include <cstdint>
#include <span>

export module gui.ui_list_builder;

import gui.list_view;
import gui.ui_list_page;
import gui.widgets;
import gui.ui_context;
import gui.ui_tree;

export namespace gui::ui
{
    template<class Ctx, int Max>
    struct ListBuilder {
        gui::ListItem<Ctx> items[Max]{};
        std::int16_t count{0};

        [[nodiscard]] inline std::int16_t size() const noexcept { return count; }
        [[nodiscard]] inline bool full() const noexcept { return count >= Max; }

        inline void clear() noexcept { count = 0; }

        [[nodiscard]] inline std::span<const gui::ListItem<Ctx>> span() const noexcept
        {
            return { items, (std::size_t)count };
        }

        [[nodiscard]] inline const gui::ListItem<Ctx>* at(std::int16_t i) const noexcept
        {
            if (i < 0 || i >= count) return nullptr;
            return &items[i];
        }

        [[nodiscard]] inline auto row_fn() const noexcept
        {
            return [this](std::int16_t i) noexcept -> const gui::ListItem<Ctx>* {
                return this->at(i);
            };
        }

        inline ListBuilder& add_action(const char* label, void (*on_activate)(Ctx&) noexcept) noexcept
        {
            if (full()) return *this;
            auto& it = items[count++];
            it.label = label;
            it.kind = gui::ItemKind::Action;
            it.on_activate = on_activate;
            return *this;
        }

        inline ListBuilder& add_toggle(const char* label,
                                       bool (*get_bool)(const Ctx&) noexcept,
                                       void (*on_activate)(Ctx&) noexcept) noexcept
        {
            if (full()) return *this;
            auto& it = items[count++];
            it.label = label;
            it.kind = gui::ItemKind::Toggle;
            it.get_bool = get_bool;
            it.on_activate = on_activate;
            return *this;
        }

        inline ListBuilder& add_progress(const char* label,
                                         std::uint8_t (*get_u8)(const Ctx&) noexcept,
                                         void (*on_activate)(Ctx&) noexcept = nullptr) noexcept
        {
            if (full()) return *this;
            auto& it = items[count++];
            it.label = label;
            it.kind = gui::ItemKind::Progress;
            it.get_u8 = get_u8;
            it.on_activate = on_activate;
            return *this;
        }

        inline ListBuilder& add_checkbox(const char* label,
                                         bool (*get_bool)(const Ctx&) noexcept,
                                         void (*on_activate)(Ctx&) noexcept) noexcept
        {
            if (full()) return *this;
            auto& it = items[count++];
            it.label = label;
            it.kind = gui::ItemKind::Checkbox;
            it.get_bool = get_bool;
            it.on_activate = on_activate;
            return *this;
        }

        inline ListBuilder& add_switch(const char* label,
                                       bool (*get_bool)(const Ctx&) noexcept,
                                       void (*on_activate)(Ctx&) noexcept) noexcept
        {
            if (full()) return *this;
            auto& it = items[count++];
            it.label = label;
            it.kind = gui::ItemKind::Switch;
            it.get_bool = get_bool;
            it.on_activate = on_activate;
            return *this;
        }

        inline ListBuilder& add_chart(const char* label,
                                      gui::ChartView (*get_chart)(const Ctx&) noexcept,
                                      void (*on_activate)(Ctx&) noexcept = nullptr) noexcept
        {
            if (full()) return *this;
            auto& it = items[count++];
            it.label = label;
            it.kind = gui::ItemKind::Chart;
            it.get_chart = get_chart;
            it.on_activate = on_activate;
            return *this;
        }

        inline ListBuilder& add_value(const char* label,
                                      std::uint16_t (*get_u16)(const Ctx&) noexcept,
                                      void (*on_activate)(Ctx&) noexcept = nullptr) noexcept
        {
            if (full()) return *this;
            auto& it = items[count++];
            it.label = label;
            it.kind = gui::ItemKind::Value;
            it.get_u16 = get_u16;
            it.on_activate = on_activate;
            return *this;
        }

        inline ListBuilder& add_value_text(const char* label,
                                           std::uint16_t (*get_u16)(const Ctx&) noexcept,
                                           const char* (*get_value_label)(const Ctx&) noexcept,
                                           void (*on_activate)(Ctx&) noexcept = nullptr) noexcept
        {
            if (full()) return *this;
            auto& it = items[count++];
            it.label = label;
            it.kind = gui::ItemKind::Value;
            it.get_u16 = get_u16;
            it.get_value_label = get_value_label;
            it.on_activate = on_activate;
            return *this;
        }

        inline ListBuilder& add_value_inline(const char* label,
                                             const char* (*get_value_text)(const Ctx&) noexcept,
                                             void (*on_activate)(Ctx&) noexcept = nullptr) noexcept
        {
            if (full()) return *this;
            auto& it = items[count++];
            it.label = label;
            it.kind = gui::ItemKind::ValueText;
            it.get_value_text = get_value_text;
            it.on_activate = on_activate;
            return *this;
        }

        inline ListBuilder& add_section(const char* label) noexcept
        {
            if (full()) return *this;
            auto& it = items[count++];
            it.label = label;
            it.kind = gui::ItemKind::Section;
            return *this;
        }

        inline ListBuilder& add_range(const char* label,
                                      std::uint8_t (*get_u8)(const Ctx&) noexcept,
                                      void (*on_activate)(Ctx&) noexcept = nullptr) noexcept
        {
            if (full()) return *this;
            auto& it = items[count++];
            it.label = label;
            it.kind = gui::ItemKind::Range;
            it.get_u8 = get_u8;
            it.on_activate = on_activate;
            return *this;
        }

        inline ListBuilder& add_stepper(const char* label,
                                        std::uint16_t (*get_u16)(const Ctx&) noexcept,
                                        const char* (*get_value_label)(const Ctx&) noexcept = nullptr,
                                        void (*on_activate)(Ctx&) noexcept = nullptr) noexcept
        {
            if (full()) return *this;
            auto& it = items[count++];
            it.label = label;
            it.kind = gui::ItemKind::Stepper;
            it.get_u16 = get_u16;
            it.get_value_label = get_value_label;
            it.on_activate = on_activate;
            return *this;
        }

        inline ListBuilder& add_segmented(const char* label,
                                          const char* const* labels,
                                          std::uint8_t count,
                                          std::uint8_t (*get_index)(const Ctx&) noexcept,
                                          void (*on_activate)(Ctx&) noexcept = nullptr) noexcept
        {
            if (full()) return *this;
            auto& it = items[count++];
            it.label = label;
            it.kind = gui::ItemKind::Segmented;
            it.segments = labels;
            it.segment_count = count;
            it.get_index = get_index;
            it.on_activate = on_activate;
            return *this;
        }
    };

    template<class Ctx, int Max>
    [[nodiscard]] inline ListPageSpec make_list_spec(UiContext& ctx,
                                                     NodeId domain_id,
                                                     const ListBuilder<Ctx, Max>& b,
                                                     ListKind kind,
                                                     ListPreset preset = ListPreset::Standard) noexcept
    {
        return default_list_spec(ctx, domain_id, b.size(), kind, preset);
    }

    template<class Ctx, int Max>
    [[nodiscard]] inline ListPageSpec make_list_spec(UiContext& ctx,
                                                     NodeId domain_id,
                                                     const ListBuilder<Ctx, Max>& b,
                                                     ListKind kind) noexcept
    {
        return default_list_spec(ctx, domain_id, b.size(), kind, ListPreset::Standard);
    }
} // namespace gui::ui
