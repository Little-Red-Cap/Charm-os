        void start_playback() {
            if (!fs_ready) {
                set_status(mount_status.empty() ? "Mount not ready" : mount_status.c_str());
                return;
            }
            FixedString<128> status;
            if (!playback.apply_action(PlaybackAction::start, 0, status)) {
                set_status(status.c_str());
                return;
            }
            set_status(status.c_str());
            set_play_button_text(true);
            sync_progress_value(0);
            begin_weekly_listening_session(true);
        }

        void pause_playback() {
            FixedString<128> status;
            if (!playback.apply_action(PlaybackAction::pause, 0, status)) return;
            set_status(status.c_str());
            set_play_button_text(false);
            end_weekly_listening_session();
        }

        void resume_playback() {
            FixedString<128> status;
            if (!playback.apply_action(PlaybackAction::resume, 0, status)) return;
            set_status(status.c_str());
            set_play_button_text(true);
            begin_weekly_listening_session(false);
        }

        void stop_playback() {
            end_weekly_listening_session();
            FixedString<128> status;
            if (playback.apply_action(PlaybackAction::stop, 0, status)) {
                set_status(status.c_str());
            } else {
                set_status("Stopped");
            }
            set_play_button_text(false);
            set_time_label(0);
            sync_progress_value(0);
        }

        void apply_actions(const PendingActions& actions) {
            if (actions.prev) {
                switch_track(-1);
            } else if (actions.next) {
                switch_track(1);
            } else if (actions.select) {
                select_track_index(actions.select_index);
            }

            if (actions.toggle_play) {
                const bool was_playing = playback.playing();
                const bool was_paused = playback.paused();
                FixedString<128> status;
                if (playback.apply_action(PlaybackAction::toggle, 0, status)) {
                    if (!status.empty()) set_status(status.c_str());
                    const bool playing_now = playback.playing();
                    if (was_playing && !playing_now) {
                        end_weekly_listening_session();
                    } else if (!was_playing && playing_now) {
                        begin_weekly_listening_session(!was_paused);
                    }
                    set_play_button_text(playing_now);
                    if (status.view() == "Opening") {
                        sync_progress_value(0);
                    }
                } else if (!status.empty()) { set_status(status.c_str()); }
            }

            if (actions.cycle_mode) {
                cycle_play_mode();
            }

            if (actions.seek) {
                FixedString<128> status;
                if (playback.apply_action(PlaybackAction::seek, actions.seek_sec, status)) {
                    if (!status.empty()) set_status(status.c_str());
                } else if (!status.empty()) { set_status(status.c_str()); }
            }
        }

        void set_track_labels(int idx) {
            if (!storage.track_titles || !storage.track_subtitles) return;
            if (idx < 0 || idx >= static_cast<int>(storage.track_titles->size())) return;
            title_text.assign((*storage.track_titles)[idx].view());
            subtitle_text.assign((*storage.track_subtitles)[idx].view());
            strip_audio_extension(title_text);
            const std::string_view raw = title_text.view();
            const auto sep = raw.find(" - ");
            if (sep != std::string_view::npos) {
                const std::string_view left = raw.substr(0, sep);
                const std::string_view right = raw.substr(sep + 3);
                if (!right.empty()) {
                    title_text.assign(right);
                    if (!left.empty()) {
                        subtitle_text.assign(left);
                    }
                }
            }
            strip_audio_extension(title_text);
            const auto looks_like_format = [&](std::string_view v) noexcept -> bool {
                if (v.empty()) return true;
                if (v == "UNKNOWN") return true;
                if (is_audio_extension(v)) return true;
                if (v.size() > 5) return false;
                for (char ch : v) {
                    if (!std::isalnum(static_cast<unsigned char>(ch))) return false;
                }
                return true;
            };
            if (looks_like_format(subtitle_text.view())) {
                subtitle_text.assign("");
            }
            set_label_slot(handles.title, text_slots.title, title_text.c_str());
            set_label_slot(handles.subtitle, text_slots.subtitle, subtitle_text.c_str());
            set_label_slot(handles.bottom_title, text_slots.bottom_title, title_text.c_str());
            set_label_slot(handles.bottom_subtitle, text_slots.bottom_subtitle, subtitle_text.c_str());
        }
        bool load_track_index(int idx) {
            if (!fs_ready) {
                set_status(mount_status.empty() ? "Mount not ready" : mount_status.c_str());
                return false;
            }
            const auto* tracks = storage.tracks;
            const auto* labels = storage.track_labels;
            if (!tracks || tracks->size() == 0) return false;
            if (!labels) return false;
            if (idx < 0) idx = 0;
            if (idx >= static_cast<int>(tracks->size())) idx = static_cast<int>(tracks->size()) - 1;
            track_index = idx;
            const auto& vfs_path = (*tracks)[track_index];
            const char* track_path = vfs_path.c_str();
            set_track_labels(track_index);
            track_format_text.clear();
            if (const auto fmt = format_from_path(vfs_path.view()); !fmt.empty()) {
                track_format_text.assign(fmt);
            }
            track_size_bytes = query_track_size(track_path);
            last_info_text.clear();
            FixedString<128> status;
            const bool track_ready = player::check_track_ready(vfs_path.view(), status);
            if (!status.empty()) { set_status(status.c_str()); }
            playback.set_track_path(track_path);
            playback.set_track_ready(track_ready);
            track_preloaded = true;
            if (track_ready) {
                cover_embedded_path.assign(vfs_path.view());
                cover_folder_path.clear();
                FixedString<260> folder_path;
                bool has_folder = false;
                if (cover_strategy == CoverStrategy::folder_only
                    || cover_strategy == CoverStrategy::folder_first) {
                    has_folder = fs_utils::find_cover_for_track(vfs_path.view(), folder_path);
                }
                cover_folder_path.assign(folder_path.view());
                cover_ready = true;
                switch (cover_strategy) {
                case CoverStrategy::embedded_only:
                    cover_path.assign(cover_embedded_path.c_str());
                    break;
                case CoverStrategy::folder_only:
                    cover_path.assign(has_folder ? cover_folder_path.c_str() : "");
                    cover_ready = has_folder;
                    break;
                case CoverStrategy::folder_first:
                    cover_path.assign(has_folder ? cover_folder_path.c_str() : cover_embedded_path.c_str());
                    break;
                case CoverStrategy::embedded_first:
                default:
                    cover_path.assign(cover_embedded_path.c_str());
                    break;
                }
            } else {
                cover_ready = false;
                cover_path.clear();
                cover_embedded_path.clear();
                cover_folder_path.clear();
            }
            update_cover_image();
            reset_duration();
            preloaded_duration_sec = 0;
            if (track_ready) {
                int secs = 0;
                if (player::probe_duration_seconds(track_path, secs)) {
                    playback.set_duration_from_probe(secs);
                    preloaded_duration_sec = secs;
                    update_info_label();
#if defined(CHARM_PLAYER_COVER_DEBUG)
                    std::printf("[np] preload duration=%d path=%s\n", secs, track_path);
#endif
                }
            }
            set_play_button_text(false);
            last_time_sec = -1;
            set_time_label(0);
            sync_progress_value(0);
            sync_list_selection();
            return playback.track_ready();
        }

        void switch_track(int delta) {
            if (!fs_ready) {
                set_status(mount_status.empty() ? "Mount not ready" : mount_status.c_str());
                return;
            }
            const auto* tracks = storage.tracks;
            if (!tracks || tracks->size() == 0) return;
            const int count = static_cast<int>(tracks->size());
            int next = track_index + delta;
            if (next < 0) next = count - 1;
            if (next >= count) next = 0;
            const bool was_active = playback.playing() || playback.paused();
            stop_playback();
            load_track_index(next);
            if (was_active && playback.track_ready()) {
                start_playback();
            }
        }

        void select_track_index(int idx) {
            if (!fs_ready) {
                set_status(mount_status.empty() ? "Mount not ready" : mount_status.c_str());
                return;
            }
            const auto* tracks = storage.tracks;
            if (!tracks || tracks->size() == 0) return;
            const bool was_playing = playback.playing();
            const bool was_paused = playback.paused();
            stop_playback();
            load_track_index(idx);
            if (was_playing && playback.track_ready()) {
                start_playback();
            } else if (was_paused && playback.track_ready()) {
                start_playback();
                pause_playback();
            }
        }

        void set_play_mode(int mode) {
            play_mode = mode;
            update_play_mode_label();
        }

        void cycle_play_mode() {
            set_play_mode((play_mode + 1) % 3);
        }

