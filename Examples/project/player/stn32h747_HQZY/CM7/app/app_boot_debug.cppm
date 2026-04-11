module;

export module player.stm32h7.app_boot_debug;

import out.api;
import out.channel;
import player.stm32h7.app_config;
import player.stm32h7.app_run_modes;

export namespace player::stm32h7::app::boot_debug {
    struct Config {
        bool stop_after_bringup{false};
        bool stop_after_channel{false};
        bool stop_after_fs{false};
        bool dump_root{false};
        bool use_out_logger_early{false};
    };

    inline constexpr Config default_config() noexcept {
        return Config{
            .stop_after_bringup = config::kApp.debug.stop_after_bringup,
            .stop_after_channel = config::kApp.debug.stop_after_channel,
            .stop_after_fs = config::kApp.debug.stop_after_fs,
            .dump_root = config::kApp.debug.dump_root,
            .use_out_logger_early = config::kBoard.bringup.use_out_logger_early
        };
    }

    inline void stop_if(bool enable) noexcept {
        if (!enable) return;
        while (true) {}
    }

    inline void log_mode(out::channel_sink& sink,
                         run_modes::Mode mode,
                         const Config& cfg) noexcept {
        if (!cfg.use_out_logger_early) return;
        (void)out::try_println<"bringup: mode {}">(sink, run_modes::name(mode));
    }
}
