//
// Minimal UI feedback helpers.
//

module;
#include <cstdint>
export module gui.ui_feedback;

export namespace gui::ui {

    [[nodiscard]] inline bool flash_active(std::uint32_t now_ms,
                                           std::uint32_t last_ms,
                                           std::uint32_t duration_ms = 120) noexcept
    {
        return (last_ms != 0) && ((now_ms - last_ms) < duration_ms);
    }

} // namespace gui::ui
