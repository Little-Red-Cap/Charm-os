        int resolve_next_track() {
            const auto* tracks = storage.tracks;
            if (!tracks || tracks->size() == 0) return -1;
            const int count = static_cast<int>(tracks->size());
            if (play_mode == 1) {
                return track_index;
            }
            if (play_mode == 2) {
                if (count <= 1) return track_index;
                int next = track_index;
                for (int i = 0; i < 4 && next == track_index; ++i) {
                    next = rand_index(count);
                }
                if (next == track_index) {
                    next = (track_index + 1) % count;
                }
                return next;
            }
            int next = track_index + 1;
            if (next >= count) next = 0;
            return next;
        }

        void handle_track_end() {
            const auto* tracks = storage.tracks;
            if (!fs_ready || !tracks || tracks->size() == 0) {
                stop_playback();
                return;
            }
            const int next = resolve_next_track();
            if (next < 0) {
                stop_playback();
                return;
            }
            stop_playback();
            if (load_track_index(next)) {
                start_playback();
            }
        }

        void update_duration_from_player() {
            if (!playback.update_duration_from_player()) return;
            const int dur = playback.duration_sec();
            if (dur > 0 && preloaded_duration_sec == 0) {
                preloaded_duration_sec = dur;
            }
            if (progress_dragging) return;
            const int cur = playback.current_sec();
            if (cur > dur) {
                set_time_label(dur);
                sync_progress_value(100);
            } else {
                set_time_label(cur);
            }
            update_info_label();
        }

        bool update_progress() {
            if (!access.valid() || progress_dragging) return false;
            auto upd = playback.update_progress();
            if (!upd.updated) return false;
            access.set_value(handles.progress, upd.value);
            set_time_label(upd.current_sec);
            return true;
        }

