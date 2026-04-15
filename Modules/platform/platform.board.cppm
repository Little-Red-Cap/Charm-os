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

    enum class BootLoadKind : util::u8 {
        copy_to_ram = 0,
        xip
    };

    struct BootLoadResolveRequest {
        BootLoadKind kind{BootLoadKind::copy_to_ram};
        util::u32 storage_payload_offset{0};
        util::u32 storage_entry_offset{0};
        util::u32 entry_offset{0};
        util::u32 payload_size{0};
        util::u32 image_size{0};
        util::u16 image_flags{0};
    };

    struct BootLoadTransferRequest {
        BootLoadKind kind{BootLoadKind::copy_to_ram};
        util::usize payload_base{0};
        util::u32 storage_payload_offset{0};
        util::u32 payload_size{0};
        util::u16 image_flags{0};
    };

    struct BootLoadDesc {
        void* ctx{nullptr};
        util::usize (*resolve_payload_base)(void* ctx,
                                            const BootLoadResolveRequest& request) noexcept {nullptr};
        bool (*load_payload)(void* ctx,
                             const BootLoadTransferRequest& request) noexcept {nullptr};
    };

    struct BootExecRequest {
        util::usize payload_base{0};
        util::usize entry_addr{0};
        util::u32 payload_size{0};
        util::u16 image_flags{0};
    };

    struct BootExecDesc {
        void* ctx{nullptr};
        bool (*prepare_jump)(void* ctx,
                             const BootExecRequest& request) noexcept {nullptr};
        bool (*jump)(void* ctx,
                     const BootExecRequest& request) noexcept {nullptr};
    };

    struct BootBoardCaps {
        BootLoadDesc load{};
        BootExecDesc exec{};
    };

    struct BoardCaps {
        UartDesc uart1{};
        ClockDesc clock{};
        const char* console_cap{"io.console0"};
        InputDesc input{};
        SpiDesc spi1{};
        I2cDesc i2c1{};
        CanDesc can0{};
        SdmmcDesc sdmmc0{};
        SpiFlashDesc flash0{};
        BootBoardCaps boot{};
    };

    constexpr BoardCaps with_boot_caps(BoardCaps caps,
                                       const BootBoardCaps& boot) noexcept {
        caps.boot = boot;
        return caps;
    }

    struct ConsoleCaps {
        UartDesc uart{};
        ClockDesc clock{};
        const char* console_cap{"io.console0"};
    };

    struct InputCaps {
        InputDesc input{};
        ClockDesc clock{};
    };

    struct BlockCaps {
        ClockDesc clock{};
    };
}
