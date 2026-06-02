        static const char* lyrics_source_label(LyricsSourceKind source) noexcept {
            switch (source) {
            case LyricsSourceKind::Sidecar: return "sidecar";
            case LyricsSourceKind::LyricsDir: return "Lyrics";
            case LyricsSourceKind::EmbeddedFlac: return "FLAC";
            case LyricsSourceKind::EmbeddedMp3: return "MP3";
            case LyricsSourceKind::UnsupportedEmbedded: return "unsupported";
            case LyricsSourceKind::None:
            default:
                return "none";
            }
        }

        static const char* lyrics_status_label(LyricsLoadStatus status) noexcept {
            switch (status) {
            case LyricsLoadStatus::Loaded: return "Lyrics";
            case LyricsLoadStatus::Truncated: return "Lyrics truncated";
            case LyricsLoadStatus::Unsupported: return "Embedded lyrics unsupported";
            case LyricsLoadStatus::ParseError: return "Lyrics parse failed";
            case LyricsLoadStatus::IoError: return "Lyrics read failed";
            case LyricsLoadStatus::Missing:
            default:
                return "No lyrics";
            }
        }

        void set_lyrics_text(WidgetHandle handle,
                             ::ui::scene::TextSlotId slot,
                             FixedString<product_config::lyrics_line_text_capacity>& cache,
                             const char* value) {
            const char* text = value ? value : "";
            if (cache.view() == text) return;
            cache.assign(text);
            set_label_slot(handle, slot, cache.c_str());
        }

        void set_lyrics_status_text(const char* value) {
            const char* text = value ? value : "";
            if (lyrics_status_text.view() == text) return;
            lyrics_status_text.assign(text);
            set_label_slot(handles.now_lyrics_status, text_slots.lyrics_status, lyrics_status_text.c_str());
        }

        void sync_lyrics_visibility() noexcept {
            const bool show_lyrics = lyrics_visible;
            const bool has_cover = has_cover_image();
            set_handle_visible(handles.now_lyrics_panel, show_lyrics);
            set_handle_visible(handles.now_cover_plate, !show_lyrics && !has_cover);
            set_handle_visible(handles.cover, !show_lyrics && has_cover);
        }

        void update_lyrics_status_label() {
            char buf[96]{};
            if (lyrics_result.status == LyricsLoadStatus::Loaded ||
                lyrics_result.status == LyricsLoadStatus::Truncated) {
                std::snprintf(buf,
                              sizeof(buf),
                              "%s · %s · %d line%s",
                              lyrics_status_label(lyrics_result.status),
                              lyrics_source_label(lyrics_result.source),
                              lyrics_result.line_count,
                              lyrics_result.line_count == 1 ? "" : "s");
            } else {
                std::snprintf(buf,
                              sizeof(buf),
                              "%s",
                              lyrics_status_label(lyrics_result.status));
            }
            set_lyrics_status_text(buf);
        }

        void clear_current_lyrics() noexcept {
            lyrics_result = LyricsLoadResult{};
            last_lyrics_position_ms = -1;
            last_lyrics_index = -2;
            lyrics_prev_text.clear();
            lyrics_current_text.clear();
            lyrics_next_text.clear();
            lyrics_status_text.clear();
            if (!access.valid()) return;
            set_label_slot(handles.now_lyrics_prev, text_slots.lyrics_prev, "");
            set_label_slot(handles.now_lyrics_current, text_slots.lyrics_current, "No lyrics");
            set_label_slot(handles.now_lyrics_next, text_slots.lyrics_next, "");
            set_label_slot(handles.now_lyrics_status, text_slots.lyrics_status, "");
        }

        void load_current_track_lyrics(std::string_view path) {
            lyrics_result = load_lyrics_for_track(path);
            last_lyrics_position_ms = -1;
            last_lyrics_index = -2;
            refresh_lyrics_window(true);
        }

        void refresh_lyrics_window(bool force = false) {
            if (!access.valid()) return;
            const int position_ms = std::max(0, playback.current_sec()) * 1000;
            const LyricsWindow window = resolve_lyrics_window(position_ms);
            if (!force && position_ms == last_lyrics_position_ms &&
                window.current_index == last_lyrics_index) {
                return;
            }
            last_lyrics_position_ms = position_ms;
            last_lyrics_index = window.current_index;

            if (!window.available) {
                set_lyrics_text(handles.now_lyrics_prev, text_slots.lyrics_prev, lyrics_prev_text, "");
                set_lyrics_text(handles.now_lyrics_current,
                                text_slots.lyrics_current,
                                lyrics_current_text,
                                lyrics_status_label(lyrics_result.status));
                set_lyrics_text(handles.now_lyrics_next, text_slots.lyrics_next, lyrics_next_text, "");
                update_lyrics_status_label();
                return;
            }

            set_lyrics_text(handles.now_lyrics_prev,
                            text_slots.lyrics_prev,
                            lyrics_prev_text,
                            window.previous);
            set_lyrics_text(handles.now_lyrics_current,
                            text_slots.lyrics_current,
                            lyrics_current_text,
                            window.current);
            set_lyrics_text(handles.now_lyrics_next,
                            text_slots.lyrics_next,
                            lyrics_next_text,
                            window.next);
            update_lyrics_status_label();
        }

        void toggle_lyrics_panel() {
            lyrics_visible = !lyrics_visible;
            sync_lyrics_visibility();
            refresh_lyrics_window(true);
        }
