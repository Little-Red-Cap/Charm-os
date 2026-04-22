        void refresh_library() {
            if (handles.nav_bar) {
                access.set_visible(handles.nav_bar, true);
            }
            if (handles.bottom_bar) {
                access.set_visible(handles.bottom_bar, true);
            }
            update_nav_page_indicator();
            update_library_state_visuals();
            update_list_title();
            update_list_path();
            update_list_sort_label();
            update_list_shuffle_label();
            update_list_placeholder();
            tick_list_duration_probe();
            update_debug_overlay();
        }

        static int compare_ci(std::string_view a, std::string_view b) noexcept {
            const std::size_t n = std::min(a.size(), b.size());
            for (std::size_t i = 0; i < n; ++i) {
                const char ca = static_cast<char>(std::tolower(static_cast<unsigned char>(a[i])));
                const char cb = static_cast<char>(std::tolower(static_cast<unsigned char>(b[i])));
                if (ca < cb) return -1;
                if (ca > cb) return 1;
            }
            if (a.size() < b.size()) return -1;
            if (a.size() > b.size()) return 1;
            return 0;
        }

        static constexpr int kLibraryTabRadius = 20;
        static constexpr int kLibrarySortRadius = 20;
        static constexpr int kLibraryPathRadius = 18;
        static constexpr int kLibraryListRowHeight = 72;
        static constexpr int kTrackDurationUnknown = -1;
        static constexpr int kTrackDurationUnavailable = -2;
        static constexpr std::uint64_t kListDurationProbeIntervalMs = 120;

        struct LibrarySortKey {
            std::string_view primary{};
            std::string_view secondary{};
            std::string_view tertiary{};
        };

        static std::string_view library_file_name(std::string_view path) noexcept {
            const auto slash = path.find_last_of("/\\");
            return (slash == std::string_view::npos) ? path : path.substr(slash + 1);
        }

        static std::string_view library_track_stem(std::string_view path) noexcept {
            std::string_view name = library_file_name(path);
            const auto dot = name.find_last_of('.');
            if (dot == std::string_view::npos) return name;
            const auto ext = name.substr(dot + 1);
            return is_audio_extension(ext) ? name.substr(0, dot) : name;
        }

        static std::string_view library_parent_folder(std::string_view path) noexcept {
            const auto slash = path.find_last_of("/\\");
            if (slash == std::string_view::npos) return {};
            std::string_view parent = path.substr(0, slash);
            const auto prev = parent.find_last_of("/\\");
            return (prev == std::string_view::npos) ? parent : parent.substr(prev + 1);
        }

        static std::string_view library_trim_space(std::string_view value) noexcept {
            while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front()))) {
                value.remove_prefix(1);
            }
            while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back()))) {
                value.remove_suffix(1);
            }
            return value;
        }

        static bool library_split_track_stem(std::string_view stem,
                                             std::string_view& title_out,
                                             std::string_view& artist_out) noexcept {
            title_out = {};
            artist_out = {};
            const auto assign_split = [&](std::string_view title,
                                          std::string_view artist) noexcept -> bool {
                title = library_trim_space(title);
                artist = library_trim_space(artist);
                if (title.empty() || artist.empty()) return false;
                title_out = title;
                artist_out = artist;
                return true;
            };

            const auto spaced_sep = stem.find(" - ");
            if (spaced_sep != std::string_view::npos) {
                if (assign_split(stem.substr(spaced_sep + 3), stem.substr(0, spaced_sep))) {
                    return true;
                }
            }

            const auto compact_sep = stem.find_last_of('-');
            if (compact_sep != std::string_view::npos && compact_sep > 0 && compact_sep + 1 < stem.size()) {
                if (assign_split(stem.substr(0, compact_sep), stem.substr(compact_sep + 1))) {
                    return true;
                }
            }
            return false;
        }

        static bool library_looks_like_format(std::string_view value) noexcept {
            if (value.empty()) return true;
            if (value == "UNKNOWN") return true;
            if (is_audio_extension(value)) return true;
            if (value.size() > 5) return false;
            for (char ch : value) {
                if (!std::isalnum(static_cast<unsigned char>(ch))) return false;
            }
            return true;
        }

        static std::string_view library_artist_from_stem(std::string_view stem) noexcept {
            std::string_view title{};
            std::string_view artist{};
            if (!library_split_track_stem(stem, title, artist)) return {};
            return artist;
        }

        static std::string_view library_title_from_stem(std::string_view stem) noexcept {
            std::string_view title{};
            std::string_view artist{};
            if (!library_split_track_stem(stem, title, artist)) return stem;
            return title;
        }

        static std::uint32_t library_shuffle_key(int track_index, std::uint32_t seed) noexcept {
            std::uint32_t x = seed ^ (0x9E3779B9u * (static_cast<std::uint32_t>(track_index) + 1u));
            x ^= x >> 16;
            x *= 0x85EBCA6Bu;
            x ^= x >> 13;
            x *= 0xC2B2AE35u;
            x ^= x >> 16;
            return x;
        }

        static const char* library_tab_title(LibraryTab tab) noexcept {
            switch (tab) {
            case LibraryTab::Songs:
                return "Songs";
            case LibraryTab::Albums:
                return "Albums";
            case LibraryTab::Artists:
                return "Artists";
            }
            return "Library";
        }

        static const char* library_tab_path(LibraryTab tab) noexcept {
            switch (tab) {
            case LibraryTab::Songs:
                return "songs";
            case LibraryTab::Albums:
                return "albums";
            case LibraryTab::Artists:
                return "artists";
            }
            return "songs";
        }

        static std::string_view library_album_label(std::string_view album) noexcept {
            return album.empty() ? std::string_view{"Unknown Album"} : album;
        }

        static std::string_view library_artist_label(std::string_view artist) noexcept {
            return artist.empty() ? std::string_view{"Unknown Artist"} : artist;
        }

        bool library_has_context() const noexcept {
            switch (library_tab) {
            case LibraryTab::Albums:
            case LibraryTab::Artists:
                return !library_context_key.empty();
            case LibraryTab::Songs:
            default:
                return false;
            }
        }

        bool library_shows_group_rows() const noexcept {
            return !library_has_context() && library_tab != LibraryTab::Songs;
        }

        void reset_track_duration_cache() {
            track_duration_cache_sec.clear();
            list_duration_probe_cursor = 0;
            last_list_duration_probe_ms = 0;
        }

        void ensure_track_duration_cache() {
            const auto* tracks = storage.tracks;
            const std::size_t count = tracks ? tracks->size() : 0;
            if (track_duration_cache_sec.size() != count) {
                track_duration_cache_sec.assign(count, kTrackDurationUnknown);
                list_duration_probe_cursor = 0;
                last_list_duration_probe_ms = 0;
            }
        }

        int cached_track_duration(int track_index) const noexcept {
            if (track_index < 0) return kTrackDurationUnknown;
            if (track_index >= static_cast<int>(track_duration_cache_sec.size())) return kTrackDurationUnknown;
            return track_duration_cache_sec[static_cast<std::size_t>(track_index)];
        }

        LibraryRowModel* library_row_model(int row_index) noexcept {
            if (row_index < 0 || row_index >= static_cast<int>(list_rows.size())) return nullptr;
            return &list_rows[static_cast<std::size_t>(row_index)];
        }

        const LibraryRowModel* library_row_model(int row_index) const noexcept {
            if (row_index < 0 || row_index >= static_cast<int>(list_rows.size())) return nullptr;
            return &list_rows[static_cast<std::size_t>(row_index)];
        }

        LibraryRowRecipe make_library_row_recipe(int row_index) const noexcept {
            LibraryRowRecipe out{};
            out.row_index = row_index;
            out.model = library_row_model(row_index);
            out.track_index = list_index_to_track(row_index);
            out.group_row = out.model ? out.model->group_row : false;
            out.selected_row = (row_index == last_list_selected);
            out.menu_title_cstr = out.group_row ? "Group" : "Track";
            if (out.group_row) {
                out.current_row = library_group_row_is_current(row_index);
            } else if (track_index >= 0 && out.track_index >= 0) {
                out.current_row = (out.track_index == track_index);
            }
            if (out.model) {
                if (!out.model->title.empty()) out.menu_title_cstr = out.model->title.c_str();
                out.detail_primary = out.model->title.view();
                out.detail_secondary = out.model->subtitle.view();
            }
            const bool row_has_action = (out.model != nullptr) || (out.track_index >= 0);
            if (out.group_row) {
                out.show_tail_action = false;
                out.tail_action_icon = ::ui::scene::invalid_image_id();
                out.tail_icon = icons.chevron_right;
            } else {
                out.show_tail_action = row_has_action && (out.selected_row || !out.current_row);
                out.tail_action_icon = out.show_tail_action ? icons.more : ::ui::scene::invalid_image_id();
                out.tail_icon = out.current_row
                    ? (is_paused() ? icons.pause : icons.play)
                    : ::ui::scene::invalid_image_id();
            }

            const auto* tracks = storage.tracks;
            if (!tracks || out.track_index < 0 || out.track_index >= static_cast<int>(tracks->size())) {
                return out;
            }

            const auto& path = (*tracks)[static_cast<std::size_t>(out.track_index)];
            out.track_path = path.view();
            if (!out.group_row) {
                out.detail_primary = out.track_path;
                out.detail_secondary = {};
            }
            if (out.group_row) {
                out.fallback_icon = icons.folder;
                out.prefer_cover = (library_tab == LibraryTab::Albums) && is_audio_path(out.track_path);
            } else {
                out.fallback_icon = is_audio_path(out.track_path)
                    ? icons.play
                    : icons.folder;
                out.prefer_cover = is_audio_path(out.track_path);
            }
            if (out.track_index < static_cast<int>(list_cover_paths.size())) {
                out.cover_path = list_cover_paths[static_cast<std::size_t>(out.track_index)].view();
            }
            return out;
        }

        bool cache_track_duration(int track_index, int seconds) {
            ensure_track_duration_cache();
            if (track_index < 0 || track_index >= static_cast<int>(track_duration_cache_sec.size())) return false;
            const int normalized = (seconds > 0) ? seconds : kTrackDurationUnavailable;
            int& slot = track_duration_cache_sec[static_cast<std::size_t>(track_index)];
            if (slot == normalized) return false;
            slot = normalized;
            return true;
        }

        void refresh_list_row_tail(LibraryRowModel& row, int track_index) {
            row.tail.clear();
            const auto* tracks = storage.tracks;
            if (!tracks || track_index < 0 || track_index >= static_cast<int>(tracks->size())) {
                return;
            }
            if (row.group_row) {
                return;
            }
            const auto path = (*tracks)[static_cast<std::size_t>(track_index)].view();
            const int duration_sec = cached_track_duration(track_index);
            if (duration_sec > 0) {
                char buf[16]{};
                format_duration_compact(duration_sec, buf, sizeof(buf));
                row.tail.assign(buf);
            } else if (duration_sec == kTrackDurationUnknown) {
                row.tail.assign("--:--");
            } else {
                row.tail.assign(format_from_path(path));
            }
            player::font_cache::ensure_text(row.tail.view());
        }

        void rebuild_list_display_tails() {
            const std::size_t count = list_order.size();
            if (list_rows.size() != count) {
                list_rows.resize(count);
            }
            for (std::size_t i = 0; i < count; ++i) {
                refresh_list_row_tail(list_rows[i], list_order[i]);
            }
        }

        bool refresh_list_tail_for_track(int track_index) {
            if (track_index < 0 || library_shows_group_rows()) return false;
            const int row_index = track_index_to_list(track_index);
            auto* row = library_row_model(row_index);
            if (!row || row->group_row) return false;
            refresh_list_row_tail(*row, track_index);
            return true;
        }

        bool refresh_group_row_summary_for_track(int track_index) {
            if (track_index < 0 || !library_shows_group_rows()) return false;
            const int row_index = track_index_to_list(track_index);
            auto* row = library_row_model(row_index);
            if (!row || !row->group_row) return false;

            const auto key = library_group_title_for_track(track_index);
            char buf[64]{};
            format_library_group_summary(key,
                                         library_group_row_is_current(row_index),
                                         buf,
                                         sizeof(buf),
                                         false);
            row->subtitle.assign(buf);
            player::font_cache::ensure_text(row->subtitle.view());
            return true;
        }

        void notify_list_subtitle_source_changed() {
            if (!access.valid() || !handles.list) return;
            access.set_list_view_subtitle_source(handles.list, this, &PlayerController::list_view_subtitle);
        }

        void notify_list_tail_source_changed() {
            if (!access.valid() || !handles.list) return;
            access.set_list_view_tail_source(handles.list, this, &PlayerController::list_view_tail);
        }

        void notify_list_text_sources_changed() {
            notify_list_subtitle_source_changed();
            notify_list_tail_source_changed();
        }

        void tick_list_duration_probe() {
            if (!access.valid() || !handles.list) return;
            const auto* tracks = storage.tracks;
            if (!tracks || tracks->size() == 0) return;
            ensure_track_duration_cache();

            const std::uint64_t now_ms = charm::system::ClockCaps::TimeSource::now();
            if (last_list_duration_probe_ms != 0
                && now_ms - last_list_duration_probe_ms < kListDurationProbeIntervalMs) {
                return;
            }

            auto try_probe_track = [this, tracks](int track_index) -> bool {
                if (track_index < 0 || track_index >= static_cast<int>(tracks->size())) return false;
                if (cached_track_duration(track_index) != kTrackDurationUnknown) return false;
                int seconds = 0;
                const bool ok = player::probe_duration_seconds((*tracks)[static_cast<std::size_t>(track_index)].c_str(), seconds);
                const bool changed = cache_track_duration(track_index, ok ? seconds : 0);
                if (changed) {
                    if (library_shows_group_rows()) {
                        if (refresh_group_row_summary_for_track(track_index)) {
                            notify_list_subtitle_source_changed();
                        } else {
                            rebuild_list_display_cache();
                            notify_list_subtitle_source_changed();
                            notify_list_tail_source_changed();
                        }
                    } else {
                        if (refresh_list_tail_for_track(track_index)) {
                            notify_list_tail_source_changed();
                        }
                        if (library_has_context() && library_context_matches_track(track_index)) {
                            update_list_placeholder();
                            show_library_context_status();
                        }
                    }
                }
                return true;
            };

            if (try_probe_track(track_index)) {
                last_list_duration_probe_ms = now_ms;
                return;
            }

            const std::size_t count = list_order.size();
            if (library_shows_group_rows()) {
                const std::size_t track_count = tracks->size();
                const std::size_t start = (list_duration_probe_cursor < track_count) ? list_duration_probe_cursor : 0;
                for (std::size_t offset = 0; offset < track_count; ++offset) {
                    const std::size_t row = (start + offset) % track_count;
                    if (try_probe_track(static_cast<int>(row))) {
                        list_duration_probe_cursor = (row + 1) % track_count;
                        last_list_duration_probe_ms = now_ms;
                        return;
                    }
                }
            } else {
                if (count == 0) return;
                const std::size_t start = (list_duration_probe_cursor < count) ? list_duration_probe_cursor : 0;
                for (std::size_t offset = 0; offset < count; ++offset) {
                    const std::size_t row = (start + offset) % count;
                    if (try_probe_track(list_order[row])) {
                        list_duration_probe_cursor = (row + 1) % count;
                        last_list_duration_probe_ms = now_ms;
                        return;
                    }
                }
            }

            last_list_duration_probe_ms = now_ms;
        }

        static std::string_view library_group_title_for_track_in_tab(StorageView storage,
                                                                     int track_index,
                                                                     LibraryTab tab) noexcept {
            const auto* tracks = storage.tracks;
            if (!tracks || track_index < 0 || track_index >= static_cast<int>(tracks->size())) {
                return {};
            }
            const auto path = (*tracks)[static_cast<std::size_t>(track_index)].view();
            const std::string_view stem = library_track_stem(path);
            const std::string_view artist = library_artist_from_stem(stem);
            const std::string_view album = library_parent_folder(path);
            switch (tab) {
            case LibraryTab::Albums:
                return library_album_label(album);
            case LibraryTab::Artists:
                return library_artist_label(artist);
            case LibraryTab::Songs:
            default:
                return {};
            }
        }

        std::string_view library_group_title_for_track(int track_index) const noexcept {
            return library_group_title_for_track_in_tab(storage, track_index, library_tab);
        }

        bool library_context_matches_track(int track_index) const noexcept {
            if (!library_has_context()) return true;
            return compare_ci(library_group_title_for_track(track_index), library_context_key.view()) == 0;
        }

        int library_group_track_count(std::string_view key) const noexcept {
            if (key.empty()) return 0;
            const auto* tracks = storage.tracks;
            if (!tracks) return 0;
            int count = 0;
            for (std::size_t i = 0; i < tracks->size(); ++i) {
                if (compare_ci(library_group_title_for_track(static_cast<int>(i)), key) == 0) {
                    ++count;
                }
            }
            return count;
        }

        int library_group_track_count_for_row(int row_index) const noexcept {
            if (!library_shows_group_rows()) return 0;
            if (row_index < 0 || row_index >= static_cast<int>(list_order.size())) return 0;
            const auto key = library_group_title_for_track(list_order[static_cast<std::size_t>(row_index)]);
            return library_group_track_count(key);
        }

        int library_group_total_duration(std::string_view key, bool* complete_out = nullptr) const noexcept {
            if (complete_out) *complete_out = false;
            if (key.empty()) return 0;
            const auto* tracks = storage.tracks;
            if (!tracks) return 0;
            bool complete = true;
            int total_sec = 0;
            for (std::size_t i = 0; i < tracks->size(); ++i) {
                if (compare_ci(library_group_title_for_track(static_cast<int>(i)), key) != 0) {
                    continue;
                }
                const int duration_sec = cached_track_duration(static_cast<int>(i));
                if (duration_sec == kTrackDurationUnknown) {
                    complete = false;
                    break;
                }
                if (duration_sec > 0) {
                    total_sec += duration_sec;
                }
            }
            if (complete_out) *complete_out = complete;
            return total_sec;
        }

        int library_group_total_duration_for_row(int row_index, bool* complete_out = nullptr) const noexcept {
            if (complete_out) *complete_out = false;
            if (!library_shows_group_rows()) return 0;
            if (row_index < 0 || row_index >= static_cast<int>(list_order.size())) return 0;
            const auto key = library_group_title_for_track(list_order[static_cast<std::size_t>(row_index)]);
            return library_group_total_duration(key, complete_out);
        }

        bool library_group_row_is_current(int row_index) const noexcept {
            if (!library_shows_group_rows()) return false;
            if (track_index < 0) return false;
            if (row_index < 0 || row_index >= static_cast<int>(list_order.size())) return false;
            const auto row_group = library_group_title_for_track(list_order[static_cast<std::size_t>(row_index)]);
            const auto current_group = library_group_title_for_track(track_index);
            return !row_group.empty() && compare_ci(row_group, current_group) == 0;
        }

        const char* library_current_group_state_text() const noexcept {
            if (is_paused()) return "Paused";
            if (is_playing()) return "Playing";
            return "Current";
        }

        bool library_context_is_current_group() const noexcept {
            if (!library_has_context()) return false;
            if (track_index < 0) return false;
            const auto current_group = library_group_title_for_track(track_index);
            return !current_group.empty()
                && compare_ci(current_group, library_context_key.view()) == 0;
        }

        bool library_shows_context_watermark() const noexcept {
            return library_has_context()
                && list_order.size() >= 1
                && list_order.size() <= 3;
        }

        void format_library_group_summary(std::string_view key,
                                          bool current_group,
                                          char* buf,
                                          std::size_t buf_size,
                                          bool include_state = true) const noexcept {
            if (!buf || buf_size == 0) return;
            buf[0] = '\0';
            if (key.empty()) return;
            const auto* tracks = storage.tracks;
            if (!tracks) return;
            const auto append_current_group_state = [&]() noexcept {
                if (!include_state) return;
                if (!current_group || library_shows_group_rows()) return;
                const char* state = library_current_group_state_text();
                const std::size_t used = std::char_traits<char>::length(buf);
                if (used == 0 || used + 3 >= buf_size) return;
                std::snprintf(buf + used, buf_size - used, " | %s", state);
            };

            int track_count = 0;
            int total_sec = 0;
            bool duration_complete = true;
            int album_count = 0;
            std::string primary_artist{};
            bool multiple_artists = false;
            std::vector<std::string> unique_albums{};

            for (std::size_t i = 0; i < tracks->size(); ++i) {
                if (compare_ci(library_group_title_for_track(static_cast<int>(i)), key) != 0) {
                    continue;
                }

                ++track_count;
                const auto path = (*tracks)[i].view();
                const int duration_sec = cached_track_duration(static_cast<int>(i));
                if (duration_sec == kTrackDurationUnknown) {
                    duration_complete = false;
                } else if (duration_sec > 0) {
                    total_sec += duration_sec;
                }

                if (library_tab == LibraryTab::Albums) {
                    const auto stem = library_track_stem(path);
                    const auto artist = library_artist_label(library_artist_from_stem(stem));
                    if (primary_artist.empty()) {
                        primary_artist.assign(artist.begin(), artist.end());
                    } else if (compare_ci(primary_artist, artist) != 0) {
                        multiple_artists = true;
                    }
                } else if (library_tab == LibraryTab::Artists) {
                    const auto album = library_album_label(library_parent_folder(path));
                    bool exists = false;
                    for (const auto& existing : unique_albums) {
                        if (compare_ci(existing, album) == 0) {
                            exists = true;
                            break;
                        }
                    }
                    if (!exists) {
                        unique_albums.emplace_back(album.begin(), album.end());
                    }
                }
            }

            album_count = static_cast<int>(unique_albums.size());
            const char* track_label = (track_count == 1) ? "track" : "tracks";
            if (library_tab == LibraryTab::Albums && !primary_artist.empty()) {
                const char* artist_label = multiple_artists ? "Various Artists" : primary_artist.c_str();
                if (duration_complete && total_sec > 0) {
                    char duration_buf[16]{};
                    format_duration_compact(total_sec, duration_buf, sizeof(duration_buf));
                    std::snprintf(buf, buf_size, "%s | %d %s | %s",
                                  artist_label, track_count, track_label, duration_buf);
                } else {
                    std::snprintf(buf, buf_size, "%s | %d %s",
                                  artist_label, track_count, track_label);
                }
                append_current_group_state();
                return;
            }

            if (library_tab == LibraryTab::Artists && album_count > 0) {
                const char* album_label = (album_count == 1) ? "album" : "albums";
                if (duration_complete && total_sec > 0) {
                    char duration_buf[16]{};
                    format_duration_compact(total_sec, duration_buf, sizeof(duration_buf));
                    std::snprintf(buf, buf_size, "%d %s | %d %s | %s",
                                  album_count, album_label, track_count, track_label, duration_buf);
                } else {
                    std::snprintf(buf, buf_size, "%d %s | %d %s",
                                  album_count, album_label, track_count, track_label);
                }
                append_current_group_state();
                return;
            }

            if (duration_complete && total_sec > 0) {
                char duration_buf[16]{};
                format_duration_compact(total_sec, duration_buf, sizeof(duration_buf));
                std::snprintf(buf, buf_size, "%d %s | %s", track_count, track_label, duration_buf);
            } else {
                std::snprintf(buf, buf_size, "%d %s", track_count, track_label);
            }
            append_current_group_state();
        }

        ::ui::scene::ImageId library_context_cover_for_view() {
            const auto* tracks = storage.tracks;
            if (!library_has_context() || !tracks) {
                return ::ui::scene::invalid_image_id();
            }
            const auto try_track = [&](int candidate) -> ::ui::scene::ImageId {
                if (candidate < 0 || candidate >= static_cast<int>(tracks->size())) {
                    return ::ui::scene::invalid_image_id();
                }
                if (!library_context_matches_track(candidate)) {
                    return ::ui::scene::invalid_image_id();
                }
                const auto track_path = (*tracks)[static_cast<std::size_t>(candidate)].view();
                if (track_path.empty()) {
                    return ::ui::scene::invalid_image_id();
                }
                std::string_view cover_path{};
                if (candidate < static_cast<int>(list_cover_paths.size())) {
                    cover_path = list_cover_paths[static_cast<std::size_t>(candidate)].view();
                }
                return resolve_list_cover_icon(track_path, cover_path);
            };

            const int selected_track = list_index_to_track(last_list_selected);
            auto image_id = try_track(selected_track);
            if (::ui::scene::image_id_valid(image_id)) {
                return image_id;
            }

            image_id = try_track(track_index);
            if (::ui::scene::image_id_valid(image_id)) {
                return image_id;
            }

            for (std::size_t i = 0; i < tracks->size(); ++i) {
                const int candidate = static_cast<int>(i);
                if (candidate == selected_track || candidate == track_index) {
                    continue;
                }
                image_id = try_track(candidate);
                if (::ui::scene::image_id_valid(image_id)) {
                    return image_id;
                }
            }
            return ::ui::scene::invalid_image_id();
        }

        void update_library_context_watermark() {
            if (!access.valid()) return;
            const auto hide = [&]() {
                if (handles.list_context_cover) {
                    access.set_visible(handles.list_context_cover, false);
                    if (access.kind(handles.list_context_cover) == WidgetKind::Image) {
                        access.set_image(handles.list_context_cover, ::ui::scene::invalid_image_id());
                    }
                }
                if (handles.list_context_cover_scrim) {
                    access.set_visible(handles.list_context_cover_scrim, false);
                }
            };
            if (!library_shows_context_watermark()) {
                hide();
                return;
            }

            const auto image_id = library_context_cover_for_view();
            if (!::ui::scene::image_id_valid(image_id)) {
                hide();
                return;
            }
            if (handles.list_context_cover && access.kind(handles.list_context_cover) == WidgetKind::Image) {
                access.set_image(handles.list_context_cover, image_id);
                access.set_visible(handles.list_context_cover, true);
            }
            if (handles.list_context_cover_scrim) {
                access.set_visible(handles.list_context_cover_scrim, true);
            }
        }

        void show_library_context_status() {
            if (!library_has_context()) return;
            const bool current_group = library_context_is_current_group();
            char summary[160]{};
            format_library_group_summary(library_context_key.view(),
                                         current_group,
                                         summary,
                                         sizeof(summary));

            char buf[256]{};
            if (summary[0] != '\0') {
                std::snprintf(buf, sizeof(buf), "%s | %.*s | %s",
                              library_group_status_label(),
                              static_cast<int>(library_context_key.size()), library_context_key.view().data(),
                              summary);
            } else if (current_group) {
                std::snprintf(buf, sizeof(buf), "%s | %.*s | %s",
                              library_group_status_label(),
                              static_cast<int>(library_context_key.size()), library_context_key.view().data(),
                              library_current_group_state_text());
            } else {
                std::snprintf(buf, sizeof(buf), "%s | %.*s",
                              library_group_status_label(),
                              static_cast<int>(library_context_key.size()), library_context_key.view().data());
            }
            if (list_shuffle_enabled) {
                append_library_status_suffix(buf, sizeof(buf), "Shuffled");
            }
            set_status(buf);
        }

        void show_library_view_status() {
            if (library_has_context()) {
                show_library_context_status();
                return;
            }
            const int count = static_cast<int>(list_order.size());
            char buf[160]{};
            const auto append_current_group = [&]() noexcept {
                if (!library_shows_group_rows() || track_index < 0) return;
                const auto current_group = library_group_title_for_track(track_index);
                if (current_group.empty()) return;
                append_library_status_suffix(buf, sizeof(buf), library_current_group_state_text());
                const std::size_t used = std::char_traits<char>::length(buf);
                if (used == 0 || used + 3 >= sizeof(buf)) return;
                std::snprintf(buf + used, sizeof(buf) - used, " | %.*s",
                              static_cast<int>(current_group.size()), current_group.data());
            };
            if (library_shows_group_rows()) {
                const char* label = (library_tab == LibraryTab::Albums)
                    ? ((count == 1) ? "album" : "albums")
                    : ((count == 1) ? "artist" : "artists");
                std::snprintf(buf, sizeof(buf), "%s | %d %s",
                              library_tab_title(library_tab), count, label);
                append_current_group();
            } else {
                const char* label = (count == 1) ? "track" : "tracks";
                std::snprintf(buf, sizeof(buf), "%s | %d %s",
                              library_tab_title(library_tab), count, label);
            }
            if (list_shuffle_enabled) {
                append_library_status_suffix(buf, sizeof(buf), "Shuffled");
            }
            set_status(buf);
        }

        static void append_library_status_suffix(char* buf,
                                                 std::size_t buf_size,
                                                 const char* suffix) noexcept {
            if (!buf || buf_size == 0 || !suffix || suffix[0] == '\0') return;
            const std::size_t used = std::char_traits<char>::length(buf);
            if (used == 0 || used + 3 >= buf_size) return;
            std::snprintf(buf + used, buf_size - used, " | %s", suffix);
        }

        bool format_library_row_selection_status(int list_index,
                                                 char* buf,
                                                 std::size_t buf_size) const noexcept {
            if (!buf || buf_size == 0) return false;
            buf[0] = '\0';
            if (list_index < 0 || list_index >= static_cast<int>(list_order.size())) return false;

            const auto row = make_library_row_recipe(list_index);
            auto primary = row.title();
            auto secondary = row.subtitle();
            if (primary.empty()) primary = row.detail_primary;
            if (secondary.empty()) secondary = row.detail_secondary;
            if (row.group_row) {
                if (primary.empty()) return false;
                if (secondary.empty()) {
                    std::snprintf(buf, buf_size, "%s | %.*s",
                                  library_group_status_label(),
                                  static_cast<int>(primary.size()), primary.data());
                } else {
                    std::snprintf(buf, buf_size, "%s | %.*s | %.*s",
                                  library_group_status_label(),
                                  static_cast<int>(primary.size()), primary.data(),
                                  static_cast<int>(secondary.size()), secondary.data());
                }
                if (row.current_row) {
                    append_library_status_suffix(buf, buf_size, library_current_group_state_text());
                }
                if (row.selected_row) {
                    append_library_status_suffix(buf, buf_size, library_group_primary_action_label());
                }
                return true;
            }

            if (primary.empty()) return false;
            if (secondary.empty()) {
                std::snprintf(buf, buf_size, "%.*s",
                              static_cast<int>(primary.size()), primary.data());
            } else {
                std::snprintf(buf, buf_size, "%.*s | %.*s",
                              static_cast<int>(primary.size()), primary.data(),
                              static_cast<int>(secondary.size()), secondary.data());
            }
            if (row.current_row) {
                append_library_status_suffix(buf, buf_size, library_current_group_state_text());
            }
            return true;
        }

        bool format_library_row_secondary_status(int list_index,
                                                 char* buf,
                                                 std::size_t buf_size) const noexcept {
            if (!buf || buf_size == 0) return false;
            buf[0] = '\0';
            if (list_index < 0 || list_index >= static_cast<int>(list_order.size())) return false;

            const auto row = make_library_row_recipe(list_index);
            if (row.group_row) {
                auto primary = row.title();
                auto secondary = row.subtitle();
                if (primary.empty()) primary = row.detail_primary;
                if (secondary.empty()) secondary = row.detail_secondary;
                if (primary.empty()) return false;
                if (secondary.empty()) {
                    std::snprintf(buf, buf_size, "%s | %.*s",
                                  library_group_status_label(),
                                  static_cast<int>(primary.size()), primary.data());
                } else {
                    std::snprintf(buf, buf_size, "%s | %.*s | %.*s",
                                  library_group_status_label(),
                                  static_cast<int>(primary.size()), primary.data(),
                                  static_cast<int>(secondary.size()), secondary.data());
                }
                return true;
            }
            if (row.track_path.empty()) {
                return format_library_row_selection_status(list_index, buf, buf_size);
            }
            std::snprintf(buf, buf_size, "Path | %.*s",
                          static_cast<int>(row.track_path.size()), row.track_path.data());
            return true;
        }

        ::ui::scene::AnchoredMenuHandles list_action_menu_handles() const noexcept {
            ::ui::scene::AnchoredMenuHandles out{};
            out.scrim = handles.list_action_scrim;
            out.card = handles.list_action_card;
            out.title = handles.list_action_title;
            for (std::size_t i = 0; i < handles.list_action_items.size(); ++i) {
                out.items[i] = handles.list_action_items[i];
            }
            return out;
        }

        void close_list_action_menu() {
            sync_list_action_menu_focus_visual(-1);
            list_action_menu_index = -1;
            ::ui::scene::hide_anchored_menu(access, list_action_menu_handles());
            if (handles.list) {
                access.set_focused(handles.list, true);
            }
        }

        std::string_view list_action_tertiary_label_for_track(int track_idx) const noexcept {
            if (track_idx < 0) return {};
            switch (library_tab) {
            case LibraryTab::Songs:
                if (!library_group_title_for_track_in_tab(storage, track_idx, LibraryTab::Albums).empty()) {
                    return "Go to album";
                }
                if (!library_group_title_for_track_in_tab(storage, track_idx, LibraryTab::Artists).empty()) {
                    return "Go to artist";
                }
                return {};
            case LibraryTab::Albums:
                if (!library_group_title_for_track_in_tab(storage, track_idx, LibraryTab::Artists).empty()) {
                    return "Go to artist";
                }
                return {};
            case LibraryTab::Artists:
                if (!library_group_title_for_track_in_tab(storage, track_idx, LibraryTab::Albums).empty()) {
                    return "Go to album";
                }
                return {};
            }
            return {};
        }

        const char* library_group_primary_action_label() const noexcept {
            return "Browse tracks";
        }

        const char* library_group_secondary_action_label() const noexcept {
            switch (library_tab) {
            case LibraryTab::Albums:
                return "Show album summary";
            case LibraryTab::Artists:
                return "Show artist summary";
            case LibraryTab::Songs:
            default:
                return "Show summary";
            }
        }

        const char* library_group_status_label() const noexcept {
            switch (library_tab) {
            case LibraryTab::Albums:
                return "Album";
            case LibraryTab::Artists:
                return "Artist";
            case LibraryTab::Songs:
            default:
                return "Group";
            }
        }

        const char* library_group_object_label() const noexcept {
            switch (library_tab) {
            case LibraryTab::Albums:
                return "album";
            case LibraryTab::Artists:
                return "artist";
            case LibraryTab::Songs:
            default:
                return "group";
            }
        }

        const char* library_group_tail_hint_label() const noexcept {
            return "Browse";
        }

        std::size_t build_list_action_menu_items(int list_index,
                                                 ::ui::scene::AnchoredMenuItemSpec (&items)[3]) const noexcept {
            for (auto& item : items) item = {};
            if (list_index < 0 || list_index >= static_cast<int>(list_order.size())) return 0;

            const auto row = make_library_row_recipe(list_index);
            items[0] = {
                .text = row.group_row ? library_group_primary_action_label() : "Play now",
                .visible = true,
            };
            items[1] = {
                .text = row.group_row ? library_group_secondary_action_label() : "Show path",
                .visible = true,
            };

            if (row.group_row) return 2;

            const int track_idx = list_index_to_track(list_index);
            const auto tertiary_label = list_action_tertiary_label_for_track(track_idx);
            if (tertiary_label.empty()) return 2;
            items[2] = {
                .text = tertiary_label.data(),
                .visible = true,
            };
            return 3;
        }

        StylePatch list_action_menu_focus_patch() const noexcept {
            StylePatch patch = ::ui::scene::make_pill_surface_patch({
                .bg_color = kUiLibraryChipActive,
                .border_color = kUiLibraryPathBorderActive,
                .corner_radius = 12,
                .inner_stroke_enabled = true,
                .inner_stroke_color = with_alpha(kUiLibraryListOnAccent, 56),
                .inner_stroke_width = 1,
            });
            patch.has_font_color = true;
            patch.font_color = kUiLibraryChipTextActive;
            patch.has_border_focus = true;
            patch.border_focus = kUiLibraryPathBorderActive;
            return patch;
        }

        void sync_list_action_menu_focus_visual(int item_index) {
            if (!access.valid()) return;
            const auto menu = list_action_menu_handles();
            const StylePatch patch = list_action_menu_focus_patch();
            for (std::size_t i = 0; i < menu.items.size(); ++i) {
                const WidgetHandle item = menu.items[i];
                if (!item) continue;
                if (static_cast<int>(i) == item_index) {
                    access.set_style_patch(item, patch);
                } else {
                    access.clear_style_patch(item);
                }
            }
        }

        int focused_list_action_menu_item() const noexcept {
            if (!access.valid()) return -1;
            return ::ui::scene::anchored_menu_focused_index(
                const_cast<::ui::scene::SceneAccess&>(access),
                list_action_menu_handles());
        }

        void focus_list_action_menu_item(int item_index) {
            ::ui::scene::set_anchored_menu_focus(access, list_action_menu_handles(), item_index);
            sync_list_action_menu_focus_visual(item_index);
        }

        bool handle_list_action_menu_key(UiKey key) {
            if (list_action_menu_index < 0) return false;
            ::ui::scene::AnchoredMenuItemSpec items[3]{};
            const std::size_t item_count = build_list_action_menu_items(list_action_menu_index, items);
            if (item_count == 0) return false;

            const int focused = focused_list_action_menu_item();
            switch (key) {
            case UiKey::Up: {
                const int next = ::ui::scene::anchored_menu_next_visible_index(items, item_count, focused, -1);
                if (next >= 0) focus_list_action_menu_item(next);
                return true;
            }
            case UiKey::Down: {
                const int next = ::ui::scene::anchored_menu_next_visible_index(items, item_count, focused, 1);
                if (next >= 0) focus_list_action_menu_item(next);
                return true;
            }
            case UiKey::Enter: {
                int invoke_index = focused;
                if (invoke_index < 0) {
                    invoke_index = ::ui::scene::anchored_menu_first_visible_index(items, item_count);
                }
                if (invoke_index >= 0) {
                    invoke_list_action_item(invoke_index);
                }
                return true;
            }
            case UiKey::Left:
                close_list_action_menu();
                return true;
            default:
                return false;
            }
        }

        void open_list_action_menu(int list_index) {
            if (!access.valid()) return;
            if (list_index < 0 || list_index >= static_cast<int>(list_order.size())) {
                close_list_action_menu();
                return;
            }

            list_action_menu_index = list_index;

            if (handles.list) {
                ignore_list_select = true;
                access.set_list_view_selected(handles.list, list_index);
                ignore_list_select = false;
                last_list_selected = list_index;
            }

            const auto row = make_library_row_recipe(list_index);
            ::ui::scene::AnchoredMenuItemSpec items[3]{};
            const std::size_t item_count = build_list_action_menu_items(list_index, items);

            Rect anchor_rect{kUiPadding, kUiPadding, 0, kLibraryListRowHeight};
            if (handles.list) {
                const Rect list_rect = access.world_rect(handles.list);
                const int scroll_y = access.scroll_y(handles.list);
                const int row_y = list_rect.y + list_index * kLibraryListRowHeight - scroll_y;
                anchor_rect = {list_rect.x, row_y, list_rect.w, kLibraryListRowHeight};
            }

            ::ui::scene::show_anchored_menu(
                access,
                list_action_menu_handles(),
                {
                    .anchor = anchor_rect,
                    .viewport = {0, 0, screen_width, screen_height},
                    .title = row.menu_title_cstr,
                    .items = items,
                    .item_count = item_count,
                });
            const int initial_focus = ::ui::scene::anchored_menu_first_visible_index(items, item_count);
            if (initial_focus >= 0) {
                focus_list_action_menu_item(initial_focus);
            } else {
                sync_list_action_menu_focus_visual(-1);
            }
        }

        void invoke_list_action_primary() {
            const int list_index = list_action_menu_index;
            close_list_action_menu();
            if (list_index < 0 || list_index >= static_cast<int>(list_order.size())) return;
            activate_list_index(list_index);
        }

        void invoke_list_action_secondary() {
            const int list_index = list_action_menu_index;
            close_list_action_menu();
            if (list_index < 0 || list_index >= static_cast<int>(list_order.size())) return;

            char buf[224]{};
            if (!format_library_row_secondary_status(list_index, buf, sizeof(buf))) return;
            set_status(buf);
        }

        void invoke_list_action_tertiary() {
            const int list_index = list_action_menu_index;
            close_list_action_menu();
            if (list_index < 0 || list_index >= static_cast<int>(list_order.size())) return;
            if (library_shows_group_rows()) return;

            const int track_idx = list_index_to_track(list_index);
            if (track_idx < 0) return;

            LibraryTab target_tab = library_tab;
            std::string_view key{};
            switch (library_tab) {
            case LibraryTab::Songs:
                key = library_group_title_for_track_in_tab(storage, track_idx, LibraryTab::Albums);
                if (!key.empty()) {
                    target_tab = LibraryTab::Albums;
                    break;
                }
                key = library_group_title_for_track_in_tab(storage, track_idx, LibraryTab::Artists);
                if (!key.empty()) {
                    target_tab = LibraryTab::Artists;
                }
                break;
            case LibraryTab::Albums:
                key = library_group_title_for_track_in_tab(storage, track_idx, LibraryTab::Artists);
                if (!key.empty()) {
                    target_tab = LibraryTab::Artists;
                }
                break;
            case LibraryTab::Artists:
                key = library_group_title_for_track_in_tab(storage, track_idx, LibraryTab::Albums);
                if (!key.empty()) {
                    target_tab = LibraryTab::Albums;
                }
                break;
            }

            if (key.empty()) return;
            library_tab = target_tab;
            library_context_key.assign(key);
            refresh_list_view();
            ensure_library_context_selection();
            show_library_context_status();
        }

        void invoke_list_action_item(int item_index) {
            switch (item_index) {
            case 0:
                invoke_list_action_primary();
                break;
            case 1:
                invoke_list_action_secondary();
                break;
            case 2:
                invoke_list_action_tertiary();
                break;
            default:
                break;
            }
        }

        void activate_list_index(int list_index) {
            if (list_index < 0 || list_index >= static_cast<int>(list_order.size())) return;
            const auto row = make_library_row_recipe(list_index);
            if (row.group_row) {
                if (row.model) {
                    library_context_key.assign(row.title());
                    refresh_list_view();
                    ensure_library_context_selection();
                    show_library_context_status();
                }
                return;
            }
            const int track_idx = list_index_to_track(list_index);
            if (track_idx >= 0) {
                select_track_index(track_idx);
            }
        }

        void invalidate_library_visual_state() noexcept {
            library_visual_state.valid = false;
            ++library_visual_generation;
            if (library_visual_generation == 0) {
                library_visual_generation = 1;
            }
        }

        WidgetHandle active_library_tab_handle() const noexcept {
            switch (library_tab) {
            case LibraryTab::Albums:
                return handles.list_tab_albums;
            case LibraryTab::Artists:
                return handles.list_tab_artist;
            case LibraryTab::Songs:
            default:
                return handles.list_tab_songs;
            }
        }

        bool library_can_step_back() const noexcept {
            return library_has_context()
                || list_shuffle_enabled
                || library_tab != LibraryTab::Songs;
        }

        const char* library_back_target_label() const noexcept {
            if (library_has_context()) {
                return library_tab_title(library_tab);
            }
            if (list_shuffle_enabled) {
                return "Shuffle Off";
            }
            if (library_tab != LibraryTab::Songs) {
                return library_tab_title(LibraryTab::Songs);
            }
            return "Library";
        }

        bool format_library_path_bar_hint(char* buf,
                                          std::size_t buf_size) const noexcept {
            if (!buf || buf_size == 0) return false;
            buf[0] = '\0';
            const std::string_view back_target = library_back_target_label();
            if (back_target.empty()) return false;
            std::snprintf(buf, buf_size, "Path bar returns to %.*s",
                          static_cast<int>(back_target.size()), back_target.data());
            return buf[0] != '\0';
        }

        void apply_library_tab_style(WidgetHandle handle, bool active, int radius) {
            if (!access.valid() || !handle) return;
            ::ui::scene::PillSurfaceSpec spec{};
            spec.bg_color = active ? kUiLibraryTabActive : kUiLibraryChipIdle;
            spec.border_color = {0, 0, 0, 0};
            spec.corner_radius = radius;
            StylePatch patch = ::ui::scene::make_pill_surface_patch(spec);
            patch.has_border_width = true;
            patch.border_width = 0;
            patch.has_shadow_enabled = true;
            patch.shadow_enabled = false;
            patch.has_inner_stroke_enabled = true;
            patch.inner_stroke_enabled = false;
            patch.has_font = true;
            patch.font = active
                ? &get_player_font_px(19, FontWeight::Bold)
                : &get_player_font_px(18, FontWeight::Medium);
            patch.has_font_color = true;
            patch.font_color = active ? kUiLibraryTabTextActive : kUiLibraryChipTextMuted;
            patch.has_border_focus = true;
            patch.border_focus = kUiLibraryTabBorderActive;
            access.set_style_override(handle, patch);
        }

        void apply_library_chip_style(WidgetHandle handle, bool active, int radius,
                                      bool elevated = false) {
            if (!access.valid() || !handle) return;
            ::ui::scene::PillSurfaceSpec spec{};
            spec.bg_color = active ? kUiLibraryChipActive : kUiLibraryChipIdle;
            spec.border_color = {0, 0, 0, 0};
            spec.corner_radius = radius;
            StylePatch patch = ::ui::scene::make_pill_surface_patch(spec);
            patch.has_border_width = true;
            patch.border_width = 0;
            patch.has_shadow_enabled = true;
            patch.shadow_enabled = false;
            patch.has_inner_stroke_enabled = true;
            patch.inner_stroke_enabled = false;
            patch.has_font_color = true;
            patch.font_color = active
                ? kUiLibraryChipTextActive
                : (elevated ? kUiLibraryChipText : kUiLibraryChipTextMuted);
            patch.has_font = true;
            patch.font = active
                ? &get_player_font_px(elevated ? 17 : 17, FontWeight::Medium)
                : &get_player_font_px(elevated ? 17 : 16, FontWeight::Regular);
            patch.has_border_focus = true;
            patch.border_focus = kUiLibraryPathBorderActive;
            access.set_style_override(handle, patch);
        }

        void update_library_state_visuals() {
            if (!access.valid()) return;
            const bool has_context = library_has_context();
            const bool path_active = library_can_step_back();
            const bool sort_desc = list_sort == ListSort::NameDesc;
            const bool sort_active = list_shuffle_enabled || sort_desc;
            const WidgetHandle active_tab = active_library_tab_handle();
            const Rect active_rect = active_tab ? access.world_rect(active_tab) : Rect{};
            if (library_visual_state.valid
                && library_visual_state.generation == library_visual_generation
                && library_visual_state.tab == library_tab
                && library_visual_state.has_context == has_context
                && library_visual_state.path_active == path_active
                && library_visual_state.shuffle_enabled == list_shuffle_enabled
                && library_visual_state.sort_desc == sort_desc
                && library_visual_state.active_tab_rect.x == active_rect.x
                && library_visual_state.active_tab_rect.y == active_rect.y
                && library_visual_state.active_tab_rect.w == active_rect.w
                && library_visual_state.active_tab_rect.h == active_rect.h) {
                return;
            }
            apply_library_tab_style(handles.list_tab_songs, library_tab == LibraryTab::Songs, kLibraryTabRadius);
            apply_library_tab_style(handles.list_tab_albums, library_tab == LibraryTab::Albums, kLibraryTabRadius);
            apply_library_tab_style(handles.list_tab_artist, library_tab == LibraryTab::Artists, kLibraryTabRadius);
            apply_library_chip_style(handles.list_shuffle, list_shuffle_enabled, kLibraryTabRadius, true);
            apply_library_chip_style(handles.list_sort, sort_active, kLibrarySortRadius, true);
            if (handles.list_sort) {
                access.set_button_icon(handles.list_sort, list_shuffle_enabled ? icons.shuffle : icons.down);
            }
            if (handles.list_tab_indicator) {
                const int indicator_w = std::max(36, active_rect.w / 4);
                access.set_rect(handles.list_tab_indicator,
                    {active_rect.x + (active_rect.w - indicator_w) / 2,
                     active_rect.y + active_rect.h + 5,
                     indicator_w,
                     4});
            }
            if (handles.list_title) {
                StylePatch patch{};
                patch.has_font_color = true;
                patch.font_color = has_context
                    ? rgba{kUiTitle.r, kUiTitle.g, kUiTitle.b, 255}
                    : kUiTitle;
                patch.has_font = true;
                patch.font = has_context
                    ? &get_player_font_px(24, FontWeight::Bold)
                    : &get_player_font_px(22, FontWeight::Medium);
                access.set_style_override(handles.list_title, patch);
            }

            if (handles.list_path_bg) {
                const rgba path_bg = has_context
                    ? with_alpha(kUiLibraryPathIdle, 180)
                    : (path_active ? kUiLibraryPathActive : kUiLibraryPathIdle);
                const rgba path_gradient_start = has_context
                    ? with_alpha(kUiLibraryPathActive, 208)
                    : (path_active ? with_alpha(kUiLibraryPathActive, 248)
                                   : with_alpha(kUiLibraryPathIdle, 236));
                const rgba path_gradient_end = has_context
                    ? with_alpha(kUiLibraryPathIdle, 168)
                    : (path_active ? with_alpha(kUiLibraryPathIdle, 224)
                                   : with_alpha(kUiLibraryPathIdle, 228));
                auto patch = ::ui::scene::make_clean_surface_patch({
                    .apply_bg_color = true,
                    .apply_border_color = true,
                    .apply_corner_radius = true,
                    .bg_color = path_bg,
                    .border_color = {0, 0, 0, 0},
                    .corner_radius = kLibraryPathRadius,
                });
                patch.has_gradient_enabled = true;
                patch.gradient_enabled = true;
                patch.has_gradient_start = true;
                patch.gradient_start = path_gradient_start;
                patch.has_gradient_end = true;
                patch.gradient_end = path_gradient_end;
                patch.has_gradient_direction = true;
                patch.gradient_direction = 0;
                access.set_style_override(handles.list_path_bg, patch);
            }
            if (handles.list_path) {
                StylePatch patch{};
                patch.has_font_color = true;
                patch.font_color = has_context
                    ? with_alpha(kUiLibraryPathTextActive, 184)
                    : path_active
                    ? kUiLibraryPathTextActive
                    : with_alpha(kUiLibraryPathText, 236);
                patch.has_font_role = true;
                patch.font_role = FontId::Small;
                patch.has_font_weight = true;
                patch.font_weight = path_active ? FontWeight::Bold : FontWeight::Medium;
                access.set_style_override(handles.list_path, patch);
            }
            if (handles.list_path_hit) {
                access.set_visible(handles.list_path_hit, path_active);
            }
            library_visual_state.tab = library_tab;
            library_visual_state.has_context = has_context;
            library_visual_state.path_active = path_active;
            library_visual_state.shuffle_enabled = list_shuffle_enabled;
            library_visual_state.sort_desc = sort_desc;
            library_visual_state.active_tab_rect = active_rect;
            library_visual_state.generation = library_visual_generation;
            library_visual_state.valid = true;
        }

        LibrarySortKey make_library_sort_key(int track_index) const noexcept {
            const auto* tracks = storage.tracks;
            if (!tracks || track_index < 0 || track_index >= static_cast<int>(tracks->size())) {
                return {};
            }

            const auto path = (*tracks)[static_cast<std::size_t>(track_index)].view();
            const std::string_view stem = library_track_stem(path);
            const std::string_view artist = library_artist_from_stem(stem);
            const std::string_view title = library_title_from_stem(stem);
            const std::string_view album = library_parent_folder(path);

            switch (library_tab) {
            case LibraryTab::Songs:
                return {
                    title.empty() ? stem : title,
                    artist,
                    album,
                };
            case LibraryTab::Albums:
                return {
                    album.empty() ? std::string_view{"Unknown Album"} : album,
                    title.empty() ? stem : title,
                    artist,
                };
            case LibraryTab::Artists:
                return {
                    artist.empty() ? std::string_view{"Unknown Artist"} : artist,
                    title.empty() ? stem : title,
                    album,
                };
            }
            return {};
        }

        void rebuild_list_display_cache() {
            list_rows.clear();

            const std::size_t count = list_order.size();
            list_rows.resize(count);
            for (std::size_t i = 0; i < count; ++i) {
                auto& row = list_rows[i];
                row.title.clear();
                row.subtitle.clear();
                row.tail.clear();
                row.group_row = false;
                const int track_index = list_order[i];
                const auto* tracks = storage.tracks;
                if (!tracks || track_index < 0 || track_index >= static_cast<int>(tracks->size())) {
                    row.title.assign("Unknown Track");
                    row.subtitle.assign("Track");
                    continue;
                }
                const auto path = (*tracks)[static_cast<std::size_t>(track_index)].view();
                const std::string_view stem = library_track_stem(path);
                const std::string_view artist = library_artist_from_stem(stem);
                const std::string_view title = library_title_from_stem(stem);
                const std::string_view album = library_parent_folder(path);
                std::string_view display_title{};
                std::string_view display_subtitle{};
                if (library_shows_group_rows()) {
                    row.group_row = true;
                    display_title = library_group_title_for_track(track_index);
                    char buf[64]{};
                    const bool is_current_group = library_group_row_is_current(static_cast<int>(i));
                    format_library_group_summary(display_title, is_current_group, buf, sizeof(buf), false);
                    row.title.assign(display_title.empty() ? std::string_view{"Library"} : display_title);
                    row.subtitle.assign(buf);
                    player::font_cache::ensure_text(row.title.view());
                    player::font_cache::ensure_text(row.subtitle.view());
                    continue;
                }

                switch (library_tab) {
                case LibraryTab::Songs:
                    display_title = title.empty() ? stem : title;
                    if (!artist.empty() && !album.empty()) {
                        char subtitle_buf[192]{};
                        std::snprintf(subtitle_buf, sizeof(subtitle_buf), "%.*s / %.*s",
                                      static_cast<int>(artist.size()), artist.data(),
                                      static_cast<int>(album.size()), album.data());
                        row.subtitle.assign(subtitle_buf);
                        display_subtitle = row.subtitle.view();
                    } else if (!artist.empty()) {
                        display_subtitle = artist;
                    } else if (!album.empty()) {
                        display_subtitle = album;
                    }
                    break;
                case LibraryTab::Albums:
                    display_title = title.empty() ? (stem.empty() ? std::string_view{"Track"} : stem) : title;
                    if (!artist.empty()) {
                        display_subtitle = artist;
                    }
                    break;
                case LibraryTab::Artists:
                    display_title = title.empty() ? (stem.empty() ? std::string_view{"Track"} : stem) : title;
                    if (!album.empty()) {
                        display_subtitle = album;
                    }
                    break;
                }

                if (display_title.empty()) display_title = "Unknown Track";
                if (library_looks_like_format(display_subtitle)) display_subtitle = {};
                row.title.assign(display_title);
                row.subtitle.assign(display_subtitle);
                player::font_cache::ensure_text(row.title.view());
                player::font_cache::ensure_text(row.subtitle.view());
            }
            rebuild_list_display_tails();
        }

        int list_index_to_track(int list_index) const noexcept {
            if (list_index < 0) return -1;
            if (list_index >= static_cast<int>(list_order.size())) return -1;
            return list_order[static_cast<std::size_t>(list_index)];
        }

        int track_index_to_list(int track_index) const noexcept {
            if (track_index < 0) return -1;
            if (library_shows_group_rows()) {
                const auto key = library_group_title_for_track(track_index);
                for (std::size_t i = 0; i < list_order.size(); ++i) {
                    if (compare_ci(library_group_title_for_track(list_order[i]), key) == 0) {
                        return static_cast<int>(i);
                    }
                }
                return -1;
            }
            for (std::size_t i = 0; i < list_order.size(); ++i) {
                if (list_order[i] == track_index) {
                    return static_cast<int>(i);
                }
            }
            return -1;
        }

        static const char* list_view_text(const void* ctx, std::uint16_t index) noexcept {
            auto* self = static_cast<const PlayerController*>(ctx);
            if (!self) return "";
            const auto row = self->make_library_row_recipe(static_cast<int>(index));
            if (row.model) {
                return row.title_c_str();
            }
            const int track_index = self->list_index_to_track(static_cast<int>(index));
            if (track_index < 0) return "";
            const auto* titles = self->storage.track_titles;
            if (!titles || track_index >= static_cast<int>(titles->size())) return "";
            return (*titles)[static_cast<std::size_t>(track_index)].c_str();
        }

        static const char* list_view_subtitle(const void* ctx, std::uint16_t index) noexcept {
            auto* self = static_cast<const PlayerController*>(ctx);
            if (!self) return "";
            const auto row = self->make_library_row_recipe(static_cast<int>(index));
            if (row.model) {
                return row.subtitle_c_str();
            }
            const int track_index = self->list_index_to_track(static_cast<int>(index));
            if (track_index < 0) return "";
            const auto* subtitles = self->storage.track_subtitles;
            if (!subtitles || track_index >= static_cast<int>(subtitles->size())) return "";
            return (*subtitles)[static_cast<std::size_t>(track_index)].c_str();
        }

        static const char* list_view_tail(const void* ctx, std::uint16_t index) noexcept {
            auto* self = static_cast<const PlayerController*>(ctx);
            if (!self) return "";
            const auto row = self->make_library_row_recipe(static_cast<int>(index));
            if (row.group_row) {
                if (row.selected_row) {
                    return self->library_group_tail_hint_label();
                }
                if (row.current_row) {
                    return self->library_current_group_state_text();
                }
            } else if (row.current_row) {
                return self->library_current_group_state_text();
            }
            if (row.model) {
                return row.tail_c_str();
            }
            const int track_index = self->list_index_to_track(static_cast<int>(index));
            if (track_index < 0) return "";
            const auto* tracks = self->storage.tracks;
            if (!tracks || track_index >= static_cast<int>(tracks->size())) return "";
            return format_from_path((*tracks)[static_cast<std::size_t>(track_index)].view()).data();
        }

        static std::uint8_t list_view_row_flags(const void* ctx, std::uint16_t index) noexcept {
            auto* self = static_cast<const PlayerController*>(ctx);
            if (!self) return 0;
            const auto row = self->make_library_row_recipe(static_cast<int>(index));
            std::uint8_t flags = 0;
            if (row.group_row) {
                flags |= ::ui::scene::kListViewRowFlagGroup;
            }
            return flags;
        }

        static ::ui::scene::ImageId list_view_tail_action_icon(const void* ctx, std::uint16_t index) noexcept {
            auto* self = static_cast<const PlayerController*>(ctx);
            if (!self) return ::ui::scene::invalid_image_id();
            const auto row = self->make_library_row_recipe(static_cast<int>(index));
            return row.tail_action_icon;
        }

        static ::ui::scene::ImageId list_view_tail_icon(const void* ctx, std::uint16_t index) noexcept {
            auto* self = static_cast<PlayerController*>(const_cast<void*>(ctx));
            if (!self) return ::ui::scene::invalid_image_id();
            const auto row = self->make_library_row_recipe(static_cast<int>(index));
            return row.tail_icon;
        }

        static ::ui::scene::ImageId list_view_icon(const void* ctx, std::uint16_t index) noexcept {
            auto* self = static_cast<PlayerController*>(const_cast<void*>(ctx));
            if (!self) return ::ui::scene::invalid_image_id();
            const auto row = self->make_library_row_recipe(static_cast<int>(index));
            const bool is_active = row.current_row && !row.group_row;
            if (row.track_path.empty()) {
                return ::ui::scene::invalid_image_id();
            }
            if (!row.prefer_cover) {
                return row.fallback_icon;
            }
            const auto cover_id = self->resolve_list_cover_icon(row.track_path, row.cover_path);
            if (::ui::scene::image_id_valid(cover_id)) return cover_id;
            if (is_active) {
                return row.tail_icon;
            }
            return row.fallback_icon;
        }

        void clear_list_cover_cache() {
            for (auto& entry : list_cover_cache) {
                if (::ui::scene::image_id_valid(entry.image.image_id)) {
                    release_cover_image(entry.image);
                }
                entry.path.clear();
            }
            list_cover_next = 0;
        }

        bool resolve_cover_path_for_track(std::string_view track_path, FixedString<260>& out_path) const {
            if (track_path.empty()) return false;
            FixedString<260> folder_path;
            bool has_folder = false;
            if (cover_strategy == CoverStrategy::folder_only
                || cover_strategy == CoverStrategy::folder_first) {
                has_folder = fs_utils::find_cover_for_track(track_path, folder_path);
            }
            switch (cover_strategy) {
            case CoverStrategy::embedded_only:
                out_path.assign(track_path);
                break;
            case CoverStrategy::folder_only:
                out_path.assign(has_folder ? folder_path.view() : "");
                break;
            case CoverStrategy::folder_first:
                out_path.assign(has_folder ? folder_path.view() : track_path);
                break;
            case CoverStrategy::embedded_first:
            default:
                out_path.assign(track_path);
                break;
            }
            return !out_path.empty();
        }

        void build_list_cover_paths() {
            list_cover_paths.clear();
            const auto* tracks = storage.tracks;
            if (!tracks) return;
            const std::size_t count = tracks->size();
            list_cover_paths.reserve(count);
            for (std::size_t i = 0; i < count; ++i) {
                FixedString<260> cover_path;
                const auto& track_path = (*tracks)[i];
                cover_path.assign(track_path.view());
                list_cover_paths.push_back(cover_path);
            }
        }

        ::ui::scene::ImageId resolve_list_cover_icon(std::string_view track_path,
                                                     std::string_view cover_path_view) {
            FixedString<260> cover_path;
            if (!cover_path_view.empty()) {
                cover_path.assign(cover_path_view);
            } else if (!track_path.empty()) {
                cover_path.assign(track_path);
            } else {
                return ::ui::scene::invalid_image_id();
            }
            if (cover_image.path == cover_path.view()
                && ::ui::scene::image_id_valid(cover_image.image_id)) {
                return cover_image.image_id;
            }
            for (auto& entry : list_cover_cache) {
                if (entry.path.view() == cover_path.view()
                    && ::ui::scene::image_id_valid(entry.image.image_id)) {
                    return entry.image.image_id;
                }
            }
            if (::ui::gfx::image_registry_locked()) {
                return ::ui::scene::invalid_image_id();
            }
            auto& slot = list_cover_cache[list_cover_next % kListCoverCache];
            list_cover_next += 1;
            if (::ui::scene::image_id_valid(slot.image.image_id)) {
                release_cover_image(slot.image);
            }
            slot.path.assign(cover_path.view());
            if (load_cover_image(cover_path.view(), slot.image)) {
                return slot.image.image_id;
            }
            slot.path.clear();
            return ::ui::scene::invalid_image_id();
        }

        void refresh_track_labels() {
            const auto* labels = storage.track_labels;
            if (!labels) return;
            for (std::size_t i = 0; i < labels->size(); ++i) {
                player::font_cache::ensure_text((*labels)[i].view());
            }
        }

        void rebuild_list_order() {
            list_order.clear();
            const auto* tracks = storage.tracks;
            if (!tracks) return;
            const std::size_t count = tracks->size();
            list_order.reserve(count);
            if (library_shows_group_rows()) {
                for (std::size_t i = 0; i < count; ++i) {
                    const auto key = library_group_title_for_track(static_cast<int>(i));
                    bool exists = false;
                    for (const int rep_index : list_order) {
                        if (compare_ci(library_group_title_for_track(rep_index), key) == 0) {
                            exists = true;
                            break;
                        }
                    }
                    if (!exists) {
                        list_order.push_back(static_cast<int>(i));
                    }
                }
            } else {
                for (std::size_t i = 0; i < count; ++i) {
                    if (library_context_matches_track(static_cast<int>(i))) {
                        list_order.push_back(static_cast<int>(i));
                    }
                }
            }
            if (list_order.size() <= 1) {
                rebuild_list_display_cache();
                return;
            }
            auto cmp = [&](int a, int b) noexcept {
                if (list_shuffle_enabled) {
                    const auto ka = library_shuffle_key(a, list_shuffle_seed);
                    const auto kb = library_shuffle_key(b, list_shuffle_seed);
                    if (ka != kb) return ka < kb;
                    return a < b;
                }

                if (library_shows_group_rows()) {
                    const auto left = library_group_title_for_track(a);
                    const auto right = library_group_title_for_track(b);
                    const int res = compare_ci(left, right);
                    if (res == 0) return a < b;
                    return (list_sort == ListSort::NameAsc) ? (res < 0) : (res > 0);
                }

                const auto left = make_library_sort_key(a);
                const auto right = make_library_sort_key(b);
                int res = compare_ci(left.primary, right.primary);
                if (res == 0) res = compare_ci(left.secondary, right.secondary);
                if (res == 0) res = compare_ci(left.tertiary, right.tertiary);
                if (res == 0) {
                    if (a < b) res = -1;
                    else if (a > b) res = 1;
                }
                return (list_sort == ListSort::NameAsc) ? (res < 0) : (res > 0);
            };
            std::sort(list_order.begin(), list_order.end(), cmp);
            rebuild_list_display_cache();
        }

        bool sync_list_selection() {
            if (!access.valid() || !handles.list) return false;
            const auto* labels = storage.track_labels;
            if (!labels || track_index < 0 || track_index >= static_cast<int>(labels->size())) {
                access.set_list_view_active(handles.list, -1);
                return false;
            }
            ignore_list_select = true;
            const int list_index = track_index_to_list(track_index);
            access.set_list_view_active(handles.list, list_index);
            if (list_index >= 0) {
                access.set_list_view_selected(handles.list, list_index);
                last_list_selected = list_index;
                ignore_list_select = false;
                return true;
            }
            ignore_list_select = false;
            return false;
        }

        bool restore_list_selection_from_track(int preferred_track) {
            if (!access.valid() || !handles.list) return false;
            const int list_index = track_index_to_list(preferred_track);
            if (list_index < 0) return false;
            ignore_list_select = true;
            access.set_list_view_selected(handles.list, list_index);
            ignore_list_select = false;
            last_list_selected = list_index;
            return true;
        }

        void ensure_library_context_selection() {
            if (!access.valid() || !handles.list) return;
            if (!library_has_context() || list_order.empty()) return;
            if (track_index_to_list(track_index) >= 0) return;
            ignore_list_select = true;
            access.set_list_view_selected(handles.list, 0);
            ignore_list_select = false;
            last_list_selected = 0;
            update_library_context_watermark();
        }

        void refresh_list_view() {
            if (!access.valid() || !handles.list) return;
            const int preferred_selected_track = list_index_to_track(last_list_selected);
            rebuild_list_order();
            const int count = static_cast<int>(list_order.size());
            access.set_list_view_source(handles.list,
                                         static_cast<std::uint16_t>(count),
                                         this,
                                         &PlayerController::list_view_text);
            access.set_list_view_subtitle_source(handles.list, this, &PlayerController::list_view_subtitle);
            access.set_list_view_tail_source(handles.list, this, &PlayerController::list_view_tail);
            access.set_list_view_row_flags_source(handles.list, this, &PlayerController::list_view_row_flags);
            access.set_list_view_tail_icon_source(handles.list, this, &PlayerController::list_view_tail_icon, 20);
            access.set_list_view_tail_action_icon_source(handles.list, this, &PlayerController::list_view_tail_action_icon, 26);
            const bool artist_group_rows = library_shows_group_rows() && library_tab == LibraryTab::Artists;
            access.set_list_view_icon_source(handles.list, this, &PlayerController::list_view_icon, artist_group_rows ? 40 : 48);
            access.set_list_view_icon_corner_radius(handles.list, artist_group_rows ? 20 : 16);
            access.set_list_row_height(handles.list, kLibraryListRowHeight);
            access.set_scroll_step(handles.list, kLibraryListRowHeight);
            if (count > 0) {
                const bool synced = sync_list_selection();
                if (!synced
                    && !restore_list_selection_from_track(preferred_selected_track)
                    && library_has_context()) {
                    ensure_library_context_selection();
                }
            } else {
                access.set_list_view_active(handles.list, -1);
                last_list_selected = -1;
            }
            update_list_title();
            update_list_path();
            update_list_sort_label();
            update_list_shuffle_label();
            update_list_placeholder();
            update_library_state_visuals();
            update_library_context_watermark();
        }

        void invoke_list_action(int list_index) {
            if (list_index < 0 || list_index >= static_cast<int>(list_order.size())) return;
            const auto row = make_library_row_recipe(list_index);
            if (row.group_row) {
                activate_list_index(list_index);
                return;
            }
            open_list_action_menu(list_index);
        }

        void set_library_tab(LibraryTab tab) {
            close_list_action_menu();
            if (library_tab == tab) {
                if (library_has_context()) {
                    library_context_key.clear();
                    refresh_list_view();
                    show_library_view_status();
                    return;
                }
                update_library_state_visuals();
                show_library_view_status();
                return;
            }
            library_tab = tab;
            library_context_key.clear();
            refresh_list_view();
            show_library_view_status();
        }

        bool set_library_context_for_preview(std::string_view key) {
            close_list_action_menu();
            if (library_tab == LibraryTab::Songs) return false;
            if (key.empty()) return false;
            const auto* tracks = storage.tracks;
            if (!tracks || tracks->size() == 0) return false;
            for (std::size_t i = 0; i < tracks->size(); ++i) {
                if (compare_ci(library_group_title_for_track(static_cast<int>(i)), key) == 0) {
                    library_context_key.assign(library_group_title_for_track(static_cast<int>(i)));
                    refresh_list_view();
                    ensure_library_context_selection();
                    show_library_context_status();
                    return true;
                }
            }
            return false;
        }

        bool open_first_library_group_for_preview() {
            close_list_action_menu();
            if (library_tab == LibraryTab::Songs) return false;
            if (!library_shows_group_rows()) return false;
            if (list_order.empty()) return false;
            const auto key = library_group_title_for_track(list_order.front());
            if (key.empty()) return false;
            library_context_key.assign(key);
            refresh_list_view();
            ensure_library_context_selection();
            show_library_context_status();
            return true;
        }

        bool set_library_selected_index_for_preview(int list_index) {
            close_list_action_menu();
            if (!access.valid() || !handles.list) return false;
            if (list_index < 0 || list_index >= static_cast<int>(list_order.size())) return false;
            ignore_list_select = true;
            access.set_list_view_selected(handles.list, list_index);
            ignore_list_select = false;
            last_list_selected = list_index;
            char buf[224]{};
            if (format_library_row_selection_status(list_index, buf, sizeof(buf))) {
                set_status(buf);
            } else {
                show_library_view_status();
            }
            update_library_context_watermark();
            return true;
        }

        void toggle_library_shuffle() {
            close_list_action_menu();
            list_shuffle_enabled = !list_shuffle_enabled;
            if (list_shuffle_enabled) {
                auto seed = static_cast<std::uint32_t>(charm::system::ClockCaps::TimeSource::now());
                if (seed == 0) seed = 0x5A17D3C1u;
                list_shuffle_seed = seed;
            }
            refresh_list_view();
            show_library_view_status();
        }

        void reset_library_context() {
            close_list_action_menu();
            if (library_has_context()) {
                library_context_key.clear();
                refresh_list_view();
                show_library_view_status();
                return;
            }
            if (list_shuffle_enabled) {
                list_shuffle_enabled = false;
                refresh_list_view();
                show_library_view_status();
                return;
            }
            if (library_tab != LibraryTab::Songs) {
                library_tab = LibraryTab::Songs;
                refresh_list_view();
                show_library_view_status();
                return;
            }
            update_library_state_visuals();
            update_list_path();
            show_library_view_status();
        }

        void apply_storage_view(StorageView view) {
            close_list_action_menu();
            storage = view;
            fs_ready = storage.fs_ready;
            mount_status.assign(storage.mount_status);
            clear_list_cover_cache();
            reset_track_duration_cache();
            refresh_track_labels();
            build_list_cover_paths();
            sync_home_collage_images(true);
            if (fs_ready && !font_retry_done && !font_ttf_path.empty()) {
                rebind_player_fonts();
                font_retry_done = true;
            }
            if (!storage.status.empty()) { set_status(storage.status.data()); }
#if defined(CHARM_PLAYER_COVER_DEBUG)
            if (player::font_cache::ready()) {
                set_status("FontCache: OK");
            } else {
                set_status("FontCache: OFF");
            }
#endif
            refresh_list_view();
            update_list_placeholder();
            if (fs_ready && storage.tracks && storage.tracks->size() > 0
                && !playback.playing() && !playback.paused()
                && !track_preloaded) {
                load_track_index(track_index);
            }
        }

        void update_list_title() {
            if (!access.valid()) return;
            const int count = static_cast<int>(list_order.size());
            const std::string_view context = library_has_context()
                ? library_context_key.view()
                : std::string_view{};
            const char* base = library_tab_title(library_tab);
            if (!context.empty()) {
                if (last_list_title_text.view() == context) return;
                last_list_title_text.assign(context);
                set_label_slot(handles.list_title, text_slots.list_title, last_list_title_text.c_str());
            } else {
                if (last_list_title_text.view() == base) return;
                last_list_title_text.assign(base);
                set_label_slot(handles.list_title, text_slots.list_title, base);
            }
            last_list_count = count;
        }

        void update_list_path() {
            if (!access.valid() || !handles.list_path) return;
            FixedString<256> path;
            const bool can_step_back = library_can_step_back();
            if (can_step_back) {
                path.assign("< ");
                path.append(library_back_target_label());
            } else {
                path.assign("Library");
            }
            set_label_slot(handles.list_path, text_slots.list_path, path.c_str());
        }

        void update_list_sort_label() {
            if (!access.valid() || !handles.list_sort) return;
            const char* label = list_shuffle_enabled
                ? "Rand"
                : ((list_sort == ListSort::NameAsc) ? "A-Z" : "Z-A");
            set_label_slot(handles.list_sort, text_slots.list_sort, label);
        }

        void update_list_shuffle_label() {
            if (!access.valid() || !handles.list_shuffle) return;
            const char* label = list_shuffle_enabled ? "Shuffle On" : "Shuffle";
            set_label_slot(handles.list_shuffle, text_slots.list_shuffle, label);
        }

        void update_list_placeholder() {
            if (!access.valid() || !handles.list_hint) return;
            const int count = static_cast<int>(list_order.size());
            const bool show_empty = (count == 0);
            const bool show_group_hint = !show_empty && library_shows_group_rows();
            const bool show_context_hint = !show_empty && library_has_context() && library_can_step_back();
            const bool show_tracks_hint = !show_empty
                && !library_has_context()
                && library_tab == LibraryTab::Songs;
            const bool show = show_empty || show_group_hint || show_context_hint || show_tracks_hint;
            const auto apply_hint_style = [&](rgba color,
                                              FontWeight weight,
                                              int px) noexcept {
                StylePatch patch{};
                patch.has_font = true;
                patch.font = &get_player_font_px(px, weight);
                patch.has_font_color = true;
                patch.font_color = color;
                access.set_style_override(handles.list_hint, patch);
            };
            access.set_visible(handles.list_hint, show);
            if (!show) return;
            if (show_empty && !mount_status.empty()) {
                apply_hint_style(with_alpha(kUiSubtitle, 230), FontWeight::Medium, 14);
                if (last_list_hint_text.view() == mount_status.view()) return;
                last_list_hint_text.assign(mount_status.view());
                set_label_slot(handles.list_hint, text_slots.list_hint, mount_status.c_str());
                return;
            }
            const char* hint = "";
            char buf[160]{};
            if (show_group_hint) {
                apply_hint_style(with_alpha(kUiLibraryPathTextActive, 214), FontWeight::Medium, 14);
                const char* label = (library_tab == LibraryTab::Albums)
                    ? ((count == 1) ? "album" : "albums")
                    : ((count == 1) ? "artist" : "artists");
                std::snprintf(buf, sizeof(buf), "%d %s available | Select an %s to browse tracks",
                              count, label, library_group_object_label());
                char path_hint_buf[80]{};
                if (format_library_path_bar_hint(path_hint_buf, sizeof(path_hint_buf))) {
                    append_library_status_suffix(buf, sizeof(buf), path_hint_buf);
                }
                if (list_shuffle_enabled) {
                    append_library_status_suffix(buf, sizeof(buf), "Shuffled");
                }
                hint = buf;
            } else if (show_context_hint) {
                char summary_buf[96]{};
                char path_hint_buf[80]{};
                format_library_group_summary(
                    library_context_key.view(),
                    library_context_is_current_group(),
                    summary_buf,
                    sizeof(summary_buf));
                const bool has_path_hint = format_library_path_bar_hint(path_hint_buf, sizeof(path_hint_buf));
                if (summary_buf[0] != '\0') {
                    apply_hint_style(with_alpha(kUiSubtitle, 236), FontWeight::Medium, 14);
                    if (has_path_hint) {
                        std::snprintf(buf, sizeof(buf), "%s | %s", summary_buf, path_hint_buf);
                        hint = buf;
                    } else {
                        hint = summary_buf;
                    }
                } else if (has_path_hint) {
                    apply_hint_style(with_alpha(kUiLibraryPathTextActive, 188), FontWeight::Medium, 14);
                    hint = path_hint_buf;
                }
            } else if (show_tracks_hint) {
                apply_hint_style(with_alpha(kUiSubtitle, 230), FontWeight::Medium, 14);
                const char* track_label = (count == 1) ? "track" : "tracks";
                if (list_shuffle_enabled) {
                    std::snprintf(buf, sizeof(buf), "%d %s available | Shuffled",
                                  count, track_label);
                } else {
                    std::snprintf(buf, sizeof(buf), "%d %s available",
                                  count, track_label);
                }
                char path_hint_buf[80]{};
                if (format_library_path_bar_hint(path_hint_buf, sizeof(path_hint_buf))) {
                    append_library_status_suffix(buf, sizeof(buf), path_hint_buf);
                }
                hint = buf;
            } else {
                apply_hint_style(with_alpha(kUiSubtitle, 236),
                                 !fs_ready ? FontWeight::Medium : FontWeight::Regular,
                                 14);
                if (!fs_ready) {
                    hint = "FS not ready";
                } else if (library_has_context()) {
                    hint = (library_tab == LibraryTab::Albums)
                        ? "No tracks in this album"
                        : "No tracks by this artist";
                } else {
                    switch (library_tab) {
                    case LibraryTab::Albums:
                        hint = "No albums in library";
                        break;
                    case LibraryTab::Artists:
                        hint = "No artists in library";
                        break;
                    case LibraryTab::Songs:
                    default:
                        hint = "No tracks in library";
                        break;
                    }
                }
            }
            if (last_list_hint_text.view() == hint) return;
            last_list_hint_text.assign(hint);
            set_label_slot(handles.list_hint, text_slots.list_hint, hint);
        }
