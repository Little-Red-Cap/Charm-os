//
// Created by Joho on 2025/12/30.
//

module;
#include <cstdint>
export module app.logic;

import app.state;
import input.events;

export namespace app {
#if 0
    // 这里的“菜单项”先硬编码 2 个：WiFi / Battery（Battery 只显示不可改）
    enum class Item : std::uint8_t { Wifi = 0, Battery = 1, Count = 2 };

    inline void on_event(MainViewState& s, const input::Event& e) noexcept {
        if (e.type != input::Type::Key || !e.pressed) return;

        const int max = int(Item::Count);

        switch (e.key) {
        case input::Key::Up:
            s.selected = (s.selected - 1 + max) % max;
            break;
        case input::Key::Down:
            s.selected = (s.selected + 1) % max;
            break;
        case input::Key::Enter:
            if (s.selected == int(Item::Wifi)) s.wifi = !s.wifi;
            break;
        case input::Key::Back:
            // MVP：先不做页面栈，留空
            break;
        }
    }
#endif
} // namespace app
