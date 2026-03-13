export module player.hqzy.controller;

import player.hqzy.app_state;
import player.hqzy.fs_utils;

export namespace player::hqzy {
    constexpr unsigned long kFrameMs = 80;

    struct Controller {
        AppState* state{nullptr};
        bool key0_prev{false};
        bool wkup2_prev{false};
        bool enc_key_prev{false};
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
                if (state->page == Page::files) {
                    (void)open_selected(*state);
                } else {
                    if (!state->playing) {
                        if (state->play_path[0] != '\0') {
                            state->play_request = true;
                            state->stop_request = false;
                            state->playing = true;
                            state->paused = false;
                        }
                    } else {
                        state->stop_request = true;
                        state->playing = false;
                        state->paused = true;
                    }
                }
            }
            if (wkup2 && !wkup2_prev) {
                const auto next = static_cast<unsigned char>((static_cast<unsigned char>(state->page) + 1u) % 4u);
                state->page = static_cast<Page>(next);
            }
            key0_prev = key0;
            wkup2_prev = wkup2;
        }

        void on_encoder(int steps, bool enc_key) noexcept {
            if (!state) return;
            if (enc_key && !enc_key_prev) {
                if (state->page == Page::files) {
                    (void)open_selected(*state);
                } else {
                    if (!state->playing) {
                        if (state->play_path[0] != '\0') {
                            state->play_request = true;
                            state->stop_request = false;
                            state->playing = true;
                            state->paused = false;
                        }
                    } else {
                        state->stop_request = true;
                        state->playing = false;
                        state->paused = true;
                    }
                }
            }
            if (steps != 0) {
                if (state->page == Page::files) {
                    if (state->entry_count > 0) {
                        int sel = static_cast<int>(state->entry_selected) + steps;
                        if (sel < 0) sel = 0;
                        const int max_sel = static_cast<int>(state->entry_count) - 1;
                        if (sel > max_sel) sel = max_sel;
                        state->entry_selected = static_cast<unsigned char>(sel);
                    }
                } else {
                    int vol = static_cast<int>(state->volume) + steps;
                    if (vol < 0) vol = 0;
                    if (vol > 100) vol = 100;
                    state->volume = static_cast<unsigned char>(vol);
                }
            }
            enc_key_prev = enc_key;
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
        }
    };
}
