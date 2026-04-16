export module charm.runtime;

// Legacy compatibility facade retained for examples and migration.
// New Modules/* code should prefer subsystem entries such as
// charm.system / charm.io / charm.net.
export import charm.io;
export import charm.net;
export import charm.system;

export import io.channel.adapters;
export import driver.usart_channel;

export import block.cache;
export import block.device;
export import block.file;
export import block.registry;
export import block.sdmmc;
export import block.spi_flash;

export import fs_block;
export import fs_block_file;
export import fs_blockfs;
export import fs_core;
export import fs_errno;
#if CHARM_ENABLE_FATFS
export import fs_fatfs;
#endif
export import fs_mal;
export import fs_mal_block;
export import fs_mal_cache;
export import fs_mal_file;
export import fs_path;
export import fs_ramfs;
export import fs_stream;
export import fs_vfs;

export import hal_core;
export import hal_gpio;
export import hal_i2c;
export import hal_input;
export import hal_irq;
export import hal_spi;
export import hal_stm32_stub;
export import hal_timer;
export import hal_uart;
#if CHARM_TARGET_HAS_WIN32
export import hal_win;
#endif

export import out.ansi;
export import out.api;
export import out.channel;
export import out.core;
export import out.domain;
export import out.format;
export import out.logger;
export import out.sink;

export import shell_cmd;
export import shell_core;
export import shell_repl;
export import shell_service;
export import shell_stdio;
export import shell_stream;
export import shell_time;

export import io.proto.modem_xymodem;
