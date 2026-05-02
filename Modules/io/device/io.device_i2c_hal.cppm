module;

export module io.device_i2c_hal;

import hal_core;
import hal_i2c;
import io.device_i2c;
import util.error;
import util.expected;

export namespace io::device {
    [[nodiscard]] constexpr util::Errc hal_i2c_status_to_errc(hal::Status status) noexcept {
        switch (status) {
        case hal::Status::ok:
            return util::Errc::ok;
        case hal::Status::busy:
            return util::Errc::busy;
        case hal::Status::timeout:
            return util::Errc::timeout;
        case hal::Status::unsupported:
            return util::Errc::not_supported;
        case hal::Status::error:
        default:
            return util::Errc::io;
        }
    }

    [[nodiscard]] constexpr I2cResult hal_i2c_result_to_i2c_result(hal::Result result) noexcept {
        if (result) {
            return {};
        }
        return util::unexpected(hal_i2c_status_to_errc(result.status));
    }

    class HalI2cBus {
    public:
        constexpr explicit HalI2cBus(hal::I2cIoHandle handle) noexcept
            : handle_(handle) {}

        [[nodiscard]] I2cResult write(I2cAddress address, ByteView tx) noexcept {
            return hal_i2c_result_to_i2c_result(hal::i2c_write(handle_, address, tx));
        }

        [[nodiscard]] I2cResult read(I2cAddress address, MutByteView rx) noexcept {
            return hal_i2c_result_to_i2c_result(hal::i2c_read(handle_, address, rx));
        }

        [[nodiscard]] I2cResult write_read(I2cAddress address,
                                           ByteView tx,
                                           MutByteView rx) noexcept {
            return hal_i2c_result_to_i2c_result(hal::i2c_write_read(handle_, address, tx, rx));
        }

        [[nodiscard]] constexpr hal::I2cIoHandle handle() const noexcept {
            return handle_;
        }

    private:
        hal::I2cIoHandle handle_{};
    };

    static_assert(I2cBus<HalI2cBus>);
}
