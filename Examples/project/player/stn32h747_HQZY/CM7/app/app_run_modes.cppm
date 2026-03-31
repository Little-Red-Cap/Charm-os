module;

#include <cstdint>

export module player.stm32h7.app_run_modes;

import fs_demo;
import out.api;
import out.channel;
import player.stm32h7.audio_mp3_demo;
import player.stm32h7.app_boot_fs;
import player.stm32h7.board_keys;
import player.stm32h7.board_sdram;
import player.stm32h7.fs_demo;
import player.stm32h7.ink_demo;
import util.core;

export namespace player::stm32h7::app::run_modes {
    enum class Mode : std::uint8_t {
        sd = 0,
        decode = 1,
        i2s = 2,
        full = 3
    };

    struct Config {
        bool enable_audio{true};
        bool enable_display{false};
        bool enable_usb_audio{true};
        bool use_usb_audio_on_boot{true};
        bool bringup_wait_key{true};
        bool debug_dump_root{false};
        bool debug_stop_after_fs{false};
        bool use_out_logger_early{false};
        bool sdram_selftest_on_boot{true};
        bool sdram_selftest_in_bringup{false};
    };

    struct Context {
        Config cfg{};
        bool display_ready{false};
    };

    using PrintFn = void (*)(const char*) noexcept;

    inline void wait_key_press(bool enable) noexcept {
        if (!enable) return;
        player::stm32h7::board::wait_for_boot_key();
    }

    inline const char* name(Mode mode) noexcept {
        switch (mode) {
        case Mode::sd: return "sd";
        case Mode::decode: return "decode";
        case Mode::i2s: return "i2s";
        case Mode::full: return "full";
        }
        return "unknown";
    }

    inline Mode select_mode(bool bringup_key_select,
                            bool key0,
                            bool wkup2,
                            Mode default_mode) noexcept {
        if (!bringup_key_select) return default_mode;
        if (key0 && wkup2) return Mode::i2s;
        if (key0) return Mode::sd;
        if (wkup2) return Mode::decode;
        return default_mode;
    }

    inline void run_display_demo(out::channel_sink& console_sink) noexcept {
        display_st7305_selftest();
        if (ink_demo_render_once()) {
            (void)out::try_println<"display: ink demo done">(console_sink);
        } else {
            (void)out::try_println<"display: ink demo failed">(console_sink);
        }
        ink_demo_run();
    }

    inline void run_audio_demo(out::channel_sink& console_sink,
                               const Config& cfg) noexcept {
        if (cfg.enable_usb_audio && cfg.use_usb_audio_on_boot) {
            (void)out::try_println<"boot: usb audio wait key">(console_sink);
            wait_key_press(cfg.bringup_wait_key);
            audio_usb_stream_run();
            return;
        }
        (void)out::try_println<"boot: wait key">(console_sink);
        wait_key_press(cfg.bringup_wait_key);
        audio_mp3_demo_run();
    }

    inline bool run(Context& ctx,
                    Mode mode,
                    out::channel_sink& console_sink,
                    PrintFn early_print) noexcept {
        const auto& cfg = ctx.cfg;
        if (mode == Mode::sd) {
            if (cfg.use_out_logger_early) {
                (void)out::try_println<"bringup: sd selftest begin">(console_sink);
            }
            (void)fs_sd_selftest(0, 128, 1);
            return true;
        }
        if (mode == Mode::decode) {
            if (early_print) early_print("boot: fs init begin\n");
            if (!fs_boot_init()) {
                if (cfg.use_out_logger_early) {
                    (void)out::try_println<"fs init failed">(console_sink);
                }
                return false;
            }
            if (early_print) early_print("boot: fs init ok\n");
            if (cfg.debug_dump_root) {
                boot_fs::dump_dir_early("/");
                boot_fs::dump_dir_early("/SDNAND~1");
            }
            if (cfg.debug_stop_after_fs) {
                while (true) {}
            }
            if (cfg.enable_audio) {
                if (cfg.use_out_logger_early) {
                    (void)out::try_println<"bringup: wait key">(console_sink);
                }
                wait_key_press(cfg.bringup_wait_key);
                if (cfg.use_out_logger_early) {
                    (void)out::try_println<"bringup: decode selftest begin">(console_sink);
                }
                audio_decode_selftest();
            } else if (early_print) {
                early_print("boot: audio disabled\n");
            }
            return true;
        }
        if (mode == Mode::i2s) {
            if (cfg.enable_audio) {
                if (cfg.use_out_logger_early) {
                    (void)out::try_println<"bringup: i2s selftest begin">(console_sink);
                }
                wait_key_press(cfg.bringup_wait_key);
                audio_i2s_selftest(2000);
            } else if (early_print) {
                early_print("boot: audio disabled\n");
            }
            return true;
        }

        if (cfg.sdram_selftest_on_boot && cfg.sdram_selftest_in_bringup) {
            if (cfg.use_out_logger_early) {
                (void)out::try_println<"bringup: sdram selftest begin">(console_sink);
            }
            (void)player::stm32h7::board::sdram_selftest(console_sink);
        } else if (cfg.sdram_selftest_on_boot && !cfg.sdram_selftest_in_bringup) {
            if (early_print) early_print("boot: sdram selftest skip\n");
        }
        if (early_print) early_print("boot: fs init begin\n");
        if (!fs_boot_init()) {
            (void)out::try_println<"fs init failed">(console_sink);
            return false;
        }
        if (early_print) early_print("boot: fs init ok\n");
        if (cfg.debug_dump_root) {
            boot_fs::dump_dir_early("/");
            boot_fs::dump_dir_early("/SDNAND~1");
        }
        if (cfg.debug_stop_after_fs) {
            while (true) {}
        }
        if (cfg.enable_display && ctx.display_ready) {
            run_display_demo(console_sink);
        } else if (early_print) {
            early_print("boot: display disabled\n");
        }
        if (cfg.enable_audio) {
            run_audio_demo(console_sink, cfg);
        } else if (early_print) {
            early_print("boot: audio disabled\n");
        }
        return true;
    }
}
