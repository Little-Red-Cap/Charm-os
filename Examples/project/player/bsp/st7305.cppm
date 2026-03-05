module;

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>

export module bsp.st7305;

export namespace bsp::st7305 {
    export constexpr int kDefaultWidth = 168;
    export constexpr int kDefaultHeight = 384;

    export enum class Status : std::uint8_t { ok, invalid, timeout };

    using TransmitFn = void (*)(std::uint8_t* data, std::uint16_t len, bool data_or_cmd);
    using ResetFn = void (*)(bool level);
    using DelayFn = void (*)(std::uint32_t ms);
    using WaitFn = bool (*)(std::uint32_t timeout_ms);

    struct Geometry {
        int width{kDefaultWidth};
        int height{kDefaultHeight};
        int x_offset_pixels{0};
        std::uint8_t caset_base{0x17};
        std::uint8_t raset_base{0x00};
    };

    export constexpr Geometry kDefaultGeometry{};

    export constexpr int native_unit_width_pixels() noexcept { return 12; }
    export constexpr int native_unit_height_pixels() noexcept { return 2; }
    export constexpr int native_unit_bytes() noexcept { return 3; }

    export constexpr int native_stride_bytes_for(const Geometry& g) noexcept {
        const int x_units = (g.width + g.x_offset_pixels + native_unit_width_pixels() - 1)
            / native_unit_width_pixels();
        return x_units * native_unit_bytes();
    }

    export constexpr std::size_t native_size_for(const Geometry& g) noexcept {
        const int x_units = (g.width + g.x_offset_pixels + native_unit_width_pixels() - 1)
            / native_unit_width_pixels();
        const int y_units = (g.height + native_unit_height_pixels() - 1)
            / native_unit_height_pixels();
        return static_cast<std::size_t>(x_units * native_unit_bytes() * y_units);
    }

    export constexpr std::size_t linear_size_for(const Geometry& g) noexcept {
        const int stride = g.width / 8;
        return static_cast<std::size_t>(stride * g.height);
    }

    struct Io {
        TransmitFn transmit{nullptr};
        ResetFn reset{nullptr};
        DelayFn delay_ms{nullptr};
        WaitFn wait_te{nullptr};
        bool reset_active_high{true};
    };

    struct InitOptions {
        bool mirror_x{false};
        bool mirror_y{false};
        bool invert{false};
        bool high_power{false};
    };

    class Panel {
    public:
        explicit Panel(Io io, Geometry geom = kDefaultGeometry) noexcept
            : io_(io), geom_(geom) {}

        Status init() noexcept;
        Status init(const InitOptions& options) noexcept;
        Status reset() noexcept;
        Status display_on(bool on) noexcept;
        Status invert(bool on) noexcept;
        Status set_mirror(bool mirror_x, bool mirror_y) noexcept;
        Status set_power_mode(bool high_power) noexcept;
        Status flush_native(std::span<const std::uint8_t> buf) noexcept;
        Status flush_native_rect(std::span<const std::uint8_t> buf,
                                 int x0, int y0, int x1, int y1) noexcept;

        [[nodiscard]] Geometry geometry() const noexcept { return geom_; }
        [[nodiscard]] std::size_t native_size() const noexcept { return native_size_for(geom_); }
        [[nodiscard]] std::size_t linear_size() const noexcept { return linear_size_for(geom_); }
        [[nodiscard]] int native_stride_bytes() const noexcept { return native_stride_bytes_for(geom_); }

        void clear_native(std::span<std::uint8_t> buf, bool on) noexcept;
        void set_native_pixel(std::span<std::uint8_t> buf, int x, int y, bool on) noexcept;
        void pack_from_linear_1bpp(std::span<const std::uint8_t> src,
                                   std::span<std::uint8_t> dst) noexcept;

    private:
        Status write_cmd(std::uint8_t cmd) noexcept;
        Status write_data(std::span<const std::uint8_t> data) noexcept;
        Status write_cmd_data(std::uint8_t cmd, std::span<const std::uint8_t> data,
                              std::uint16_t delay_ms) noexcept;
        Status set_window_full() noexcept;
        Status set_window_units(int x_unit0, int x_unit1, int y_unit0, int y_unit1) noexcept;

        void set_native_pixel_unchecked(std::uint8_t* buf, int x, int y, bool on) noexcept;
        Status transmit(std::span<const std::uint8_t> data, bool data_or_cmd) noexcept;
        void delay_ms(std::uint16_t ms) noexcept;
        bool wait_te(std::uint32_t timeout_ms) noexcept;

