#include <array>
#include <cstddef>
#include <cstdio>

import device.types;
import io.channel;
import io.reactor;
import io.registry;
import usb.host.runtime_channel;
import usb.host.runtime_manager;
import util.core;

#include "../support/usb_host_runtime_channel_support.hpp"

namespace {
    using examples::usb::support::CdcRuntimeHarness;

    bool expect(bool cond, const char* message) {
        if (!cond) {
            std::fprintf(stderr, "[ERR] %s\n", message);
            return false;
        }
        return true;
    }

    bool expect_error(const io::result& r, io::errc want, const char* message) {
        if (r.error() != want) {
            std::fprintf(stderr,
                         "[ERR] %s err=%d want=%d\n",
                         message,
                         static_cast<int>(r.error()),
                         static_cast<int>(want));
            return false;
        }
        return true;
    }
}

int main() {
    constexpr auto kCapName = "io.usb0";

    io::Registry<4> io_registry{};
    io_registry.init();
    usb::host::RuntimeManager<4, 4> runtime{"usb.host.cdc"};

    CdcRuntimeHarness<io::Registry<4>> cdc{
        io_registry,
        kCapName,
        0x1209,
        0x0006
    };

    auto add_r = cdc.add_to(runtime);
    if (!add_r) {
        std::fprintf(stderr,
                     "[ERR] runtime manager add_exported failed err=%d\n",
                     static_cast<int>(add_r.error()));
        return 1;
    }

    auto* stable = cdc.stable();
    if (!expect(stable == &cdc.exported_slot().channel(),
                "registry did not expose the stable channel slot")) {
        return 1;
    }
    if (!expect(cdc.exported(), "slot export should stay published")) return 1;
    if (!expect(!cdc.attached(), "slot should start detached")) return 1;
    if (!expect(cdc.generation() == 0, "slot generation should start at 0")) return 1;

    std::array<util::u8, 4> read_buf{};
    std::array<util::u8, 4> write_buf{
        static_cast<util::u8>('P'),
        static_cast<util::u8>('I'),
        static_cast<util::u8>('N'),
        static_cast<util::u8>('G')
    };

    if (!expect_error(stable->read(read_buf), io::errc::noent, "detached slot should read as noent")) return 1;
    if (!expect_error(stable->write(write_buf), io::errc::noent, "detached slot should write as noent")) return 1;
    if (!expect_error(stable->flush(), io::errc::noent, "detached slot should flush as noent")) return 1;

    if (!expect(runtime.scan(), "runtime manager scan failed")) {
        return 1;
    }
    if (!expect(cdc.enumerated_in(runtime),
                "runtime manager did not enumerate the CDC device")) {
        return 1;
    }
    if (!expect(runtime.registry().device_count() == 1,
                "runtime registry should contain one CDC device")) {
        return 1;
    }
    if (!expect(cdc.attached(), "runtime init did not attach the CDC slot")) return 1;
    if (!expect(cdc.generation() == 1,
                "slot generation should advance after attach")) {
        return 1;
    }
    if (!expect(runtime.registry().device_at(0).state == device::DeviceState::running,
                "runtime CDC device should be running after match_all")) {
        return 1;
    }

    auto* stable_after_attach = io_registry.open_channel(io::cap_id(kCapName));
    if (!expect(stable_after_attach == stable,
                "registry pointer should stay stable after attach")) {
        return 1;
    }

    auto read_r = stable->read(read_buf);
    if (!expect(static_cast<bool>(read_r) && read_r.value() == 2,
                "attached slot should read two bytes")) {
        return 1;
    }
    if (!expect(read_buf[0] == static_cast<util::u8>('O') &&
                read_buf[1] == static_cast<util::u8>('K'),
                "attached slot returned unexpected read payload")) {
        return 1;
    }

    auto write_r = stable->write(write_buf);
    if (!expect(static_cast<bool>(write_r) && write_r.value() == write_buf.size(),
                "attached slot should write the full payload")) {
        return 1;
    }
    if (!expect(cdc.backend.tx_size == write_buf.size(),
                "backend did not observe the expected write size")) {
        return 1;
    }
    if (!expect(cdc.backend.tx_data[0] == static_cast<util::u8>('P') &&
                cdc.backend.tx_data[1] == static_cast<util::u8>('I') &&
                cdc.backend.tx_data[2] == static_cast<util::u8>('N') &&
                cdc.backend.tx_data[3] == static_cast<util::u8>('G'),
                "backend did not observe the expected write payload")) {
        return 1;
    }

    auto flush_r = stable->flush();
    if (!expect(static_cast<bool>(flush_r), "attached slot should flush successfully")) return 1;
    if (!expect(cdc.backend.flushed, "backend flush callback was not observed")) return 1;

    if (!expect(cdc.remove_from(runtime),
                "runtime remove did not detach the CDC slot")) {
        return 1;
    }
    if (!expect(!cdc.attached(), "CDC slot should be detached after remove")) return 1;
    if (!expect(cdc.generation() == 2,
                "slot generation should advance after detach")) {
        return 1;
    }
    if (!expect(io_registry.open_channel(kCapName) == stable,
                "registry pointer should remain stable after detach")) {
        return 1;
    }

    if (!expect_error(stable->read(read_buf), io::errc::noent, "detached slot should read as noent after remove")) return 1;
    if (!expect_error(stable->write(write_buf), io::errc::noent, "detached slot should write as noent after remove")) return 1;
    if (!expect_error(stable->flush(), io::errc::noent, "detached slot should flush as noent after remove")) return 1;

    std::puts("[OK] usb-host-runtime-channel-smoke passed");
    return 0;
}
