#include <array>
#include <cstdio>
#include <span>

import driver.i2c_register_device;
import hal_core;
import hal_i2c;
import io.device_i2c;
import io.device_i2c_hal;
import util.core;
import util.error;

namespace {
    struct FakeHalI2c {
        util::usize write_count{0};
        util::usize read_count{0};
        util::usize write_read_count{0};
        util::u16 last_addr{0};
        std::array<util::u8, 4> last_tx{};
        util::usize last_tx_size{0};
        hal::Status next_write_status{hal::Status::ok};
        hal::Status next_read_status{hal::Status::ok};
        hal::Status next_write_read_status{hal::Status::ok};

        static hal::Result write(void* ctx,
                                 util::u16 addr,
                                 std::span<const util::u8> data) noexcept {
            auto* self = static_cast<FakeHalI2c*>(ctx);
            if (!self) {
                return hal::err(hal::Status::error);
            }
            self->last_addr = addr;
            self->last_tx_size = data.size();
            for (util::usize i = 0; i < data.size() && i < self->last_tx.size(); ++i) {
                self->last_tx[i] = data[i];
            }
            ++self->write_count;
            if (self->next_write_status != hal::Status::ok) {
                return hal::err(self->next_write_status);
            }
            return hal::ok();
        }

        static hal::Result read(void* ctx,
                                util::u16 addr,
                                std::span<util::u8> data) noexcept {
            auto* self = static_cast<FakeHalI2c*>(ctx);
            if (!self) {
                return hal::err(hal::Status::error);
            }
            self->last_addr = addr;
            ++self->read_count;
            if (self->next_read_status != hal::Status::ok) {
                return hal::err(self->next_read_status);
            }
            if (!data.empty()) {
                data[0] = 0x44;
            }
            return hal::ok();
        }

        static hal::Result write_read(void* ctx,
                                      util::u16 addr,
                                      std::span<const util::u8> tx,
                                      std::span<util::u8> rx) noexcept {
            auto* self = static_cast<FakeHalI2c*>(ctx);
            if (!self) {
                return hal::err(hal::Status::error);
            }
            self->last_addr = addr;
            self->last_tx_size = tx.size();
            for (util::usize i = 0; i < tx.size() && i < self->last_tx.size(); ++i) {
                self->last_tx[i] = tx[i];
            }
            ++self->write_read_count;
            if (self->next_write_read_status != hal::Status::ok) {
                return hal::err(self->next_write_read_status);
            }
            if (!rx.empty()) {
                rx[0] = 0x33;
            }
            return hal::ok();
        }
    };

    constexpr hal::I2cOps kFakeOps{
        .init = nullptr,
        .enable = nullptr,
        .disable = nullptr,
        .write = &FakeHalI2c::write,
        .read = &FakeHalI2c::read,
        .write_read = &FakeHalI2c::write_read,
    };

    bool expect(bool condition, const char* message) noexcept {
        if (!condition) {
            std::fprintf(stderr, "[ERR] %s\n", message);
            return false;
        }
        return true;
    }
}

int main() {
    constexpr io::device::I2cAddress kAddress = 0x18;

    FakeHalI2c fake{};
    io::device::HalI2cBus bus{hal::I2cIoHandle{&fake, &kFakeOps}};
    auto dev = io::device::make_i2c_device_ref(io::device::make_i2c_bus_ref(bus), kAddress);
    driver::i2c::RegisterDevice8 regs{dev};

    auto id = regs.read_u8(0x0F);
    if (!expect(id && id.value() == 0x33, "register read via hal adapter mismatch")) return 1;
    if (!expect(fake.write_read_count == 1, "fake hal write_read not called")) return 2;
    if (!expect(fake.last_addr == kAddress, "fake hal address mismatch")) return 3;
    if (!expect(fake.last_tx_size == 1 && fake.last_tx[0] == 0x0F, "fake hal tx mismatch")) return 4;

    auto write = regs.write_u8(0x20, 0x57);
    if (!expect(static_cast<bool>(write), "register write via hal adapter failed")) return 5;
    if (!expect(fake.write_count == 1, "fake hal write not called")) return 6;
    if (!expect(fake.last_tx_size == 2 && fake.last_tx[0] == 0x20 && fake.last_tx[1] == 0x57,
                "fake hal write tx mismatch")) return 7;

    fake.next_write_read_status = hal::Status::timeout;
    auto timeout = regs.read_u8(0x0F);
    if (!expect(!timeout && timeout.error() == util::Errc::timeout, "timeout status mapping mismatch")) return 8;

    io::device::HalI2cBus unsupported_bus{hal::I2cIoHandle{&fake, nullptr}};
    auto unsupported_ref = io::device::make_i2c_bus_ref(unsupported_bus);
    std::array<util::u8, 1> rx{};
    auto unsupported = unsupported_ref.read(kAddress, rx);
    if (!expect(!unsupported && unsupported.error() == util::Errc::not_supported,
                "unsupported status mapping mismatch")) return 9;

    fake.next_write_status = hal::Status::busy;
    auto busy = regs.write_u8(0x20, 0x57);
    if (!expect(!busy && busy.error() == util::Errc::busy, "busy status mapping mismatch")) return 10;

    std::puts("i2c hal adapter smoke: ok");
    std::printf("write=%zu read=%zu write_read=%zu\n",
                fake.write_count,
                fake.read_count,
                fake.write_read_count);
    return 0;
}