        Io io_{};
        Geometry geom_{};
        std::uint8_t madctl_{0x48};
    };
}

namespace {
    constexpr std::uint8_t kMadctlMx = 0x40;
    constexpr std::uint8_t kMadctlMy = 0x80;
    constexpr std::uint8_t kMadctlBase = 0x48;

    constexpr std::uint8_t kData_D6[] = {0x13, 0x02};
    constexpr std::uint8_t kData_D1[] = {0x01};
    constexpr std::uint8_t kData_C0[] = {0x12, 0x0A};
    constexpr std::uint8_t kData_C1[] = {0x3C, 0x3E, 0x3C, 0x3C};
    constexpr std::uint8_t kData_C2[] = {0x23, 0x21, 0x23, 0x23};
    constexpr std::uint8_t kData_C4[] = {0x5A, 0x5C, 0x5A, 0x5A};
    constexpr std::uint8_t kData_C5[] = {0x37, 0x35, 0x37, 0x37};
    constexpr std::uint8_t kData_D8[] = {0xA6, 0xE9};
    constexpr std::uint8_t kData_B2[] = {0x12};
    constexpr std::uint8_t kData_B3[] = {0xE5, 0xF6, 0x17, 0x77, 0x77, 0x77, 0x77, 0x77, 0x77, 0x71};
    constexpr std::uint8_t kData_B4[] = {0x05, 0x46, 0x77, 0x77, 0x77, 0x77, 0x76, 0x45};
    constexpr std::uint8_t kData_62[] = {0x32, 0x03, 0x1F};
    constexpr std::uint8_t kData_B7[] = {0x13};
    constexpr std::uint8_t kData_B0[] = {0x60};
    constexpr std::uint8_t kData_C9[] = {0x00};
    constexpr std::uint8_t kData_36[] = {kMadctlBase};
    constexpr std::uint8_t kData_3A[] = {0x11};
    constexpr std::uint8_t kData_B9[] = {0x20};
    constexpr std::uint8_t kData_B8[] = {0x29};
    constexpr std::uint8_t kData_D0[] = {0xFF};
    constexpr std::uint8_t kData_BB[] = {0x4F};

    struct InitCmd {
        std::uint8_t cmd;
        const std::uint8_t* data;
        std::uint8_t data_len;
        std::uint16_t delay_ms;
    };

    constexpr InitCmd kInitSeq[] = {
        {0xD6, kData_D6, sizeof(kData_D6), 0},
        {0xD1, kData_D1, sizeof(kData_D1), 0},
        {0xC0, kData_C0, sizeof(kData_C0), 0},
        {0xC1, kData_C1, sizeof(kData_C1), 0},
        {0xC2, kData_C2, sizeof(kData_C2), 0},
        {0xC4, kData_C4, sizeof(kData_C4), 0},
        {0xC5, kData_C5, sizeof(kData_C5), 0},
        {0xD8, kData_D8, sizeof(kData_D8), 0},
        {0xB2, kData_B2, sizeof(kData_B2), 0},
        {0xB3, kData_B3, sizeof(kData_B3), 0},
        {0xB4, kData_B4, sizeof(kData_B4), 0},
        {0x62, kData_62, sizeof(kData_62), 0},
        {0xB7, kData_B7, sizeof(kData_B7), 0},
        {0xB0, kData_B0, sizeof(kData_B0), 0},
        {0x11, nullptr, 0, 120},
        {0xC9, kData_C9, sizeof(kData_C9), 0},
        {0x36, kData_36, sizeof(kData_36), 0},
        {0x3A, kData_3A, sizeof(kData_3A), 0},
        {0xB9, kData_B9, sizeof(kData_B9), 0},
        {0xB8, kData_B8, sizeof(kData_B8), 0},
        {0xD0, kData_D0, sizeof(kData_D0), 0},
        {0x39, nullptr, 0, 0},
        {0x29, nullptr, 0, 100},
        {0x20, nullptr, 0, 0},
        {0xBB, kData_BB, sizeof(kData_BB), 0},
    };
}

namespace bsp::st7305 {
    Status Panel::init() noexcept {
        InitOptions options{};
        return init(options);
    }

