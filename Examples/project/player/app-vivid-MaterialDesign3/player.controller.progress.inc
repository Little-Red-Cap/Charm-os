        int resolve_next_track() {
            if (available_track_count() <= 0) return -1;
            if (!ensure_playback_queue()) return -1;
            return playback.resolve_track_for_playback_mode();
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
            load_track_with_resume_mode(next, PlaybackResumeMode::Playing);
        }

        void update_duration_from_player() {
            if (!playback.update_duration_from_player()) return;
            const int dur = playback.duration_sec();
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

