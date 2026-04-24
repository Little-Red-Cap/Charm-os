//
// Created by Joho on 2026/03/05.
//

module;
#include <cstdint>
#include <span>
#include <string_view>

export module ui.menu_model;

import input.intent;

export namespace ui {
    enum class MenuSignal : std::uint8_t {
        None,
        Selection,
        Value,
        Activate,
        Back,
    };

    struct MenuDelta {
        MenuSignal    signal{MenuSignal::None};
        std::uint16_t index{0};
    };

    using MenuAction = void (*)(void* ctx) noexcept;
    using MenuAdjust = void (*)(void* ctx, int delta) noexcept;

    struct MenuItemOps {
        MenuAction activate{nullptr};
        MenuAdjust adjust{nullptr};
    };

    struct MenuItem {
        std::string_view label{};
        void*            ctx{nullptr};
        MenuItemOps      ops{};
        bool             selectable{true};
    };

    class MenuModel {
    public:
        explicit MenuModel(std::span<MenuItem> items, std::uint16_t selected = 0) noexcept
            : items_(items) {
            selected_ = clamp_index(selected);
            if (!items_.empty() && !items_[selected_].selectable) {
                select_next();
            }
        }

        MenuDelta dispatch(const input::Intent& intent) noexcept {
            if (items_.empty()) {
                return {};
            }

            switch (intent.type) {
                case input::IntentType::NavPrev:
                    if (select_prev()) {
                        return {MenuSignal::Selection, selected_};
                    }
                    break;
                case input::IntentType::NavNext:
                    if (select_next()) {
                        return {MenuSignal::Selection, selected_};
                    }
                    break;
                case input::IntentType::Activate:
                    if (auto* item = selected_item()) {
                        if (item->ops.activate) {
                            item->ops.activate(item->ctx);
                            return {MenuSignal::Activate, selected_};
                        }
                    }
                    break;
                case input::IntentType::Back:
                    return {MenuSignal::Back, selected_};
                case input::IntentType::Adjust:
                    if (auto* item = selected_item()) {
                        if (item->ops.adjust) {
                            item->ops.adjust(item->ctx, static_cast<int>(intent.a));
                            return {MenuSignal::Value, selected_};
                        }
                    }
                    break;
                default:
                    break;
            }
            return {};
        }

        std::uint16_t selected() const noexcept { return selected_; }
        std::span<const MenuItem> items() const noexcept { return items_; }

        MenuItem* selected_item() noexcept {
            if (items_.empty()) {
                return nullptr;
            }
            return &items_[selected_];
        }

    private:
        std::uint16_t clamp_index(std::uint16_t idx) const noexcept {
            if (items_.empty()) {
                return 0;
            }
            if (idx >= items_.size()) {
                return static_cast<std::uint16_t>(items_.size() - 1);
            }
            return idx;
        }

        bool select_next() noexcept {
            if (items_.empty()) {
                return false;
            }
            const std::uint16_t start = selected_;
            std::uint16_t idx = selected_;
            for (;;) {
                idx = static_cast<std::uint16_t>((idx + 1) % items_.size());
                if (items_[idx].selectable) {
                    selected_ = idx;
                    return idx != start;
                }
                if (idx == start) {
                    return false;
                }
            }
        }

        bool select_prev() noexcept {
            if (items_.empty()) {
                return false;
            }
            const std::uint16_t start = selected_;
            std::uint16_t idx = selected_;
            for (;;) {
                idx = static_cast<std::uint16_t>((idx + items_.size() - 1) % items_.size());
                if (items_[idx].selectable) {
                    selected_ = idx;
                    return idx != start;
                }
                if (idx == start) {
                    return false;
                }
            }
        }

        std::span<MenuItem> items_{};
        std::uint16_t selected_{0};
    };
} // namespace ui