    Status Panel::init(const InitOptions& options) noexcept {
        if (!io_.transmit) return Status::invalid;
        auto st = reset();
        if (st != Status::ok) return st;
        for (const auto& cmd : kInitSeq) {
            const std::span<const std::uint8_t> payload{
                cmd.data,
                cmd.data ? static_cast<std::size_t>(cmd.data_len) : 0u
            };
            st = write_cmd_data(cmd.cmd, payload, cmd.delay_ms);
            if (st != Status::ok) return st;
        }
        st = set_mirror(options.mirror_x, options.mirror_y);
        if (st != Status::ok) return st;
        st = invert(options.invert);
        if (st != Status::ok) return st;
        st = set_power_mode(options.high_power);
        if (st != Status::ok) return st;
        return Status::ok;
    }

    Status Panel::reset() noexcept {
        if (!io_.reset) return Status::ok;
        const bool active = io_.reset_active_high;
        io_.reset(active);
        delay_ms(20);
        io_.reset(!active);
        delay_ms(20);
        return Status::ok;
    }

    Status Panel::display_on(bool on) noexcept {
        return write_cmd(on ? 0x29 : 0x28);
    }

    Status Panel::invert(bool on) noexcept {
        return write_cmd(on ? 0x21 : 0x20);
    }

    Status Panel::set_mirror(bool mirror_x, bool mirror_y) noexcept {
        std::uint8_t value = madctl_;
        value &= static_cast<std::uint8_t>(~(kMadctlMx | kMadctlMy));
        if (mirror_x) value |= kMadctlMx;
        if (mirror_y) value |= kMadctlMy;
        madctl_ = value;
        return write_cmd_data(0x36, std::span<const std::uint8_t>{&madctl_, 1}, 0);
    }

    Status Panel::set_power_mode(bool high_power) noexcept {
        return write_cmd(high_power ? 0x38 : 0x39);
    }

    Status Panel::flush_native(std::span<const std::uint8_t> buf) noexcept {
        if (buf.size() < native_size()) return Status::invalid;
        if (!wait_te(20)) return Status::timeout;
        auto st = set_window_full();
        if (st != Status::ok) return st;
        return write_data(buf.subspan(0, native_size()));
    }

    Status Panel::flush_native_rect(std::span<const std::uint8_t> buf,
                                    int x0, int y0, int x1, int y1) noexcept {
        if (buf.size() < native_size()) return Status::invalid;
        if (x1 <= x0 || y1 <= y0) return Status::invalid;
        if (x0 < 0) x0 = 0;
        if (y0 < 0) y0 = 0;
        if (x1 > geom_.width) x1 = geom_.width;
        if (y1 > geom_.height) y1 = geom_.height;
        const int x_unit0 = (x0 + geom_.x_offset_pixels) / native_unit_width_pixels();
        const int x_unit1 = (x1 - 1 + geom_.x_offset_pixels) / native_unit_width_pixels();
        const int y_unit0 = y0 / native_unit_height_pixels();
        const int y_unit1 = (y1 - 1) / native_unit_height_pixels();
        if (!wait_te(20)) return Status::timeout;
        auto st = set_window_units(x_unit0, x_unit1, y_unit0, y_unit1);
        if (st != Status::ok) return st;
        const int bytes_per_row = native_stride_bytes();
        const int span_units = x_unit1 - x_unit0 + 1;
        const int span_bytes = span_units * native_unit_bytes();
        for (int row = y_unit0; row <= y_unit1; ++row) {
            const std::size_t offset = static_cast<std::size_t>(
                row * bytes_per_row + x_unit0 * native_unit_bytes()
            );
            const auto row_view = buf.subspan(offset, static_cast<std::size_t>(span_bytes));
            st = write_data(row_view);
            if (st != Status::ok) return st;
        }
        return Status::ok;
    }

    void Panel::clear_native(std::span<std::uint8_t> buf, bool on) noexcept {
        if (buf.size() < native_size()) return;
        std::memset(buf.data(), on ? 0xFF : 0x00, native_size());
    }

    void Panel::set_native_pixel(std::span<std::uint8_t> buf, int x, int y, bool on) noexcept {
        if (buf.size() < native_size()) return;
        set_native_pixel_unchecked(buf.data(), x, y, on);
    }

