module;

#include <cstdint>
#include <optional>

export module hal_input;

import input.raw;

export namespace hal {
    struct RawInputDriver {
        void* ctx{nullptr};
        bool (*is_down)(void* ctx, input::Button button) noexcept { nullptr };
        input::PointerRaw (*read_pointer)(void* ctx) noexcept { nullptr };
        input::AxisRaw (*read_axis)(void* ctx) noexcept { nullptr };
        std::optional<std::uint8_t> (*pop_encoder_ab)(void* ctx) noexcept { nullptr };
    };

    class RawInputSource {
    public:
        explicit RawInputSource(const RawInputDriver& driver) noexcept
            : driver_(&driver) {}

        bool is_down(input::Button button) const noexcept {
            if (!driver_ || !driver_->is_down) return false;
            return driver_->is_down(driver_->ctx, button);
        }

        input::PointerRaw read_pointer() const noexcept {
            if (!driver_ || !driver_->read_pointer) return input::PointerRaw{};
            return driver_->read_pointer(driver_->ctx);
        }

        input::AxisRaw read_axis() const noexcept {
            if (!driver_ || !driver_->read_axis) return input::AxisRaw{};
            return driver_->read_axis(driver_->ctx);
        }

        std::optional<std::uint8_t> pop_encoder_ab() noexcept {
            if (!driver_ || !driver_->pop_encoder_ab) return std::nullopt;
            return driver_->pop_encoder_ab(driver_->ctx);
        }

    private:
        const RawInputDriver* driver_{nullptr};
    };
}
