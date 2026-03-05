export module player.hqzy.controller;

import player.hqzy.app_state;
import player.hqzy.fs_utils;

export namespace player::hqzy {
    constexpr unsigned long kFrameMs = 80;

    struct Controller {
        AppState* state{nullptr};
        bool key0_prev{false};
        bool wkup2_prev{false};
        bool list_scanned{false};

        void attach(AppState& s) noexcept { state = &s; }

        void scan_files() noexcept {
            if (!state || list_scanned) return;
            auto st = scan_dir(*state, "/MUSIC");
            if (!static_cast<bool>(st) || state->entry_count == 0) {
                (void)scan_dir(*state, "/");
            }
            list_scanned = true;
        }

        void on_keys(bool key0, bool wkup2) noexcept {
            if (!state) return;
            if (key0 && !key0_prev) {
                state->playing = !state->playing;
                state->paused = !state->playing;
            }
            if (wkup2 && !wkup2_prev) {
                const auto next = static_cast<unsigned char>((static_cast<unsigned char>(state->page) + 1u) % 4u);
                state->page = static_cast<Page>(next);
            }
            key0_prev = key0;
            wkup2_prev = wkup2;
        }

        void tick(unsigned long now_ms) noexcept {
            if (!state) return;
            if (!list_scanned) scan_files();
            state->uptime_ms = now_ms;
            state->frames += 1;
            if (state->playing) {
                state->progress = static_cast<unsigned char>((state->progress + 1u) % 101u);
            }
            state->buffer = static_cast<unsigned char>(60u + (state->progress / 4u));
            state->volume = static_cast<unsigned char>((state->volume + 1u) % 101u);
        }
    };
}
