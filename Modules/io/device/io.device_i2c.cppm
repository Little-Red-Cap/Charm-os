module;

#include <concepts>
#include <span>

export module io.device_i2c;

import util.core;
import util.error;
import util.expected;

export namespace io::device {
    using I2cAddress = util::u16;
    using ByteView = std::span<const util::u8>;
    using MutByteView = std::span<util::u8>;

    enum class I2cErrorKind : util::u8 {
        ok = 0,
        bus,
        arbitration_lost,
        nack_address,
        nack_data,
        overrun,
        timeout,
        target_detached,
        policy_violation,
        unsupported,
        unknown,
    };

    [[nodiscard]] constexpr util::Errc to_errc(I2cErrorKind kind) noexcept {
        switch (kind) {
        case I2cErrorKind::ok:
            return util::Errc::ok;
        case I2cErrorKind::timeout:
            return util::Errc::timeout;
        case I2cErrorKind::target_detached:
            return util::Errc::noent;
        case I2cErrorKind::policy_violation:
            return util::Errc::invalid_arg;
        case I2cErrorKind::unsupported:
            return util::Errc::not_supported;
        case I2cErrorKind::bus:
        case I2cErrorKind::arbitration_lost:
        case I2cErrorKind::nack_address:
        case I2cErrorKind::nack_data:
        case I2cErrorKind::overrun:
        case I2cErrorKind::unknown:
        default:
            return util::Errc::io;
        }
    }

    using I2cResult = util::Result<void>;

    [[nodiscard]] constexpr I2cResult i2c_fail(I2cErrorKind kind) noexcept {
        return util::unexpected(to_errc(kind));
    }

    template <typename T>
    concept I2cBus = requires(T& bus, I2cAddress address, ByteView tx, MutByteView rx) {
        { bus.write(address, tx) } noexcept -> std::same_as<I2cResult>;
        { bus.read(address, rx) } noexcept -> std::same_as<I2cResult>;
        { bus.write_read(address, tx, rx) } noexcept -> std::same_as<I2cResult>;
    };

    struct I2cBusOps {
        I2cResult (*write)(void*, I2cAddress, ByteView) noexcept { nullptr };
        I2cResult (*read)(void*, I2cAddress, MutByteView) noexcept { nullptr };
        I2cResult (*write_read)(void*, I2cAddress, ByteView, MutByteView) noexcept { nullptr };
    };

    struct I2cBusRef {
        void* self{nullptr};
        const I2cBusOps* ops{nullptr};

        [[nodiscard]] constexpr bool valid() const noexcept {
            return self != nullptr
                && ops != nullptr
                && ops->write != nullptr
                && ops->read != nullptr
                && ops->write_read != nullptr;
        }

        [[nodiscard]] I2cResult write(I2cAddress address, ByteView tx) const noexcept {
            if (!valid()) {
                return util::unexpected(util::Errc::invalid_arg);
            }
            return ops->write(self, address, tx);
        }

        [[nodiscard]] I2cResult read(I2cAddress address, MutByteView rx) const noexcept {
            if (!valid()) {
                return util::unexpected(util::Errc::invalid_arg);
            }
            return ops->read(self, address, rx);
        }

        [[nodiscard]] I2cResult write_read(I2cAddress address,
                                           ByteView tx,
                                           MutByteView rx) const noexcept {
            if (!valid()) {
                return util::unexpected(util::Errc::invalid_arg);
            }
            return ops->write_read(self, address, tx, rx);
        }
    };

    template <I2cBus T>
    [[nodiscard]] inline const I2cBusOps* i2c_bus_ops() noexcept {
        static const I2cBusOps ops{
            .write = [](void* self, I2cAddress address, ByteView tx) noexcept {
                return static_cast<T*>(self)->write(address, tx);
            },
            .read = [](void* self, I2cAddress address, MutByteView rx) noexcept {
                return static_cast<T*>(self)->read(address, rx);
            },
            .write_read = [](void* self, I2cAddress address, ByteView tx, MutByteView rx) noexcept {
                return static_cast<T*>(self)->write_read(address, tx, rx);
            },
        };
        return &ops;
    }

    template <I2cBus T>
    [[nodiscard]] inline I2cBusRef make_i2c_bus_ref(T& bus) noexcept {
        return I2cBusRef{&bus, i2c_bus_ops<T>()};
    }

    struct I2cDeviceRef {
        I2cBusRef bus{};
        I2cAddress address{0};

        [[nodiscard]] constexpr bool valid() const noexcept {
            return bus.valid();
        }

        [[nodiscard]] I2cResult write(ByteView tx) const noexcept {
            return bus.write(address, tx);
        }

        [[nodiscard]] I2cResult read(MutByteView rx) const noexcept {
            return bus.read(address, rx);
        }

        [[nodiscard]] I2cResult write_read(ByteView tx, MutByteView rx) const noexcept {
            return bus.write_read(address, tx, rx);
        }
    };

    [[nodiscard]] constexpr I2cDeviceRef make_i2c_device_ref(I2cBusRef bus,
                                                             I2cAddress address) noexcept {
        return I2cDeviceRef{bus, address};
    }
}
