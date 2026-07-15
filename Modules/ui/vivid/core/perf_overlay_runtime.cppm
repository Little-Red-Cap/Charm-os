module;

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string_view>

export module charm.ui.vivid.perf_overlay_runtime;

export namespace ui::perf_overlay_runtime {
    inline constexpr std::size_t debug_line_count = 6;

    struct Snapshot {
        std::uint32_t dispatch_groups{0};
        std::uint32_t batch_flushes{0};
        std::uint32_t failed_cmds{0};
        std::uint32_t batch_shrink{0};
        std::uint32_t batch_shrink_rect{0};
        std::uint32_t batch_shrink_round{0};
        std::uint32_t group_rect{0};
        std::uint32_t group_text{0};
        std::uint32_t group_image{0};
        std::uint32_t group_line{0};
        std::uint32_t group_path{0};
        std::uint32_t group_other{0};
        std::uint32_t cmd_rect{0};
        std::uint32_t cmd_text{0};
        std::uint32_t cmd_image{0};
        std::uint32_t cmd_line{0};
        std::uint32_t cmd_path{0};
        std::uint32_t cmd_other{0};
    };
}

namespace {
    inline constexpr std::size_t kDebugLineSize = 96;
    inline constexpr std::size_t kDebugNameSize = 24;

    struct DebugLines {
        std::array<std::array<char, kDebugLineSize>, ui::perf_overlay_runtime::debug_line_count> text{};
        std::array<std::uint8_t, ui::perf_overlay_runtime::debug_line_count> len{};
    };

    struct DebugChannels {
        std::array<std::array<char, kDebugNameSize>, ui::perf_overlay_runtime::debug_line_count> name{};
        std::array<std::uint8_t, ui::perf_overlay_runtime::debug_line_count> len{};
    };

    ui::perf_overlay_runtime::Snapshot g_perf_overlay_stats{};
    DebugLines g_debug_lines{};
    DebugChannels g_debug_channels{};
    bool g_perf_overlay_stats_valid{false};

    bool channel_name_equals(std::size_t idx, std::string_view name) noexcept {
        const auto& slot = g_debug_channels.name[idx];
        const std::uint8_t len = g_debug_channels.len[idx];
        return len == name.size() && std::memcmp(slot.data(), name.data(), len) == 0;
    }

    std::size_t find_channel(std::string_view name) noexcept {
        for (std::size_t i = 0; i < ui::perf_overlay_runtime::debug_line_count; ++i) {
            if (g_debug_channels.len[i] == 0) continue;
            if (channel_name_equals(i, name)) return i;
        }
        return ui::perf_overlay_runtime::debug_line_count;
    }

    std::size_t alloc_channel(std::string_view name) noexcept {
        for (std::size_t i = 0; i < ui::perf_overlay_runtime::debug_line_count; ++i) {
            if (g_debug_channels.len[i] != 0) continue;
            auto& slot = g_debug_channels.name[i];
            const std::size_t len = (name.size() < slot.size() - 1u)
                ? name.size()
                : slot.size() - 1u;
            if (len > 0) std::memcpy(slot.data(), name.data(), len);
            slot[len] = '\0';
            g_debug_channels.len[i] = static_cast<std::uint8_t>(len);
            return i;
        }
        return ui::perf_overlay_runtime::debug_line_count;
    }
}

export namespace ui::perf_overlay_runtime {
    void set(const Snapshot& stats) noexcept {
        g_perf_overlay_stats = stats;
        g_perf_overlay_stats_valid = true;
    }

    void clear() noexcept {
        g_perf_overlay_stats = {};
        g_perf_overlay_stats_valid = false;
    }

    [[nodiscard]] bool valid() noexcept {
        return g_perf_overlay_stats_valid;
    }

    [[nodiscard]] const Snapshot& get() noexcept {
        return g_perf_overlay_stats;
    }

    [[nodiscard]] std::string_view debug_line(std::size_t idx) noexcept {
        if (idx >= debug_line_count) return {};
        return {g_debug_lines.text[idx].data(), g_debug_lines.len[idx]};
    }
}

export void set_perf_overlay_debug_line(std::size_t idx, std::string_view text) noexcept {
    if (idx >= ui::perf_overlay_runtime::debug_line_count) return;
    auto& slot = g_debug_lines.text[idx];
    const std::size_t len = (text.size() < slot.size() - 1u)
        ? text.size()
        : slot.size() - 1u;
    if (len > 0) std::memcpy(slot.data(), text.data(), len);
    slot[len] = '\0';
    g_debug_lines.len[idx] = static_cast<std::uint8_t>(len);
}

export void clear_perf_overlay_debug_line(std::size_t idx) noexcept {
    if (idx >= ui::perf_overlay_runtime::debug_line_count) return;
    g_debug_lines.text[idx][0] = '\0';
    g_debug_lines.len[idx] = 0;
}

export void clear_perf_overlay_debug_lines() noexcept {
    for (std::size_t i = 0; i < ui::perf_overlay_runtime::debug_line_count; ++i) {
        clear_perf_overlay_debug_line(i);
    }
}

export std::size_t perf_overlay_debug_channel(std::string_view name) noexcept {
    if (name.empty()) return ui::perf_overlay_runtime::debug_line_count;
    const std::size_t existing = find_channel(name);
    if (existing < ui::perf_overlay_runtime::debug_line_count) return existing;
    return alloc_channel(name);
}

export void set_perf_overlay_debug_channel(std::size_t idx, std::string_view text) noexcept {
    set_perf_overlay_debug_line(idx, text);
}

export std::string_view perf_overlay_debug_channel_name(std::size_t idx) noexcept {
    if (idx >= ui::perf_overlay_runtime::debug_line_count) return {};
    const std::uint8_t len = g_debug_channels.len[idx];
    if (len == 0) return {};
    return {g_debug_channels.name[idx].data(), len};
}

export void clear_perf_overlay_debug_channels() noexcept {
    for (std::size_t i = 0; i < ui::perf_overlay_runtime::debug_line_count; ++i) {
        g_debug_channels.name[i][0] = '\0';
        g_debug_channels.len[i] = 0;
    }
}
