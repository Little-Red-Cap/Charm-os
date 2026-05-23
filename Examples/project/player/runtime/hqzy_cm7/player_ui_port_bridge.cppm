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
    };

    struct PortConfig {
        Framebuffer framebuffer{};
        DisplayCallbacks display{};
        TouchSampleSource touch{};
        ClockSource clock{};

        [[nodiscard]] bool valid() const noexcept {
            return framebuffer.valid() && clock.now_ms != nullptr;
        }
    };

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
