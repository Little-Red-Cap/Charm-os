#pragma once

#include "capabilities/world.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <span>
#include <string_view>

namespace h747::host_support {

class StdoutLog {
public:
    [[nodiscard]] charm::cap::Transfer write(const std::span<const std::byte> bytes) noexcept {
        const auto written = std::fwrite(bytes.data(), 1U, bytes.size(), stdout);
        return charm::cap::Transfer{
            charm::cap::Status::from(written == bytes.size() ? charm::cap::StatusCode::ok
                                                             : charm::cap::StatusCode::io_error),
            written,
        };
    }

    [[nodiscard]] charm::cap::Transfer write(const std::string_view text) noexcept {
        return write(std::as_bytes(std::span<const char>{text.data(), text.size()}));
    }

    [[nodiscard]] charm::cap::Status flush() noexcept {
        return std::fflush(stdout) == 0 ? charm::cap::Status::ok()
                                        : charm::cap::Status::from(charm::cap::StatusCode::io_error);
    }
};

template <std::size_t Capacity>
class BufferedLogStorage {
public:
    [[nodiscard]] charm::cap::Transfer write(const std::span<const std::byte> bytes) noexcept {
        const auto remaining = bytes_.size() - used_;
        const auto count = bytes.size() < remaining ? bytes.size() : remaining;
        if (count != 0U) {
            std::memcpy(bytes_.data() + used_, bytes.data(), count);
            used_ += count;
        }
        ++writes_;
        return charm::cap::Transfer{charm::cap::Status::ok(), count};
    }

    [[nodiscard]] charm::cap::Transfer write(const std::string_view text) noexcept {
        return write(std::as_bytes(std::span<const char>{text.data(), text.size()}));
    }

    [[nodiscard]] charm::cap::Status flush() noexcept {
        return charm::cap::Status::ok();
    }

    [[nodiscard]] std::string_view view() const noexcept {
        return {bytes_.data(), used_};
    }

    [[nodiscard]] std::uint32_t write_count() const noexcept {
        return writes_;
    }

private:
    std::array<char, Capacity> bytes_{};
    std::size_t used_{0U};
    std::uint32_t writes_{0U};
};

using BufferedLog = BufferedLogStorage<128U>;

class ManualClock {
public:
    constexpr ManualClock() = default;

    explicit constexpr ManualClock(const std::uint32_t initial_tick_ms) noexcept
        : tick_(initial_tick_ms) {}

    [[nodiscard]] charm::cap::Milliseconds tick_ms() const noexcept {
        return {tick_};
    }

    void delay(const charm::cap::Milliseconds duration) noexcept {
        tick_ += duration.value;
    }

    void advance(const std::uint32_t ms) noexcept {
        tick_ += ms;
    }

private:
    std::uint32_t tick_{0U};
};

class MemoryRasterDisplay {
public:
    explicit constexpr MemoryRasterDisplay(charm::cap::FrameBuffer frame) noexcept
        : frame_(frame) {}

    [[nodiscard]] charm::cap::DisplayMode mode() const noexcept {
        return frame_.mode();
    }

    [[nodiscard]] charm::cap::Status present(const charm::cap::SurfaceView frame,
                                             std::span<const charm::cap::Rect> dirty_rects) noexcept {
        (void)dirty_rects;
        if (!charm::cap::same_mode(frame.mode, mode()) || frame.pixels.size() != frame_.pixels().size()) {
            return charm::cap::Status::from(charm::cap::StatusCode::invalid_argument);
        }
        std::memcpy(frame_.pixels().data(), frame.pixels.data(), frame.pixels.size());
        ++present_count_;
        return charm::cap::Status::ok();
    }

    [[nodiscard]] std::uint32_t present_count() const noexcept {
        return present_count_;
    }

private:
    charm::cap::FrameBuffer frame_;
    std::uint32_t present_count_{0U};
};

class NullInput {
public:
    [[nodiscard]] charm::cap::InputFrame sample() const noexcept {
        return {};
    }
};

template <std::uint16_t Width, std::uint16_t Height>
class HostFrameBufferStorage {
public:
    [[nodiscard]] charm::cap::FrameBuffer framebuffer() noexcept {
        return charm::cap::FrameBuffer{
            std::span<std::byte>{pixels_.data(), pixels_.size()},
            mode(),
        };
    }

    [[nodiscard]] static constexpr charm::cap::DisplayMode mode() noexcept {
        return charm::cap::DisplayMode{
            .extent = charm::cap::Extent2D{.width = Width, .height = Height},
            .format = charm::cap::PixelFormat::argb8888,
            .stride_bytes = Width * 4U,
        };
    }

    [[nodiscard]] std::span<const std::byte> pixels() const noexcept {
        return pixels_;
    }

    [[nodiscard]] std::span<std::byte> pixels() noexcept {
        return pixels_;
    }

private:
    std::array<std::byte, static_cast<std::size_t>(Width) * Height * 4U> pixels_{};
};

[[nodiscard]] inline bool has_arg(const int argc, char** argv, const char* expected) noexcept {
    for (int i = 1; i < argc; ++i) {
        if ((argv[i] != nullptr) && (std::strcmp(argv[i], expected) == 0)) {
            return true;
        }
    }
    return false;
}

[[nodiscard]] inline std::uint32_t fnv1a32(const std::span<const std::byte> bytes) noexcept {
    std::uint32_t hash = 2166136261U;
    for (const auto byte : bytes) {
        hash ^= static_cast<std::uint8_t>(byte);
        hash *= 16777619U;
    }
    return hash;
}

[[nodiscard]] inline bool write_ppm(const std::filesystem::path& path,
                                    const charm::cap::DisplayMode mode,
                                    const std::span<const std::byte> pixels) {
    auto* file = std::fopen(path.string().c_str(), "wb");
    if (file == nullptr) {
        return false;
    }
    std::fprintf(file, "P6\n%u %u\n255\n", mode.extent.width, mode.extent.height);
    for (std::size_t offset = 0U; offset + 3U < pixels.size(); offset += 4U) {
        const auto argb = reinterpret_cast<const std::uint8_t*>(pixels.data() + offset);
        const std::uint8_t rgb[3] = {argb[2], argb[1], argb[0]};
        std::fwrite(rgb, 1U, sizeof(rgb), file);
    }
    std::fclose(file);
    return true;
}

template <class World>
[[nodiscard]] bool write_ppm(const std::filesystem::path& path, World& world) {
    return write_ppm(path, world.mode(), world.pixels());
}

[[nodiscard]] inline std::filesystem::path output_path(const char* argv0, const char* filename) {
    if ((argv0 == nullptr) || (argv0[0] == '\0')) {
        return std::filesystem::current_path() / filename;
    }
    return std::filesystem::absolute(argv0).parent_path() / filename;
}

} // namespace h747::host_support
