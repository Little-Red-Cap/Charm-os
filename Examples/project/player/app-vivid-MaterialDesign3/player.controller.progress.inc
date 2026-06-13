        int resolve_next_track() {
            const int count = available_track_count();
            if (count <= 0) return -1;
            if (play_mode == 1) {
                return track_index;
            }
            if (play_mode == 2) {
                return playback_queue_random_track();
            }
            return playback_queue_track_for_delta(1);
        }

        void handle_track_end() {
            if (!fs_ready || !has_available_tracks()) {
                stop_playback();
                return;
            }
            const int next = resolve_next_track();
            if (next < 0) {
                stop_playback();
                return;
            }
            load_track_with_resume_mode(next, TrackResumeMode::Playing);
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
                sync_progress_idle(dur);
                sync_progress_value(100);
            } else {
                sync_progress_idle(cur);
            }
        }

        bool update_progress() {
            if (!access.valid() || progress_dragging) return false;
            auto upd = playback.update_progress();
            if (!upd.updated) return false;
            sync_progress_idle(upd.current_sec);
            return true;
        }