    void Panel::pack_from_linear_1bpp(std::span<const std::uint8_t> src,
                                      std::span<std::uint8_t> dst) noexcept {
        if (src.size() < linear_size() || dst.size() < native_size()) return;
        std::memset(dst.data(), 0, native_size());
        std::uint8_t* out = dst.data();
        const int width = geom_.width;
        const int height = geom_.height;
        const int stride = width / 8;
        for (int y = 0; y < height; ++y) {
            const std::size_t row = static_cast<std::size_t>(y) * static_cast<std::size_t>(stride);
            for (int x = 0; x < width; ++x) {
                const std::uint8_t byte = src[row + (x >> 3)];
                const std::uint8_t mask = static_cast<std::uint8_t>(0x80u >> (x & 7));
                const bool on = (byte & mask) != 0;
                set_native_pixel_unchecked(out, x, y, on);
            }
        }
    }

    Status Panel::write_cmd(std::uint8_t cmd) noexcept {
        const std::uint8_t value = cmd;
        return transmit(std::span<const std::uint8_t>{&value, 1}, false);
    }

    Status Panel::write_data(std::span<const std::uint8_t> data) noexcept {
        return transmit(data, true);
    }

    Status Panel::write_cmd_data(std::uint8_t cmd, std::span<const std::uint8_t> data,
                                 std::uint16_t delay_ms_value) noexcept {
        auto st = write_cmd(cmd);
        if (st != Status::ok) return st;
        if (!data.empty()) {
            st = write_data(data);
            if (st != Status::ok) return st;
        }
        delay_ms(delay_ms_value);
        return Status::ok;
    }

    Status Panel::set_window_full() noexcept {
        const int x_units = (geom_.width + geom_.x_offset_pixels + native_unit_width_pixels() - 1)
            / native_unit_width_pixels();
        const int y_units = (geom_.height + native_unit_height_pixels() - 1)
            / native_unit_height_pixels();
        return set_window_units(0, x_units - 1, 0, y_units - 1);
    }

    Status Panel::set_window_units(int x_unit0, int x_unit1, int y_unit0, int y_unit1) noexcept {
        if (x_unit1 < x_unit0 || y_unit1 < y_unit0) return Status::invalid;
        const std::uint8_t caset[] = {
            static_cast<std::uint8_t>(geom_.caset_base + x_unit0),
            static_cast<std::uint8_t>(geom_.caset_base + x_unit1)
        };
        const std::uint8_t raset[] = {
            static_cast<std::uint8_t>(geom_.raset_base + y_unit0),
            static_cast<std::uint8_t>(geom_.raset_base + y_unit1)
        };
        auto st = write_cmd_data(0x2A, std::span<const std::uint8_t>{caset, sizeof(caset)}, 0);
        if (st != Status::ok) return st;
        st = write_cmd_data(0x2B, std::span<const std::uint8_t>{raset, sizeof(raset)}, 0);
        if (st != Status::ok) return st;
        return write_cmd(0x2C);
    }

    void Panel::set_native_pixel_unchecked(std::uint8_t* buf, int x, int y, bool on) noexcept {
        if (!buf) return;
        const int width = geom_.width;
        const int height = geom_.height;
        if (x < 0 || y < 0 || x >= width || y >= height) return;
        const int x_with_offset = x + geom_.x_offset_pixels;
        const int real_x = x_with_offset >> 2;
        const int real_y = y >> 1;
        const int byte_index = real_y * native_stride_bytes() + real_x;
        const int one_two = y & 1;
        const int line_bit_4 = x_with_offset & 3;
        const int bit = 7 - (line_bit_4 * 2 + one_two);
        const std::uint8_t mask = static_cast<std::uint8_t>(1u << bit);
        if (on) {
            buf[byte_index] |= mask;
        } else {
            buf[byte_index] &= static_cast<std::uint8_t>(~mask);
        }
    }

    Status Panel::transmit(std::span<const std::uint8_t> data, bool data_or_cmd) noexcept {
        if (!io_.transmit) return Status::invalid;
        if (data.empty()) return Status::ok;
        std::size_t offset = 0;
        while (offset < data.size()) {
            std::size_t remaining = data.size() - offset;
            if (remaining > 0xFFFFu) remaining = 0xFFFFu;
            io_.transmit(const_cast<std::uint8_t*>(data.data() + offset),
                         static_cast<std::uint16_t>(remaining),
                         data_or_cmd);
            offset += remaining;
        }
        return Status::ok;
    }

    void Panel::delay_ms(std::uint16_t ms) noexcept {
        if (ms == 0) return;
        if (!io_.delay_ms) return;
        io_.delay_ms(ms);
    }

    bool Panel::wait_te(std::uint32_t timeout_ms) noexcept {
        if (!io_.wait_te) return true;
        return io_.wait_te(timeout_ms);
    }
}
