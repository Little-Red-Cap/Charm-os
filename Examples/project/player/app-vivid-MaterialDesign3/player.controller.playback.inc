        template <typename Fn>
        bool dispatch_playback_action(PlaybackAction action, int value, Fn&& fn) {
            FixedString<128> status;
            const bool ok = playback.apply_action(action, value, status);
            fn(ok, status);
            return ok;
        }

        void start_playback() {
            if (!ensure_playback_fs_ready()) return;
            (void)dispatch_playback_action(PlaybackAction::start, 0, [&](bool ok, const auto& status) {
                set_status(status.c_str());
                if (!ok) return;
                sync_active_playback_ui(true, true);
            });
        }

        void pause_playback() {
            (void)dispatch_playback_action(PlaybackAction::pause, 0, [&](bool ok, const auto& status) {
                if (!ok) return;
                set_status(status.c_str());
                sync_inactive_playback_ui();
            });
        }

        void resume_playback() {
            (void)dispatch_playback_action(PlaybackAction::resume, 0, [&](bool ok, const auto& status) {
                if (!ok) return;
                set_status(status.c_str());
                sync_active_playback_ui(false);
            });
        }

        void stop_playback() {
            (void)dispatch_playback_action(PlaybackAction::stop, 0, [&](bool ok, const auto& status) {
                set_status(ok ? status.c_str() : "Stopped");
                sync_inactive_playback_ui(true);
            });
        }

        enum class TrackResumeMode : std::uint8_t {
            Stopped,
            Playing,
            Paused,
        };

        TrackResumeMode current_track_resume_mode() const noexcept {
            if (is_playing()) return TrackResumeMode::Playing;
            if (is_paused()) return TrackResumeMode::Paused;
            return TrackResumeMode::Stopped;
        }

        void restore_track_resume_mode(TrackResumeMode mode) {
            if (!playback.track_ready()) return;
            if (mode == TrackResumeMode::Stopped) return;
            start_playback();
            if (mode == TrackResumeMode::Paused) {
                pause_playback();
            }
        }

        bool load_track_with_resume_mode(int idx, TrackResumeMode mode) {
            stop_playback();
            if (!load_track_index(idx)) return false;
            restore_track_resume_mode(mode);
            return playback.track_ready();
        }

        void apply_primary_pending_action(const PendingActions& actions) {
            if (actions.prev) {
                switch_track(-1);
            } else if (actions.next) {
                switch_track(1);
            } else if (actions.list_action) {
                invoke_list_action(actions.list_action_index);
            } else if (actions.select) {
                activate_list_index(actions.select_index);
            }
        }

        void apply_toggle_play_pending_action() {
            const auto previous_mode = current_track_resume_mode();
            (void)dispatch_playback_action(PlaybackAction::toggle, 0, [&](bool ok, const auto& status) {
                set_status_if_present(status);
                if (!ok) return;
                const auto next_mode = current_track_resume_mode();
                if (next_mode == TrackResumeMode::Playing) {
                    sync_active_playback_ui(previous_mode == TrackResumeMode::Stopped,
                                            status.view() == "Opening");
                } else {
                    sync_inactive_playback_ui();
                }
            });
        }

        void apply_seek_pending_action(int seek_sec) {
            (void)dispatch_playback_action(PlaybackAction::seek, seek_sec, [&](bool, const auto& status) {
                set_status_if_present(status);
            });
            refresh_lyrics_window(true);
        }

        void apply_actions(const PendingActions& actions) {
            apply_primary_pending_action(actions);

            if (actions.toggle_play) {
                apply_toggle_play_pending_action();
            }

            if (actions.cycle_mode) {
                cycle_play_mode();
            }

            if (actions.seek) {
                apply_seek_pending_action(actions.seek_sec);
            }
        }

        void set_track_labels(int idx) {
            if (!storage.track_titles || !storage.track_subtitles) return;
            if (idx < 0 || idx >= static_cast<int>(storage.track_titles->size())) return;
            title_text.assign((*storage.track_titles)[idx].view());
            subtitle_text.assign((*storage.track_subtitles)[idx].view());
            strip_audio_extension(title_text);
            title_text.assign(library_trim_space(title_text.view()));
            subtitle_text.assign(library_trim_space(subtitle_text.view()));
            const std::string_view raw = title_text.view();
            std::string_view parsed_title{};
            std::string_view parsed_artist{};
            if (library_split_track_stem(raw, parsed_title, parsed_artist)) {
                title_text.assign(parsed_title);
                subtitle_text.assign(parsed_artist);
            }
            strip_audio_extension(title_text);
            title_text.assign(library_trim_space(title_text.view()));
            subtitle_text.assign(library_trim_space(subtitle_text.view()));
            if (library_looks_like_format(subtitle_text.view())) {
                subtitle_text.assign("");
            }
            set_label_slot(handles.title, text_slots.title, title_text.c_str());
            set_label_slot(handles.subtitle, text_slots.subtitle, subtitle_text.c_str());
            set_label_slot(handles.bottom_title, text_slots.bottom_title, title_text.c_str());
            set_label_slot(handles.bottom_subtitle, text_slots.bottom_subtitle, subtitle_text.c_str());
#if CHARM_PLAYER_LAYERED_TRANSITIONS
            set_label(handles.transition_title, title_text.c_str());
            set_label(handles.transition_subtitle, subtitle_text.c_str());
#endif
        }

        void update_track_cover_source(std::string_view vfs_path, bool track_ready) {
            cover_failed_embedded_path.clear();
            cover_failed_folder_path.clear();
            if (!covers_enabled()) {
                cover_ready = false;
                cover_path.clear();
                cover_embedded_path.clear();
                cover_folder_path.clear();
                update_cover_image();
                return;
            }
            if (track_ready) {
                cover_embedded_path.assign(vfs_path);
                cover_folder_path.clear();
                FixedString<260> folder_path;
                bool has_folder = false;
                if (cover_strategy == CoverStrategy::folder_only
                    || cover_strategy == CoverStrategy::folder_first) {
                    has_folder = fs_utils::find_cover_for_track(vfs_path, folder_path);
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
        }

        bool covers_enabled() const noexcept {
#if defined(CHARM_PLAYER_COVER_DECODE) && CHARM_PLAYER_COVER_DECODE
            return true;
#else
            return cover_strategy != CoverStrategy::embedded_only;
#endif
        }

        void reset_loaded_track_runtime_state() {
            reset_duration();
            preloaded_duration_sec = 0;
            set_play_button_text(false);
            last_time_sec = -1;
            last_time_total_sec = -1;
            set_time_label(0);
            sync_progress_value(0);
            sync_list_selection();
        }

        void preload_loaded_track_duration(const char* track_path) {
            if (!playback.track_ready() || !track_path || !*track_path) return;
            int secs = 0;
            if (!player::probe_duration_seconds(track_path, secs)) return;
            playback.set_duration_from_probe(secs);
            preloaded_duration_sec = secs;
            if (cache_track_duration(track_index, secs) && current_page == PlayerPage::Library) {
                rebuild_list_display_cache();
                notify_list_text_sources_changed();
            }
            update_info_label();
#if defined(CHARM_PLAYER_COVER_DEBUG)
            std::printf("[np] preload duration=%d path=%s\n", secs, track_path);
#endif
        }

        bool load_track_index(int idx) {
            if (!ensure_playback_fs_ready()) return false;
            const auto* tracks = storage.tracks;
            const auto* labels = storage.track_labels;
            if (!has_available_tracks()) return false;
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
            set_status_if_present(status);
            playback.set_track_path(track_path);
            playback.set_track_ready(track_ready);
            track_preloaded = true;
            update_track_cover_source(vfs_path.view(), track_ready);
            load_current_track_lyrics(vfs_path.view());
            reset_loaded_track_runtime_state();
            preload_loaded_track_duration(track_path);
            return playback.track_ready();
        }

        void switch_track(int delta) {
            if (!ensure_playback_fs_ready()) return;
            if (!has_available_tracks()) return;
            const int count = available_track_count();
            int next = track_index + delta;
            if (next < 0) next = count - 1;
            if (next >= count) next = 0;
            load_track_with_resume_mode(next, has_active_playback() ? TrackResumeMode::Playing
                                                                    : TrackResumeMode::Stopped);
        }

        void select_track_index(int idx) {
            if (!ensure_playback_fs_ready()) return;
            if (!has_available_tracks()) return;
            load_track_with_resume_mode(idx, current_track_resume_mode());
        }

        void set_play_mode(int mode) {
            play_mode = mode;
            update_play_mode_label();
        }

        void cycle_play_mode() {
            set_play_mode((play_mode + 1) % 3);
        }
