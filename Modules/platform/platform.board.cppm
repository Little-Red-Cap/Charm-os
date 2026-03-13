module;

export module platform.board;

import block.sdmmc;
import block.spi_flash;
import hal_input;
import hal_i2c;
import hal_spi;
import hal_uart;
import io.channel;
import util.core;

export namespace platform::board {
    struct ClockDesc {
        void* ctx{nullptr};
        util::u64 (*now_ms)(void* ctx) noexcept { nullptr };
        util::u64 (*now_us)(void* ctx) noexcept { nullptr };
    };

    struct UartDesc {
        hal::UartIoHandle handle{};
        hal::UartConfig config{};
        const char* io_cap{"io.uart1"};
        const char* hal_cap{"hal.uart1"};
    };

    struct InputDesc {
        const hal::RawInputDriver* driver{nullptr};
        const char* service_cap{"input.service"};
        const char* router_cap{"input.router"};
        const char* pump_cap{"input.pump"};
    };

    struct SpiDesc {
        hal::SpiIoHandle handle{};
        hal::SpiConfig config{};
        const char* hal_cap{"hal.spi1"};
    };

    struct I2cDesc {
        hal::I2cIoHandle handle{};
        hal::I2cConfig config{};
        const char* hal_cap{"hal.i2c1"};
    };

    struct CanDesc {
        io::Channel* channel{nullptr};
        const char* io_cap{"io.can0"};
    };

    struct SdmmcDesc {
        block::SdmmcHandle handle{};
        block::SdmmcConfig config{};
        const char* block_cap{"block.sd0"};
        const char* hal_cap{nullptr};
    };

    struct SpiFlashDesc {
        block::SpiFlashHandle handle{};
        block::SpiFlashConfig config{};
        const char* block_cap{"block.flash0"};
        const char* hal_cap{nullptr};
    };

    struct BoardCaps {
        UartDesc uart1{};
        ClockDesc clock{};
        InputDesc input{};
        SpiDesc spi1{};
        I2cDesc i2c1{};
        CanDesc can0{};
        SdmmcDesc sdmmc0{};
        SpiFlashDesc flash0{};
    };

    struct ConsoleCaps {
        UartDesc uart{};
        ClockDesc clock{};
        const char* console_cap{"io.console0"};
    };
}
