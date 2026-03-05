module;

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>

export module bsp.st7305;

export namespace bsp::st7305 {
    export constexpr int kWidth = 168;
    export constexpr int kHeight = 384;
    export constexpr int kNativeStride = kWidth / 4;
    export constexpr int kNativeHeight = kHeight / 2;
    export constexpr std::size_t kNativeSize =
        static_cast<std::size_t>(kNativeStride) * static_cast<std::size_t>(kNativeHeight);
    export constexpr int kLinearStride = kWidth / 8;
    export constexpr std::size_t kLinearSize =
        static_cast<std::size_t>(kLinearStride) * static_cast<std::size_t>(kHeight);

    export enum class Status : std::uint8_t { ok, invalid };

    using TransmitFn = void (*)(std::uint8_t* data, std::uint16_t len, bool data_or_cmd);
    using ResetFn = void (*)(bool level);
    using DelayFn = void (*)(std::uint32_t ms);

    struct Io {
        TransmitFn transmit{nullptr};
        ResetFn reset{nullptr};
        DelayFn delay_ms{nullptr};
        bool reset_active_high{true};
    };

    class Panel {
    public:
        explicit Panel(Io io) noexcept : io_(io) {}

        Status init() noexcept;
        Status reset() noexcept;
        Status display_on(bool on) noexcept;
        Status invert(bool on) noexcept;
        Status set_mirror(bool mirror_x, bool mirror_y) noexcept;
        Status flush_native(std::span<const std::uint8_t> buf) noexcept;

        static void clear_native(std::span<std::uint8_t> buf, bool on) noexcept;
        static void set_native_pixel(std::span<std::uint8_t> buf, int x, int y, bool on) noexcept;
        static void pack_from_linear_1bpp(std::span<const std::uint8_t> src,
                                          std::span<std::uint8_t> dst) noexcept;

    private:
        Status write_cmd(std::uint8_t cmd) noexcept;
        Status write_data(std::span<const std::uint8_t> data) noexcept;
        Status write_cmd_data(std::uint8_t cmd, std::span<const std::uint8_t> data,
                              std::uint16_t delay_ms) noexcept;
        Status set_window_full() noexcept;

        static void set_native_pixel_unchecked(std::uint8_t* buf, int x, int y, bool on) noexcept;
        Status transmit(std::span<const std::uint8_t> data, bool data_or_cmd) noexcept;
        void delay_ms(std::uint16_t ms) noexcept;

        Io io_{};
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
    constexpr std::uint8_t kData_2A[] = {0x17, 0x24};
    constexpr std::uint8_t kData_2B[] = {0x00, 0xBF};
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
        {0x2A, kData_2A, sizeof(kData_2A), 0},
        {0x2B, kData_2B, sizeof(kData_2B), 0},
        {0xD0, kData_D0, sizeof(kData_D0), 0},
        {0x39, nullptr, 0, 0},
        {0x29, nullptr, 0, 100},
        {0x20, nullptr, 0, 0},
        {0xBB, kData_BB, sizeof(kData_BB), 0},
    };
}

namespace bsp::st7305 {
    Status Panel::init() noexcept {
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
        return Status::ok;
    }

    Status Panel::reset() noexcept {
        if (!io_.reset) return Status::ok;
        const bool active = io_.reset_active_high;
        io_.reset(active);
        delay_ms(10);
        io_.reset(!active);
        delay_ms(10);
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

    Status Panel::flush_native(std::span<const std::uint8_t> buf) noexcept {
        if (buf.size() < kNativeSize) return Status::invalid;
        auto st = set_window_full();
        if (st != Status::ok) return st;
        return write_data(buf.subspan(0, kNativeSize));
    }

    void Panel::clear_native(std::span<std::uint8_t> buf, bool on) noexcept {
        if (buf.size() < kNativeSize) return;
        std::memset(buf.data(), on ? 0xFF : 0x00, kNativeSize);
    }

    void Panel::set_native_pixel(std::span<std::uint8_t> buf, int x, int y, bool on) noexcept {
        if (buf.size() < kNativeSize) return;
        set_native_pixel_unchecked(buf.data(), x, y, on);
    }

    void Panel::pack_from_linear_1bpp(std::span<const std::uint8_t> src,
                                      std::span<std::uint8_t> dst) noexcept {
        if (src.size() < kLinearSize || dst.size() < kNativeSize) return;
        std::memset(dst.data(), 0, kNativeSize);
        std::uint8_t* out = dst.data();
        for (int y = 0; y < kHeight; ++y) {
            const std::size_t row = static_cast<std::size_t>(y) * static_cast<std::size_t>(kLinearStride);
            for (int x = 0; x < kWidth; ++x) {
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
        auto st = write_cmd_data(0x2A, std::span<const std::uint8_t>{kData_2A, sizeof(kData_2A)}, 0);
        if (st != Status::ok) return st;
        st = write_cmd_data(0x2B, std::span<const std::uint8_t>{kData_2B, sizeof(kData_2B)}, 0);
        if (st != Status::ok) return st;
        return write_cmd(0x2C);
    }

    void Panel::set_native_pixel_unchecked(std::uint8_t* buf, int x, int y, bool on) noexcept {
        if (!buf) return;
        if (x < 0 || y < 0 || x >= kWidth || y >= kHeight) return;
        const int real_x = x >> 2;
        const int real_y = y >> 1;
        const int byte_index = real_y * kNativeStride + real_x;
        const int one_two = y & 1;
        const int line_bit_4 = x & 3;
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
}
