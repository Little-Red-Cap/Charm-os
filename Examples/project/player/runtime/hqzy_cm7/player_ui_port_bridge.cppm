module;

#include <cstddef>
#include <cstdint>

export module player.runtime.hqzy_cm7.player_ui_port_bridge;

import charm.port;
import player.stm32h7.board_config;

export namespace player::app_test_hqzy::player_ui_port_bridge {
    enum class PixelFormat : std::uint8_t {
        RGB565,
        RGB888,
        ARGB8888,
    };

    struct DirtyRegion {
        int x{0};
        int y{0};
        int w{0};
        int h{0};

        [[nodiscard]] bool empty() const noexcept {
            return w <= 0 || h <= 0;
        }
    };

    struct PortProbeResult {
        bool framebuffer_valid{false};
        bool clock_valid{false};
        bool clock_read{false};
        bool touch_valid{false};
        bool present_callback_valid{false};
        bool dirty_valid{false};
        bool clean_cache_ok{true};
        bool flush_dirty_ok{true};
        bool present_ok{true};
        bool ok{false};
        DirtyRegion dirty{};
        std::uint32_t now_ms{0};
    };

    struct Framebuffer {
        std::byte* pixels{nullptr};
        int width{0};
        int height{0};
        std::size_t stride_bytes{0};
        PixelFormat pixel_format{PixelFormat::RGB565};

        [[nodiscard]] bool valid() const noexcept {
            return pixels != nullptr && width > 0 && height > 0 && stride_bytes >= min_stride_bytes();
        }

        [[nodiscard]] std::size_t bytes_per_pixel() const noexcept {
            switch (pixel_format) {
            case PixelFormat::RGB565: return 2;
            case PixelFormat::RGB888: return 3;
            case PixelFormat::ARGB8888: return 4;
            }
            return 0;
        }

        [[nodiscard]] std::size_t min_stride_bytes() const noexcept {
            return width > 0 ? static_cast<std::size_t>(width) * bytes_per_pixel() : 0;
        }

        [[nodiscard]] std::size_t size_bytes() const noexcept {
            return height > 0 ? stride_bytes * static_cast<std::size_t>(height) : 0;
        }
    };

    inline DirtyRegion full_dirty_region(const Framebuffer& fb) noexcept {
        return DirtyRegion{0, 0, fb.width, fb.height};
    }

    inline DirtyRegion clip_dirty_region(DirtyRegion dirty, const Framebuffer& fb) noexcept {
        if (dirty.empty() || fb.width <= 0 || fb.height <= 0) {
            return {};
        }

        const int x0 = dirty.x < 0 ? 0 : dirty.x;
        const int y0 = dirty.y < 0 ? 0 : dirty.y;
        const int x1_raw = dirty.x + dirty.w;
        const int y1_raw = dirty.y + dirty.h;
        const int x1 = x1_raw > fb.width ? fb.width : x1_raw;
        const int y1 = y1_raw > fb.height ? fb.height : y1_raw;
        if (x1 <= x0 || y1 <= y0) {
            return {};
        }
        return DirtyRegion{x0, y0, x1 - x0, y1 - y0};
    }

    struct TouchSampleSource {
        void* ctx{nullptr};
        bool (*read)(void* ctx, bool& down, float& x, float& y, std::uint8_t& id, std::uint32_t& ms) noexcept{nullptr};

        [[nodiscard]] bool valid() const noexcept {
            return read != nullptr;
        }
    };

    struct DisplayCallbacks {
        void* ctx{nullptr};
        bool (*clean_cache)(void* ctx, const Framebuffer& fb, DirtyRegion dirty) noexcept{nullptr};
        bool (*flush_dirty)(void* ctx, const Framebuffer& fb, DirtyRegion dirty) noexcept{nullptr};
        bool (*present)(void* ctx, const Framebuffer& fb, DirtyRegion dirty) noexcept{nullptr};
    };

    struct ClockSource {
        void* ctx{nullptr};
        std::uint32_t (*now_ms)(void* ctx) noexcept{nullptr};

        [[nodiscard]] std::uint32_t read_ms() const noexcept {
            return now_ms ? now_ms(ctx) : 0u;
        }

        [[nodiscard]] bool valid() const noexcept {
            return now_ms != nullptr;
        }
    };

    struct PortConfig {
        Framebuffer framebuffer{};
        DisplayCallbacks display{};
        TouchSampleSource touch{};
        ClockSource clock{};

        [[nodiscard]] bool valid() const noexcept {
            return framebuffer.valid() && clock.valid();
        }
    };

    inline bool present_dirty(const PortConfig& config, DirtyRegion dirty) noexcept {
        if (!config.framebuffer.valid()) {
            return false;
        }

        const DirtyRegion clipped = clip_dirty_region(dirty, config.framebuffer);
        bool ok = true;
        if (!clipped.empty() && config.display.clean_cache) {
            ok = config.display.clean_cache(config.display.ctx, config.framebuffer, clipped) && ok;
        }
        if (!clipped.empty() && config.display.flush_dirty) {
            ok = config.display.flush_dirty(config.display.ctx, config.framebuffer, clipped) && ok;
        }
        if (config.display.present) {
            ok = config.display.present(config.display.ctx, config.framebuffer, clipped) && ok;
        }
        return ok;
    }

    inline PortProbeResult probe_port(const PortConfig& config) noexcept {
        PortProbeResult result{};
        result.framebuffer_valid = config.framebuffer.valid();
        result.clock_valid = config.clock.valid();
        result.touch_valid = config.touch.valid();
        result.present_callback_valid = config.display.present != nullptr;
        if (result.clock_valid) {
            result.now_ms = config.clock.read_ms();
            result.clock_read = true;
        }
        result.dirty = result.framebuffer_valid ? full_dirty_region(config.framebuffer) : DirtyRegion{};
        result.dirty = clip_dirty_region(result.dirty, config.framebuffer);
        result.dirty_valid = !result.dirty.empty();

        if (result.framebuffer_valid) {
            const auto& display = config.display;
            if (result.dirty_valid && display.clean_cache) {
                result.clean_cache_ok = display.clean_cache(display.ctx, config.framebuffer, result.dirty);
            }
            if (result.dirty_valid && display.flush_dirty) {
                result.flush_dirty_ok = display.flush_dirty(display.ctx, config.framebuffer, result.dirty);
            }
            if (display.present) {
                result.present_ok = display.present(display.ctx, config.framebuffer, result.dirty);
            }
        }

        result.ok = result.framebuffer_valid
            && result.clock_valid
            && result.clock_read
            && result.dirty_valid
            && result.clean_cache_ok
            && result.flush_dirty_ok
            && result.present_ok;
        return result;
    }

    inline Framebuffer make_sdram_framebuffer(int width,
                                              int height,
                                              PixelFormat format = PixelFormat::RGB565) noexcept {
        const std::size_t bpp = format == PixelFormat::RGB565 ? 2u
            : (format == PixelFormat::RGB888 ? 3u : 4u);
        return Framebuffer{
            reinterpret_cast<std::byte*>(player::stm32h7::board::kSdram.base),
            width,
            height,
            static_cast<std::size_t>(width > 0 ? width : 0) * bpp,
            format,
        };
    }

    inline std::uint32_t port_now_ms(void*) noexcept {
        return static_cast<std::uint32_t>(charm::port::now_ms(nullptr));
    }

    inline PortConfig make_default_port_config(int width, int height) noexcept {
        return PortConfig{
            .framebuffer = make_sdram_framebuffer(width, height),
            .display = {},
            .touch = {},
            .clock = ClockSource{nullptr, &port_now_ms},
        };
    }
}
