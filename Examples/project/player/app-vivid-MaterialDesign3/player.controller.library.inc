        void refresh_library() {
            sync_page_chrome(PlayerPage::Library);
            if (list_info_popup_open()) {
                set_mini_bar_chrome_visible(false);
            }
            refresh_library_body_content();
            update_debug_overlay();
        }

        static int compare_ci(std::string_view a, std::string_view b) noexcept {
            return compare_media_text_ci(a, b);
        }

        static constexpr int kLibraryTabRadius = 20;
        static constexpr int kLibrarySortRadius = 20;
        static constexpr int kLibraryPathRadius = 18;
        static constexpr int kLibraryListRowHeight = 72;
        static constexpr int kTrackDurationUnknown = -1;
        static constexpr int kTrackDurationUnavailable = -2;
        static constexpr std::uint64_t kListDurationProbeIntervalMs = 120;

        enum class LibraryCoverMode : std::uint8_t {
            ListCovers,
            FocusedPreviewOnly,
            IconsOnly,
        };

        // TODO(player-ui): expose LibraryCoverMode through the future settings page
        // instead of keeping it as a compile-time page-local switch.
        static constexpr LibraryCoverMode kLibraryCoverMode =
            LibraryCoverMode::FocusedPreviewOnly;

        static void format_bytes_compact(std::uint64_t bytes,
                                         char* out,
                                         std::size_t out_size) noexcept {
            if (!out || out_size == 0) return;
            out[0] = '\0';
            if (bytes == 0) return;
            static constexpr const char* kUnits[] = {"B", "KB", "MB", "GB"};
            double value = static_cast<double>(bytes);
            std::size_t unit_index = 0;
            while (value >= 1024.0 && unit_index + 1 < std::size(kUnits)) {
                value /= 1024.0;
                ++unit_index;
            }
            if (unit_index == 0) {
                std::snprintf(out, out_size, "%llu %s",
                              static_cast<unsigned long long>(bytes),
                              kUnits[unit_index]);
            } else if (value >= 10.0) {
                std::snprintf(out, out_size, "%.0f %s", value, kUnits[unit_index]);
            } else {
                std::snprintf(out, out_size, "%.1f %s", value, kUnits[unit_index]);
            }
        }

        struct LibrarySortKey {
            std::string_view primary{};
            std::string_view secondary{};
            std::string_view tertiary{};
        };

        static std::string_view library_file_name(std::string_view path) noexcept {
            return media_file_name(path);
        }

        static std::string_view library_track_stem(std::string_view path) noexcept {
            return media_track_stem(path);
        }

        static std::string_view library_parent_folder(std::string_view path) noexcept {
            return media_parent_folder(path);
        }

        static std::string_view library_directory_path(std::string_view path) noexcept {
            return media_directory_path(path);
        }

        static bool library_starts_with_bytes(std::string_view value,
                                              unsigned char a,
                                              unsigned char b) noexcept {
            return value.size() >= 2
                && static_cast<unsigned char>(value[0]) == a
                && static_cast<unsigned char>(value[1]) == b;
        }

        static bool library_starts_with_bytes(std::string_view value,
                                              unsigned char a,
                                              unsigned char b,
                                              unsigned char c) noexcept {
            return value.size() >= 3
                && static_cast<unsigned char>(value[0]) == a
                && static_cast<unsigned char>(value[1]) == b
                && static_cast<unsigned char>(value[2]) == c;
        }

        static bool library_ends_with_bytes(std::string_view value,
                                            unsigned char a,
                                            unsigned char b) noexcept {
            return value.size() >= 2
                && static_cast<unsigned char>(value[value.size() - 2]) == a
                && static_cast<unsigned char>(value[value.size() - 1]) == b;
        }

        static bool library_ends_with_bytes(std::string_view value,
                                            unsigned char a,
                                            unsigned char b,
                                            unsigned char c) noexcept {
            return value.size() >= 3
                && static_cast<unsigned char>(value[value.size() - 3]) == a
                && static_cast<unsigned char>(value[value.size() - 2]) == b
                && static_cast<unsigned char>(value[value.size() - 1]) == c;
        }

        static bool library_trim_prefix_once(std::string_view& value) noexcept {
            if (value.empty()) return false;
            const unsigned char front = static_cast<unsigned char>(value.front());
            if (front == 0 || std::isspace(front)) {
                value.remove_prefix(1);
                return true;
            }
            if (library_starts_with_bytes(value, 0xC2u, 0xA0u)) {
                value.remove_prefix(2);
                return true;
            }
            if (library_starts_with_bytes(value, 0xE3u, 0x80u, 0x80u)
                || library_starts_with_bytes(value, 0xEFu, 0xBBu, 0xBFu)) {
                value.remove_prefix(3);
                return true;
            }
            return false;
        }

        static bool library_trim_suffix_once(std::string_view& value) noexcept {
            if (value.empty()) return false;
            const unsigned char back = static_cast<unsigned char>(value.back());
            if (back == 0 || std::isspace(back)) {
                value.remove_suffix(1);
                return true;
            }
            if (library_ends_with_bytes(value, 0xC2u, 0xA0u)) {
                value.remove_suffix(2);
                return true;
            }
            if (library_ends_with_bytes(value, 0xE3u, 0x80u, 0x80u)
                || library_ends_with_bytes(value, 0xEFu, 0xBBu, 0xBFu)) {
                value.remove_suffix(3);
                return true;
            }
            return false;
        }

        static std::string_view library_trim_space(std::string_view value) noexcept {
            while (library_trim_prefix_once(value)) {}
            while (library_trim_suffix_once(value)) {}
            return value;
        }

        static std::string_view library_popup_directory_segment_label(
            std::string_view value) noexcept {
            value = library_trim_space(value);
            if (compare_ci(value, "Storage root") == 0) {
                return "Library root";
            }
            return value;
        }

        static std::size_t utf8_codepoint_span(std::string_view value, std::size_t offset) noexcept {
            if (offset >= value.size()) return 0;
            const unsigned char lead = static_cast<unsigned char>(value[offset]);
            if ((lead & 0x80u) == 0u) return 1;
            if ((lead & 0xE0u) == 0xC0u && offset + 1 < value.size()) return 2;
            if ((lead & 0xF0u) == 0xE0u && offset + 2 < value.size()) return 3;
            if ((lead & 0xF8u) == 0xF0u && offset + 3 < value.size()) return 4;
            return 1;
        }

        template <std::size_t Capacity>
        static void utf8_truncate_with_ellipsis_to(FixedString<Capacity>& out,
                                                   std::string_view value,
                                                   std::size_t max_codepoints) noexcept {
            out.clear();
            value = library_trim_space(value);
            if (value.empty()) return;
            if (max_codepoints == 0) {
                out.assign(value);
                return;
            }

            std::size_t offset = 0;
            std::size_t codepoints = 0;
            while (offset < value.size() && codepoints < max_codepoints) {
                offset += utf8_codepoint_span(value, offset);
                ++codepoints;
            }
            if (offset >= value.size()) {
                out.assign(value);
                return;
            }

            out.assign(value.substr(0, offset));
            (void)out.append("...");
        }

        template <std::size_t Capacity>
        static void append_library_preview_part(FixedString<Capacity>& out,
                                                std::string_view value,
                                                std::size_t max_codepoints = 0) noexcept {
            value = library_trim_space(value);
            if (value.empty()) return;
            FixedString<Capacity> compact{};
            utf8_truncate_with_ellipsis_to(compact, value, max_codepoints);
            if (compact.empty()) return;
            if (!out.empty()) {
                out += " · ";
            }
            out += compact;
        }

        template <std::size_t Capacity>
        static void library_preview_compact_summary_to(FixedString<Capacity>& out,
                                                       std::string_view value,
                                                       std::size_t max_codepoints) noexcept {
            out.clear();
            value = library_trim_space(value);
            if (value.empty()) return;

            FixedString<Capacity> compact{};
            for (std::size_t i = 0; i < value.size();) {
                if (i + 2 < value.size()
                    && value[i] == ' '
                    && (value[i + 1] == '|' || value[i + 1] == '/')
                    && value[i + 2] == ' ') {
                    compact += " · ";
                    i += 3;
                    continue;
                }
                const char ch[2]{value[i], '\0'};
                (void)compact.append(std::string_view{ch, 1});
                ++i;
            }
            utf8_truncate_with_ellipsis_to(out, compact.view(), max_codepoints);
        }

        static bool library_tail_looks_like_duration(std::string_view tail) noexcept {
            return tail.find(':') != std::string_view::npos
                && std::find_if(tail.begin(), tail.end(), [](char ch) {
                       return std::isdigit(static_cast<unsigned char>(ch)) != 0;
                   }) != tail.end();
        }

        template <std::size_t Capacity>
        static void library_compact_popup_directory_to(FixedString<Capacity>& out,
                                                       std::string_view path,
                                                       std::size_t keep_segments = 4,
                                                       std::size_t max_codepoints = 52) noexcept {
            out.clear();
            path = library_trim_space(path);
            if (path.empty()) {
                out.assign("Library root");
                return;
            }

            std::array<std::string_view, product_config::library_segment_capacity> segments{};
            std::size_t segment_count = 0;
            std::size_t start = 0;
            while (start < path.size()) {
                while (start < path.size() && (path[start] == '/' || path[start] == '\\')) {
                    ++start;
                }
                if (start >= path.size()) break;
                std::size_t end = start;
                while (end < path.size() && path[end] != '/' && path[end] != '\\') {
                    ++end;
                }
                if (segment_count < segments.size()) {
                    segments[segment_count++] = path.substr(start, end - start);
                } else {
                    for (std::size_t i = 1; i < segments.size(); ++i) {
                        segments[i - 1] = segments[i];
                    }
                    segments[segments.size() - 1] = path.substr(start, end - start);
                }
                start = end;
            }

            if (segment_count == 0) {
                utf8_truncate_with_ellipsis_to(out, path, max_codepoints);
                return;
            }

            const std::size_t begin =
                (segment_count > keep_segments) ? (segment_count - keep_segments) : 0;
            FixedString<Capacity> compact{};
            if (begin > 0) {
                compact.assign("...");
            }
            for (std::size_t i = begin; i < segment_count; ++i) {
                const auto label = library_popup_directory_segment_label(segments[i]);
                if (!compact.empty()) {
                    compact += " / ";
                }
                (void)compact.append(label);
            }
            utf8_truncate_with_ellipsis_to(out, compact.view(), max_codepoints);
        }

        static const char* library_popup_eyebrow(LibraryTab tab,
                                                 bool has_context,
                                                 bool shuffle_enabled) noexcept {
            switch (tab) {
            case LibraryTab::Albums:
                return has_context ? "Album track" : "Albums";
            case LibraryTab::Artists:
                return has_context ? "Artist track" : "Artists";
            case LibraryTab::Songs:
            default:
                return shuffle_enabled ? "Shuffle pick" : "Track info";
            }
        }

        static const char* library_track_secondary_action_label() noexcept {
            return "Track details";
        }

        static const char* library_popup_path_title() noexcept {
            return "File location";
        }

        static const char* library_popup_close_hint() noexcept {
            return "Tap outside or press Back";
        }

        static const char* library_action_menu_title(LibraryTab tab,
                                                     bool has_context) noexcept {
            switch (tab) {
            case LibraryTab::Albums:
                return has_context ? "Album track" : "Albums";
            case LibraryTab::Artists:
                return has_context ? "Artist track" : "Artists";
            case LibraryTab::Songs:
            default:
                return "Track actions";
            }
        }

        static constexpr ::ui::scene::AnchoredMenuSpec library_action_menu_spec() noexcept {
            return {
                .width = 236,
                .padding_left = 36,
                .padding_right = 14,
                .padding_top = 12,
                .padding_bottom = 14,
                .title_height = 16,
                .title_bottom_gap = 10,
                .item_height = 38,
                .item_gap = 8,
                .anchor_right_inset = 10,
                .anchor_top_offset = 6,
                .viewport_margin = 12,
                .card_style_class = static_cast<StyleClassId>(PlayerStyleClass::LibraryListCard),
                .item_style_class = static_cast<StyleClassId>(PlayerStyleClass::TopBarButton),
            };
        }

        static bool library_split_track_stem(std::string_view stem,
                                             std::string_view& title_out,
                                             std::string_view& artist_out) noexcept {
            std::string_view artist{};
            std::string_view title{};
            if (!split_media_stem_artist_title(stem, artist, title)) return false;
            title_out = library_trim_space(title);
            artist_out = library_trim_space(artist);
            return !title_out.empty() && !artist_out.empty();
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
            std::string_view artist{};
            std::string_view title{};
            if (!split_media_stem_artist_title(stem, artist, title)) return {};
            return library_trim_space(artist);
        }

        static std::string_view library_title_from_stem(std::string_view stem) noexcept {
            std::string_view artist{};
            std::string_view title{};
            if (!split_media_stem_artist_title(stem, artist, title)) return stem;
            return library_trim_space(title);
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

        static constexpr bool library_list_uses_cover_art() noexcept {
            return kLibraryCoverMode == LibraryCoverMode::ListCovers;
        }

        static constexpr bool library_uses_focused_preview_cover() noexcept {
            return kLibraryCoverMode == LibraryCoverMode::FocusedPreviewOnly;
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
                track_duration_cache_sec.clear();
                const std::size_t capped_count =
                    (count < track_duration_cache_sec.capacity()) ? count : track_duration_cache_sec.capacity();
                for (std::size_t i = 0; i < capped_count; ++i) {
                    (void)track_duration_cache_sec.push_back(kTrackDurationUnknown);
                }
                list_duration_probe_cursor = 0;
                last_list_duration_probe_ms = 0;
            }
        }

        int cached_track_duration(int track_index) const noexcept {
            if (track_index < 0) return kTrackDurationUnknown;
            if (track_index >= static_cast<int>(track_duration_cache_sec.size())) return kTrackDurationUnknown;
            return track_duration_cache_sec[static_cast<std::size_t>(track_index)];
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

        void format_library_row_tail(FixedString<product_config::library_row_tail_capacity>& out,
                                     int track_index,
                                     bool group_row) const {
            out.clear();
            const auto* tracks = storage.tracks;
            if (!tracks || track_index < 0 || track_index >= static_cast<int>(tracks->size())) {
                return;
            }
            if (group_row) {
                return;
            }
            const auto path = (*tracks)[static_cast<std::size_t>(track_index)].view();
            const int duration_sec = cached_track_duration(track_index);
            if (duration_sec > 0) {
                char buf[16]{};
                format_duration_compact(duration_sec, buf, sizeof(buf));
                out.assign(buf);
            } else if (duration_sec == kTrackDurationUnknown) {
                out.assign("--:--");
            } else {
                out.assign(format_from_path(path));
            }
            player::font_cache::ensure_text(out.view());
        }

        void format_library_row_text(int row_index,
                                     int track_index,
                                     bool group_row) const noexcept {
            list_row_title_scratch.clear();
            list_row_subtitle_scratch.clear();
            list_row_tail_scratch.clear();

            const auto* tracks = storage.tracks;
            if (!tracks || track_index < 0 || track_index >= static_cast<int>(tracks->size())) {
                list_row_title_scratch.assign("Unknown Track");
                list_row_subtitle_scratch.assign("Track");
                player::font_cache::ensure_text(list_row_title_scratch.view());
                player::font_cache::ensure_text(list_row_subtitle_scratch.view());
                return;
            }

            const auto path = (*tracks)[static_cast<std::size_t>(track_index)].view();
            const auto media = derive_media_track(path);
            const std::string_view stem = media.stem;
            const std::string_view artist = media.has_artist ? media.artist : std::string_view{};
            const std::string_view title = media.title;
            const std::string_view album = media.album;
            std::string_view display_title{};
            std::string_view display_subtitle{};

            if (group_row) {
                display_title = library_group_title_for_track(track_index);
                char buf[64]{};
                format_library_group_summary(display_title,
                                             library_group_row_is_current(row_index),
                                             buf,
                                             sizeof(buf),
                                             false);
                list_row_title_scratch.assign(display_title.empty() ? std::string_view{"Library"} : display_title);
                list_row_subtitle_scratch.assign(buf);
                player::font_cache::ensure_text(list_row_title_scratch.view());
                player::font_cache::ensure_text(list_row_subtitle_scratch.view());
                return;
            }

            switch (library_tab) {
            case LibraryTab::Songs:
                display_title = title.empty() ? stem : title;
                if (!artist.empty() && !album.empty()) {
                    char subtitle_buf[192]{};
                    std::snprintf(subtitle_buf, sizeof(subtitle_buf), "%.*s / %.*s",
                                  static_cast<int>(artist.size()), artist.data(),
                                  static_cast<int>(album.size()), album.data());
                    list_row_subtitle_scratch.assign(subtitle_buf);
                    display_subtitle = list_row_subtitle_scratch.view();
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
            list_row_title_scratch.assign(display_title);
            if (list_row_subtitle_scratch.empty()) {
                list_row_subtitle_scratch.assign(display_subtitle);
            }
            format_library_row_tail(list_row_tail_scratch, track_index, false);
            player::font_cache::ensure_text(list_row_title_scratch.view());
            player::font_cache::ensure_text(list_row_subtitle_scratch.view());
        }

        LibraryRowRecipe make_library_row_recipe(int row_index) const noexcept {
            LibraryRowRecipe out{};
            out.row_index = row_index;
            out.track_index = list_index_to_track(row_index);
            out.group_row = library_shows_group_rows()
                && row_index >= 0
                && row_index < static_cast<int>(list_order.size());
            out.selected_row = (row_index == last_list_selected);
            out.menu_title_cstr = out.group_row ? "Group" : "Track";
            if (out.group_row) {
                out.current_row = library_group_row_is_current(row_index);
            } else if (track_index >= 0 && out.track_index >= 0) {
                out.current_row = (out.track_index == track_index);
            }
            format_library_row_text(row_index, out.track_index, out.group_row);
            out.title_text = list_row_title_scratch.view();
            out.subtitle_text = list_row_subtitle_scratch.view();
            out.tail_text = list_row_tail_scratch.view();
            out.title_cstr = list_row_title_scratch.c_str();
            out.subtitle_cstr = list_row_subtitle_scratch.c_str();
            out.tail_cstr = list_row_tail_scratch.c_str();
            out.text_resolved = true;
            if (!out.title_text.empty()) out.menu_title_cstr = out.title_cstr;
            out.detail_primary = out.title_text;
            out.detail_secondary = out.subtitle_text;

            const bool row_has_action = !out.title_text.empty() || (out.track_index >= 0);
            if (out.group_row) {
                out.show_tail_action = false;
                out.tail_action_icon = ::ui::scene::invalid_image_id();
                out.tail_icon = icons.chevron_right;
            } else {
                out.show_tail_action = row_has_action && out.selected_row;
                out.tail_action_icon = out.show_tail_action ? icons.more : ::ui::scene::invalid_image_id();
                out.tail_icon = ::ui::scene::invalid_image_id();
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
            list_cover_path_scratch.clear();
            if (resolve_cover_path_for_track(out.track_path, list_cover_path_scratch)) {
                out.cover_path = list_cover_path_scratch.view();
            }
            return out;
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
                        notify_list_subtitle_source_changed();
                    } else {
                        notify_list_tail_source_changed();
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
            const auto media = derive_media_track(path);
            switch (tab) {
            case LibraryTab::Albums:
                return media_group_key(media, MediaGroupKind::Albums);
            case LibraryTab::Artists:
                return media_group_key(media, MediaGroupKind::Artists);
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
            if (library_uses_focused_preview_cover()) {
                return !list_order.empty();
            }
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
            FixedString<96> primary_artist{};
            bool multiple_artists = false;
            std::array<FixedString<product_config::library_unique_album_text_capacity>,
                       product_config::library_segment_capacity> unique_albums{};
            std::size_t unique_album_count = 0;

            for (std::size_t i = 0; i < tracks->size(); ++i) {
                if (compare_ci(library_group_title_for_track(static_cast<int>(i)), key) != 0) {
                    continue;
                }

                ++track_count;
                const auto path = (*tracks)[i].view();
                const auto media = derive_media_track(path);
                const int duration_sec = cached_track_duration(static_cast<int>(i));
                if (duration_sec == kTrackDurationUnknown) {
                    duration_complete = false;
                } else if (duration_sec > 0) {
                    total_sec += duration_sec;
                }

                if (library_tab == LibraryTab::Albums) {
                    const auto artist = media_group_key(media, MediaGroupKind::Artists);
                    if (primary_artist.empty()) {
                        primary_artist.assign(artist);
                    } else if (compare_ci(primary_artist.view(), artist) != 0) {
                        multiple_artists = true;
                    }
                } else if (library_tab == LibraryTab::Artists) {
                    const auto album = media_group_key(media, MediaGroupKind::Albums);
                    bool exists = false;
                    for (std::size_t album_index = 0; album_index < unique_album_count; ++album_index) {
                        if (compare_ci(unique_albums[album_index].view(), album) == 0) {
                            exists = true;
                            break;
                        }
                    }
                    if (!exists && unique_album_count < unique_albums.size()) {
                        unique_albums[unique_album_count++].assign(album);
                    }
                }
            }

            album_count = static_cast<int>(unique_album_count);
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

        int library_context_preview_list_index() const noexcept {
            if (list_order.empty()) return -1;
            if (last_list_selected >= 0 && last_list_selected < static_cast<int>(list_order.size())) {
                return last_list_selected;
            }
            const int current_list_index = track_index_to_list(track_index);
            if (current_list_index >= 0
                && current_list_index < static_cast<int>(list_order.size())) {
                return current_list_index;
            }
            return 0;
        }

        bool format_library_context_preview_text(char* title_buf,
                                                 std::size_t title_size,
                                                 char* subtitle_buf,
                                                 std::size_t subtitle_size) const noexcept {
            if (!title_buf || title_size == 0 || !subtitle_buf || subtitle_size == 0) return false;
            title_buf[0] = '\0';
            subtitle_buf[0] = '\0';

            const int list_index = library_context_preview_list_index();
            if (list_index < 0 || list_index >= static_cast<int>(list_order.size())) return false;

            const auto row = make_library_row_recipe(list_index);
            auto primary = row.title();
            auto secondary = row.subtitle();
            if (primary.empty()) primary = row.detail_primary;
            if (secondary.empty()) secondary = row.detail_secondary;
            if (primary.empty()) {
                primary = std::string_view{library_tab_title(library_tab)};
            }
            if (primary.empty()) return false;

            FixedString<192> title_text{};
            utf8_truncate_with_ellipsis_to(title_text, primary, row.group_row ? 24u : 28u);
            std::snprintf(title_buf, title_size, "%s", title_text.c_str());

            FixedString<224> subtitle_text{};
            if (row.group_row) {
                library_preview_compact_summary_to(subtitle_text, secondary, 40);
                if (row.current_row) {
                    append_library_preview_part(
                        subtitle_text,
                        std::string_view{library_current_group_state_text()},
                        12);
                }
            } else {
                const auto* tracks = storage.tracks;
                std::string_view artist{};
                std::string_view album{};
                if (tracks
                    && row.track_index >= 0
                    && row.track_index < static_cast<int>(tracks->size())) {
                    const auto path = (*tracks)[static_cast<std::size_t>(row.track_index)].view();
                    const auto media = derive_media_track(path);
                    artist = media.has_artist ? media.artist : std::string_view{};
                    album = media.album;
                }

                if (!library_looks_like_format(artist)) {
                    switch (library_tab) {
                    case LibraryTab::Songs:
                    case LibraryTab::Albums:
                        append_library_preview_part(subtitle_text, artist, 16);
                        break;
                    case LibraryTab::Artists:
                        break;
                    }
                }
                if (!library_looks_like_format(album)) {
                    switch (library_tab) {
                    case LibraryTab::Songs:
                    case LibraryTab::Artists:
                        append_library_preview_part(subtitle_text, album, 18);
                        break;
                    case LibraryTab::Albums:
                        break;
                    }
                }

                if (subtitle_text.empty()) {
                    library_preview_compact_summary_to(subtitle_text, secondary, 36);
                }

                const auto tail = row.tail();
                if (library_tail_looks_like_duration(tail)) {
                    append_library_preview_part(subtitle_text, tail, 8);
                }
                if (row.current_row) {
                    append_library_preview_part(
                        subtitle_text,
                        std::string_view{is_paused() ? "Paused" : "Playing"},
                        12);
                }
            }

            std::snprintf(subtitle_buf, subtitle_size, "%s", subtitle_text.c_str());
            return true;
        }

        bool library_cover_request_matches_track(std::string_view request_track_path,
                                                 std::string_view request_cover_path,
                                                 int candidate_track_index) const noexcept {
            const auto* tracks = storage.tracks;
            if (!tracks) return false;
            if (candidate_track_index < 0
                || candidate_track_index >= static_cast<int>(tracks->size())) {
                return false;
            }
            const auto candidate_track_path =
                (*tracks)[static_cast<std::size_t>(candidate_track_index)].view();
            FixedString<260> candidate_cover_path_store{};
            std::string_view candidate_cover_path{};
            if (resolve_cover_path_for_track(candidate_track_path, candidate_cover_path_store)) {
                candidate_cover_path = candidate_cover_path_store.view();
            }
            if (!request_track_path.empty()) {
                if (request_track_path == candidate_track_path) return true;
                if (!candidate_cover_path.empty() && request_track_path == candidate_cover_path) return true;
            }
            if (!request_cover_path.empty()) {
                if (request_cover_path == candidate_track_path) return true;
                if (!candidate_cover_path.empty() && request_cover_path == candidate_cover_path) return true;
            }
            return false;
        }

        bool library_allows_cover_request(std::string_view request_track_path,
                                          std::string_view request_cover_path) const noexcept {
            if (!library_uses_focused_preview_cover()) return true;
            if (current_page != PlayerPage::Library) return true;

            const int preview_list_index = library_context_preview_list_index();
            const int preview_track_index = list_index_to_track(preview_list_index);
            if (library_cover_request_matches_track(
                    request_track_path, request_cover_path, preview_track_index)) {
                return true;
            }

            if (list_info_popup_open()
                && library_cover_request_matches_track(
                    request_track_path, request_cover_path, list_info_popup_track_index)) {
                return true;
            }

            return false;
        }

        void clear_library_context_summary() noexcept {
            if (handles.list_context_title) {
                access.set_text(handles.list_context_title, "");
                set_handle_visible(handles.list_context_title, false);
            }
            if (handles.list_context_subtitle) {
                access.set_text(handles.list_context_subtitle, "");
                set_handle_visible(handles.list_context_subtitle, false);
            }
        }

        void set_library_context_summary(const char* title, const char* subtitle) noexcept {
            if (handles.list_context_title) {
                access.set_text(handles.list_context_title, title ? title : "");
                set_handle_visible(handles.list_context_title, title && title[0] != '\0');
            }
            if (handles.list_context_subtitle) {
                access.set_text(handles.list_context_subtitle, subtitle ? subtitle : "");
                set_handle_visible(handles.list_context_subtitle, subtitle && subtitle[0] != '\0');
            }
        }

        void hide_library_context_cover_placeholder() noexcept {
            set_handle_visible(handles.list_context_cover_placeholder, false);
        }

        void show_library_context_cover_placeholder(::ui::scene::ImageId icon) noexcept {
            if (handles.list_context_cover_placeholder) {
                access.set_button_icon(handles.list_context_cover_placeholder,
                                       ::ui::scene::image_id_valid(icon) ? icon : icons.folder);
                set_handle_visible(handles.list_context_cover_placeholder, true);
            }
        }

        ::ui::scene::ImageId library_context_placeholder_icon() const noexcept {
            const int list_index = library_context_preview_list_index();
            if (list_index >= 0 && list_index < static_cast<int>(list_order.size())) {
                const auto row = make_library_row_recipe(list_index);
                if (row.group_row) {
                    return icons.folder_active;
                }
                if (::ui::scene::image_id_valid(row.fallback_icon)
                    && row.fallback_icon != icons.play
                    && row.fallback_icon != icons.pause) {
                    return row.fallback_icon;
                }
            }
            return icons.folder_active;
        }

        void update_library_context_watermark() {
            if (!access.valid()) return;
            const auto hide = [&]() {
                set_handle_visible(handles.list_context_root, false);
                hide_library_context_cover_placeholder();
                clear_library_context_summary();
            };
            if (!library_shows_context_watermark()) {
                hide();
                return;
            }

            set_handle_visible(handles.list_context_root, true);
            char title_buf[192]{};
            char subtitle_buf[224]{};
            if (format_library_context_preview_text(
                    title_buf, sizeof(title_buf),
                    subtitle_buf, sizeof(subtitle_buf))) {
                set_library_context_summary(title_buf, subtitle_buf);
            } else {
                clear_library_context_summary();
            }
            show_library_context_cover_placeholder(library_context_placeholder_icon());
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
            char back_hint_buf[80]{};
            if (format_library_back_status_hint(back_hint_buf, sizeof(back_hint_buf))) {
                append_library_status_suffix(buf, sizeof(buf), back_hint_buf);
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
            char back_hint_buf[80]{};
            if (format_library_back_status_hint(back_hint_buf, sizeof(back_hint_buf))) {
                append_library_status_suffix(buf, sizeof(buf), back_hint_buf);
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

        bool list_info_popup_open() const noexcept {
            return list_info_popup_track_index >= 0;
        }

        void set_list_info_popup_visible(bool visible) noexcept {
            set_handle_visible(handles.list_info_scrim, visible);
            set_handle_visible(handles.list_info_card, visible);
            if (library_page_active()) {
                set_mini_bar_chrome_visible(!visible);
            }
        }

        void hide_list_info_popup_widgets() {
            if (!access.valid()) return;
            set_list_info_popup_visible(false);
            clear_image_slot_if_present(library_info_cover_slot(), false);
        }

        void close_list_info_popup() {
            list_info_popup_track_index = -1;
            hide_list_info_popup_widgets();
            if (access.valid() && handles.list) {
                access.set_focused(handles.list, true);
            }
            if (library_page_active()) {
                sync_page_chrome(PlayerPage::Library);
            }
        }

        void close_list_action_menu() {
            sync_list_action_menu_focus_visual(-1);
            list_action_menu_index = -1;
            list_action_menu_focus_index = -1;
            ::ui::scene::hide_anchored_menu(
                access, list_action_menu_handles(), library_action_menu_spec());
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
                .text = row.group_row ? library_group_secondary_action_label()
                                      : library_track_secondary_action_label(),
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
            const auto roles = player::cover_theme::derive_library_action_menu_color_roles(now_playing_roles);
            const rgba focus_border = roles.item_focus.border.a != 0
                ? roles.item_focus.border
                : with_alpha(roles.item_focus.fg, 48);
            StylePatch patch = ::ui::scene::make_pill_surface_patch({
                .bg_color = roles.item_focus.bg,
                .border_color = focus_border,
                .corner_radius = 12,
                .inner_stroke_enabled = true,
                .inner_stroke_color = with_alpha(roles.item_focus.fg, 40),
                .inner_stroke_width = 1,
            });
            patch.has_font_color = true;
            patch.font_color = roles.item_focus.fg;
            patch.has_border_focus = true;
            patch.border_focus = focus_border;
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
            if (!access.valid()) return list_action_menu_focus_index;
            const int focused = ::ui::scene::anchored_menu_focused_index(
                const_cast<::ui::scene::SceneAccess&>(access),
                list_action_menu_handles());
            return focused >= 0 ? focused : list_action_menu_focus_index;
        }

        void focus_list_action_menu_item(int item_index) {
            list_action_menu_focus_index = item_index;
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

        bool populate_list_info_popup_content(int track_idx) {
            if (!access.valid()) return false;
            const auto* tracks = storage.tracks;
            if (!tracks || track_idx < 0 || track_idx >= static_cast<int>(tracks->size())) return false;

            const auto& track = (*tracks)[static_cast<std::size_t>(track_idx)];
            const auto path = track.view();
            if (path.empty()) return false;

            FixedString<192> title{};
            if (const auto* titles = storage.track_titles;
                titles && track_idx < static_cast<int>(titles->size())) {
                title.assign((*titles)[static_cast<std::size_t>(track_idx)].view());
            }
            if (title.empty()) {
                const auto media = derive_media_track(path);
                title.assign(media.title.empty() ? media.stem : media.title);
            }
            strip_audio_extension(title);

            FixedString<96> artist{};
            std::string_view parsed_title{};
            std::string_view parsed_artist{};
            if (library_split_track_stem(title.view(), parsed_title, parsed_artist)) {
                title.assign(parsed_title);
                artist.assign(parsed_artist);
            }
            if (const auto* subtitles = storage.track_subtitles;
                subtitles && track_idx < static_cast<int>(subtitles->size())) {
                const auto subtitle = (*subtitles)[static_cast<std::size_t>(track_idx)].view();
                if (!subtitle.empty() && !library_looks_like_format(subtitle)) {
                    artist.assign(subtitle);
                }
            }
            if (artist.empty()) {
                const auto media = derive_media_track(path);
                if (media.has_artist && !media.artist.empty()) {
                    artist.assign(media.artist);
                }
            }
            if (!artist.empty() && compare_ci(artist.view(), title.view()) == 0) {
                artist.clear();
            }

            FixedString<96> album{};
            {
                const auto media = derive_media_track(path);
                if (!media.album.empty()) {
                    album.assign(media.album);
                }
            }
            const auto artist_group =
                library_group_title_for_track_in_tab(storage, track_idx, LibraryTab::Artists);

            FixedString<192> subtitle_line{};
            const bool album_context = library_tab == LibraryTab::Albums;
            const bool artist_context = library_tab == LibraryTab::Artists;
            const bool album_looks_distinct_from_title =
                !album.empty() && compare_ci(album.view(), title.view()) != 0;
            const bool artist_looks_distinct_from_title =
                !artist.empty() && compare_ci(artist.view(), title.view()) != 0;
            const bool artist_group_looks_distinct_from_title =
                !artist_group.empty() && compare_ci(artist_group, title.view()) != 0;
            const bool context_key_looks_distinct_from_title =
                !library_context_key.empty() && compare_ci(library_context_key.view(), title.view()) != 0;
            const bool list_title_looks_distinct_from_title =
                !last_list_title_text.empty()
                && compare_ci(last_list_title_text.view(), library_tab_title(library_tab)) != 0
                && compare_ci(last_list_title_text.view(), title.view()) != 0;
            if (artist_context && album_looks_distinct_from_title) {
                subtitle_line.assign(album.view());
            } else if (artist_context && artist_group_looks_distinct_from_title) {
                subtitle_line.assign(artist_group);
            } else if (artist_context && context_key_looks_distinct_from_title) {
                subtitle_line.assign(library_context_key.view());
            } else if (artist_context && list_title_looks_distinct_from_title) {
                subtitle_line.assign(last_list_title_text.view());
            } else if (artist_looks_distinct_from_title) {
                subtitle_line.assign(artist.view());
            } else {
                subtitle_line.assign(artist_context ? "Unknown Album" : "Unknown Artist");
            }

            FixedString<128> meta_line{};
            auto append_meta_part = [&](std::string_view value) {
                if (value.empty()) return;
                if (!meta_line.empty()) meta_line.append(" · ");
                meta_line.append(value);
            };
            if (!album.empty()
                && !album_context
                && !(artist_context && subtitle_line.view() == album.view())) {
                append_meta_part(album.view());
            }
            const auto format = format_from_path(path);
            append_meta_part(format.empty() ? std::string_view{"Audio"} : format);

            int duration_sec = cached_track_duration(track_idx);
            if (duration_sec == kTrackDurationUnknown) {
                int probed_duration = 0;
                if (player::probe_duration_seconds(track.c_str(), probed_duration) && probed_duration > 0) {
                    duration_sec = probed_duration;
                } else {
                    duration_sec = 0;
                }
            } else if (duration_sec < 0) {
                duration_sec = 0;
            }

            char duration_buf[16]{};
            if (duration_sec > 0) {
                format_duration_compact(duration_sec, duration_buf, sizeof(duration_buf));
                append_meta_part(duration_buf);
            }

            char size_buf[24]{};
            format_bytes_compact(query_track_size(track.c_str()), size_buf, sizeof(size_buf));
            if (size_buf[0] != '\0') {
                append_meta_part(size_buf);
            }

            FixedString<96> file_line{};
            FixedString<128> directory_line{};
            FixedString<96> popup_title{};
            FixedString<96> popup_subtitle{};
            FixedString<128> popup_meta{};
            utf8_truncate_with_ellipsis_to(file_line, library_file_name(path), 46);
            library_compact_popup_directory_to(directory_line, library_directory_path(path), 4, 58);
            utf8_truncate_with_ellipsis_to(popup_title, title.view(), 28);
            utf8_truncate_with_ellipsis_to(popup_subtitle, subtitle_line.view(), 24);
            utf8_truncate_with_ellipsis_to(popup_meta, meta_line.view(), 42);

            set_label(handles.list_info_eyebrow,
                      library_popup_eyebrow(library_tab, library_has_context(), list_shuffle_enabled));
            set_label(handles.list_info_title,
                      popup_title.empty() ? title.c_str() : popup_title.c_str());
            set_label(handles.list_info_subtitle,
                      popup_subtitle.empty() ? subtitle_line.c_str() : popup_subtitle.c_str());
            set_label(handles.list_info_meta,
                      popup_meta.empty() ? meta_line.c_str() : popup_meta.c_str());
            set_label(handles.list_info_path_title, library_popup_path_title());
            set_label(handles.list_info_path,
                      file_line.empty() ? track.c_str() : file_line.c_str());
            set_label(handles.list_info_path_detail, directory_line.c_str());
            set_label(handles.list_info_hint, library_popup_close_hint());

            FixedString<260> cover_path{};
            if (!resolve_cover_path_for_track(path, cover_path)) {
                cover_path.assign(path);
            }
            ::ui::scene::ImageId image_id = resolve_list_cover_icon(path, cover_path.view());
            set_image_slot_if_present(library_info_cover_slot(), image_id, false);
            return true;
        }

        bool open_list_info_popup(int list_index) {
            if (!access.valid()) return false;
            if (list_index < 0 || list_index >= static_cast<int>(list_order.size())) return false;
            const auto row = make_library_row_recipe(list_index);
            if (row.group_row) return false;

            const int track_idx = list_index_to_track(list_index);
            if (track_idx < 0) return false;

            list_info_popup_track_index = track_idx;
            if (!populate_list_info_popup_content(track_idx)) {
                close_list_info_popup();
                return false;
            }
            set_list_info_popup_visible(true);
            return true;
        }

        bool handle_list_info_popup_key(UiKey key) {
            if (!list_info_popup_open()) return false;
            switch (key) {
            case UiKey::Left:
            case UiKey::Enter:
                close_list_info_popup();
                return true;
            case UiKey::Up:
            case UiKey::Down:
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
            close_list_info_popup();

            list_action_menu_index = list_index;

            if (handles.list) {
                ignore_list_select = true;
                access.set_list_view_selected(handles.list, list_index);
                ignore_list_select = false;
                last_list_selected = list_index;
            }

            const char* menu_title =
                library_action_menu_title(library_tab, library_has_context());
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
                    .title = menu_title,
                    .items = items,
                    .item_count = item_count,
                },
                library_action_menu_spec());
            apply_library_action_menu_theme();
            const int initial_focus = ::ui::scene::anchored_menu_first_visible_index(items, item_count);
            if (initial_focus >= 0) {
                focus_list_action_menu_item(initial_focus);
            } else {
                list_action_menu_focus_index = -1;
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

            const auto row = make_library_row_recipe(list_index);
            if (!row.group_row) {
                (void)open_list_info_popup(list_index);
                return;
            }

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
                if (row.text_resolved) {
                    library_context_key.assign(row.title());
                    refresh_list_view();
                    ensure_library_context_selection();
                    show_library_context_status();
                }
                return;
            }
            const int track_idx = list_index_to_track(list_index);
            if (track_idx >= 0) {
                select_library_track_index(track_idx, list_index);
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
            std::snprintf(buf, buf_size, "Back to %.*s",
                          static_cast<int>(back_target.size()), back_target.data());
            return buf[0] != '\0';
        }

        bool format_library_back_status_hint(char* buf,
                                             std::size_t buf_size) const noexcept {
            if (!buf || buf_size == 0) return false;
            buf[0] = '\0';
            if (!library_can_step_back()) return false;
            if (list_shuffle_enabled && !library_has_context()) {
                std::snprintf(buf, buf_size, "Back turns shuffle off");
                return buf[0] != '\0';
            }
            const std::string_view back_target = library_back_target_label();
            if (back_target.empty()) return false;
            std::snprintf(buf, buf_size, "Back to %.*s",
                          static_cast<int>(back_target.size()), back_target.data());
            return buf[0] != '\0';
        }

        void apply_library_tab_style(WidgetHandle handle,
                                     bool active,
                                     int radius,
                                     const player::cover_theme::LibraryChromeColorRoles& roles) {
            if (!access.valid() || !handle) return;
            const auto& role = active ? roles.tab_active : roles.tab_idle;
            ::ui::scene::PillSurfaceSpec spec{};
            spec.bg_color = role.bg;
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
            patch.font_color = role.fg;
            patch.has_border_focus = true;
            patch.border_focus = roles.focus_ring;
            access.set_style_override(handle, patch);
        }

        void apply_library_chip_style(WidgetHandle handle,
                                      bool active,
                                      int radius,
                                      const player::cover_theme::LibraryChromeColorRoles& roles,
                                      bool elevated = false) {
            if (!access.valid() || !handle) return;
            const auto& role = active ? roles.chip_active : roles.chip_idle;
            ::ui::scene::PillSurfaceSpec spec{};
            spec.bg_color = role.bg;
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
            patch.font_color = role.fg;
            patch.has_font = true;
            patch.font = active
                ? &get_player_font_px(elevated ? 17 : 17, FontWeight::Medium)
                : &get_player_font_px(elevated ? 17 : 16, FontWeight::Regular);
            patch.has_border_focus = true;
            patch.border_focus = roles.focus_ring;
            access.set_style_override(handle, patch);
        }

        void apply_library_action_menu_theme() noexcept {
            if (!access.valid()) return;
            constexpr rgba kTransparent{0, 0, 0, 0};
            const auto roles = player::cover_theme::derive_library_action_menu_color_roles(now_playing_roles);

            access.recolor_surface(handles.list_action_scrim, roles.scrim, kTransparent, kTransparent);
            access.recolor_surface(handles.list_action_card,
                                   roles.card.bg,
                                   roles.card.border,
                                   roles.card.fg);
            access.set_text_color(handles.list_action_title, roles.title);
            for (const WidgetHandle item : handles.list_action_items) {
                access.recolor_surface(item, roles.item.bg, roles.item.border, roles.item.fg);
            }
        }

        void apply_library_popup_theme() noexcept {
            if (!access.valid()) return;
            constexpr rgba kTransparent{0, 0, 0, 0};
            const auto roles = player::cover_theme::derive_library_popup_color_roles(now_playing_roles);

            access.recolor_surface(handles.list_info_scrim, roles.sheet_scrim, kTransparent, kTransparent);
            access.recolor_surface(handles.list_info_card,
                                   roles.sheet_card.bg,
                                   roles.sheet_card.border,
                                   roles.sheet_card.fg);
            access.recolor_surface(handles.list_info_cover_plate,
                                   roles.sheet_cover_plate.bg,
                                   roles.sheet_cover_plate.border,
                                   roles.sheet_cover_plate.fg);
            access.recolor_surface(handles.list_info_close,
                                   roles.sheet_close.bg,
                                   roles.sheet_close.border,
                                   roles.sheet_close.fg);
            access.recolor_surface(handles.list_info_path_plate,
                                   roles.sheet_path_plate.bg,
                                   roles.sheet_path_plate.border,
                                   roles.sheet_path_plate.fg);

            access.set_text_color(handles.list_info_eyebrow, roles.sheet_eyebrow);
            access.set_text_color(handles.list_info_title, roles.sheet_title);
            access.set_text_color(handles.list_info_subtitle, roles.sheet_subtitle);
            access.set_text_color(handles.list_info_meta, roles.sheet_meta);
            access.set_text_color(handles.list_info_path_title, roles.sheet_path_title);
            access.set_text_color(handles.list_info_path, roles.sheet_path_value);
            access.set_text_color(handles.list_info_path_detail, roles.sheet_meta);
            access.set_text_color(handles.list_info_hint, roles.sheet_hint);
        }

        bool library_page_active() const noexcept {
            return current_page == PlayerPage::Library;
        }

        void apply_library_overlay_theme() noexcept {
            apply_library_action_menu_theme();
            apply_library_popup_theme();
        }

        void refresh_library_visual_content(bool invalidate_state = false) noexcept {
            if (invalidate_state) {
                invalidate_library_visual_state();
            }
            apply_library_overlay_theme();
            update_library_state_visuals();
            update_list_placeholder();
        }

        void refresh_library_visual_if_active(bool invalidate_state = false) noexcept {
            if (!library_page_active()) return;
            refresh_library_visual_content(invalidate_state);
        }

        void refresh_library_runtime_content() {
            update_list_title();
            update_list_path();
            update_list_sort_label();
            update_list_shuffle_label();
            tick_list_duration_probe();
        }

        void refresh_library_body_content() {
            refresh_library_visual_content();
            refresh_library_runtime_content();
        }

        void update_library_state_visuals() {
            if (!access.valid()) return;
            const auto roles = player::cover_theme::derive_library_chrome_color_roles(now_playing_roles);
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
            apply_library_tab_style(handles.list_tab_songs,
                                    library_tab == LibraryTab::Songs,
                                    kLibraryTabRadius,
                                    roles);
            apply_library_tab_style(handles.list_tab_albums,
                                    library_tab == LibraryTab::Albums,
                                    kLibraryTabRadius,
                                    roles);
            apply_library_tab_style(handles.list_tab_artist,
                                    library_tab == LibraryTab::Artists,
                                    kLibraryTabRadius,
                                    roles);
            apply_library_chip_style(handles.list_shuffle,
                                     list_shuffle_enabled,
                                     kLibraryTabRadius,
                                     roles,
                                     true);
            apply_library_chip_style(handles.list_sort,
                                     sort_active,
                                     kLibrarySortRadius,
                                     roles,
                                     true);
            if (handles.list_sort) {
                access.set_button_icon(handles.list_sort, list_shuffle_enabled ? icons.shuffle : icons.down);
            }
            if (handles.list_tab_indicator) {
                const int indicator_w = std::max<int>(36, active_rect.w / 4);
                access.set_style_override(handles.list_tab_indicator, ::ui::scene::make_clean_surface_patch({
                    .apply_bg_color = true,
                    .apply_border_color = true,
                    .apply_corner_radius = true,
                    .bg_color = roles.indicator,
                    .border_color = {0, 0, 0, 0},
                    .corner_radius = 2,
                }));
                access.set_rect(handles.list_tab_indicator,
                    {active_rect.x + (active_rect.w - indicator_w) / 2,
                     active_rect.y + active_rect.h + 5,
                     indicator_w,
                     4});
            }
            if (handles.list_title) {
                StylePatch patch{};
                patch.has_font_color = true;
                patch.font_color = has_context ? roles.title_context : roles.title;
                patch.has_font = true;
                patch.font = has_context
                    ? &get_player_font_px(24, FontWeight::Bold)
                    : &get_player_font_px(22, FontWeight::Medium);
                access.set_style_override(handles.list_title, patch);
            }

            if (handles.list_path_bg) {
                const rgba path_bg = has_context
                    ? roles.path_bg_context
                    : (path_active ? roles.path_bg_active : roles.path_bg_idle);
                const rgba path_gradient_start = has_context
                    ? roles.path_gradient_start_context
                    : (path_active ? roles.path_gradient_start_active
                                   : roles.path_gradient_start_idle);
                const rgba path_gradient_end = has_context
                    ? roles.path_gradient_end_context
                    : (path_active ? roles.path_gradient_end_active
                                   : roles.path_gradient_end_idle);
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
                    ? roles.path_text_context
                    : path_active
                    ? roles.path_text_active
                    : roles.path_text_idle;
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
            const auto media = derive_media_track(path);

            switch (library_tab) {
            case LibraryTab::Songs:
                return {
                    media.title.empty() ? media.stem : media.title,
                    media.artist,
                    media.album,
                };
            case LibraryTab::Albums:
                return {
                    media_group_key(media, MediaGroupKind::Albums),
                    media.title.empty() ? media.stem : media.title,
                    media.artist,
                };
            case LibraryTab::Artists:
                return {
                    media_group_key(media, MediaGroupKind::Artists),
                    media.title.empty() ? media.stem : media.title,
                    media.album,
                };
            }
            return {};
        }

        void rebuild_list_display_cache() {
            list_row_title_scratch.clear();
            list_row_subtitle_scratch.clear();
            list_row_tail_scratch.clear();
            list_cover_path_scratch.clear();
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
            if (row.text_resolved) {
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
            if (row.text_resolved) {
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
            if (row.text_resolved) {
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
            if (row.track_path.empty()) {
                return ::ui::scene::invalid_image_id();
            }
            if (!library_list_uses_cover_art()) {
                return row.fallback_icon;
            }
            if (!row.prefer_cover) {
                return row.fallback_icon;
            }
            const auto cover_id = self->resolve_list_cover_icon(row.track_path, row.cover_path);
            if (::ui::scene::image_id_valid(cover_id)) return cover_id;
            return row.fallback_icon;
        }

        void clear_list_cover_cache() {
            for (auto& entry : list_cover_cache) {
                if (::ui::scene::image_id_valid(entry.cover.image_id)) {
                    release_resolved_cover(entry.cover);
                }
                entry.path.clear();
                entry.missing = false;
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
            if (!library_allows_cover_request(track_path, cover_path.view())) {
                return ::ui::scene::invalid_image_id();
            }
            if (current_cover.key.view() == cover_path.view()
                && ::ui::scene::image_id_valid(current_cover.image_id)) {
                return current_cover.image_id;
            }
            for (auto& entry : list_cover_cache) {
                if (entry.path.view() != cover_path.view()) continue;
                if (entry.missing) return ::ui::scene::invalid_image_id();
                if (::ui::scene::image_id_valid(entry.cover.image_id)) return entry.cover.image_id;
            }
            if (::ui::gfx::image_registry_locked()) {
                return ::ui::scene::invalid_image_id();
            }
            if constexpr (kListCoverCache == 0) {
                return ::ui::scene::invalid_image_id();
            } else {
                auto& slot = list_cover_cache[list_cover_next % kListCoverCache];
                list_cover_next += 1;
                if (::ui::scene::image_id_valid(slot.cover.image_id)) {
                    release_resolved_cover(slot.cover);
                }
                slot.path.assign(cover_path.view());
                slot.missing = false;
                if (resolve_cover(cover_path.view(), slot.cover)) {
                    return slot.cover.image_id;
                }
                slot.missing = true;
                return ::ui::scene::invalid_image_id();
            }
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
                        (void)list_order.push_back(static_cast<int>(i));
                    }
                }
            } else {
                for (std::size_t i = 0; i < count; ++i) {
                    if (library_context_matches_track(static_cast<int>(i))) {
                        (void)list_order.push_back(static_cast<int>(i));
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
            access.set_list_view_tail_icon_source(handles.list, this, &PlayerController::list_view_tail_icon, 18);
            access.set_list_view_tail_action_icon_source(handles.list, this, &PlayerController::list_view_tail_action_icon, 22);
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
            close_list_info_popup();
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
            close_list_info_popup();
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
            close_list_info_popup();
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
            close_list_info_popup();
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

        bool open_library_info_popup_for_preview(int list_index = -1) {
            close_list_action_menu();
            if (!access.valid()) return false;

            auto try_open = [&](int candidate) {
                if (candidate < 0 || candidate >= static_cast<int>(list_order.size())) return false;
                const auto row = make_library_row_recipe(candidate);
                if (row.group_row) return false;
                return open_list_info_popup(candidate);
            };

            if (try_open(list_index)) {
                return true;
            }
            if (try_open(last_list_selected)) {
                return true;
            }
            for (int candidate = 0; candidate < static_cast<int>(list_order.size()); ++candidate) {
                if (try_open(candidate)) {
                    return true;
                }
            }
            return false;
        }

        bool open_library_action_menu_for_preview(int list_index = -1) {
            close_list_info_popup();
            if (!access.valid()) return false;

            auto try_open = [&](int candidate) {
                if (candidate < 0 || candidate >= static_cast<int>(list_order.size())) return false;
                const auto row = make_library_row_recipe(candidate);
                if (row.group_row) return false;
                open_list_action_menu(candidate);
                return list_action_menu_index == candidate;
            };

            if (try_open(list_index)) {
                return true;
            }
            if (try_open(last_list_selected)) {
                return true;
            }
            for (int candidate = 0; candidate < static_cast<int>(list_order.size()); ++candidate) {
                if (try_open(candidate)) {
                    return true;
                }
            }
            return false;
        }

        void toggle_library_shuffle() {
            close_list_info_popup();
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
            close_list_info_popup();
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

        void apply_storage_view(StorageView view, bool allow_track_preload = true) {
            close_list_info_popup();
            close_list_action_menu();
            storage = view;
            fs_ready = storage.fs_ready;
            reset_playback_queue();
            mount_status.assign(storage.mount_status);
            clear_list_cover_cache();
            reset_track_duration_cache();
            refresh_track_labels();
            refresh_home_collage_if_active(true);
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
            if (fs_ready && storage.tracks && storage.tracks->size() > 0) {
                (void)rebuild_full_playback_queue(track_index);
            }
            if (allow_track_preload
                && fs_ready && storage.tracks && storage.tracks->size() > 0
                && handles.list
                && !has_active_playback()
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
            const auto roles = player::cover_theme::derive_library_chrome_color_roles(now_playing_roles);
            const int count = static_cast<int>(list_order.size());
            const bool show_empty = (count == 0);
            const bool show_group_hint = !show_empty && library_shows_group_rows();
            const bool show_context_hint = !show_empty && library_has_context() && library_can_step_back();
            const bool show_tracks_hint = !show_empty
                && !library_has_context()
                && library_tab == LibraryTab::Songs;
            const bool show = show_empty || show_group_hint || show_context_hint || show_tracks_hint;
            const auto soften_hint_color = [](rgba color) noexcept -> rgba {
                return rgba{color.r, color.g, color.b,
                            static_cast<std::uint8_t>(std::min<int>(color.a, 168))};
            };
            const auto apply_hint_style = [&](rgba color,
                                              FontWeight weight,
                                              int px) noexcept {
                StylePatch patch{};
                patch.has_font = true;
                patch.font = &get_player_font_px(px, weight);
                patch.has_font_color = true;
                patch.font_color = soften_hint_color(color);
                access.set_style_override(handles.list_hint, patch);
            };
            access.set_visible(handles.list_hint, show);
            if (!show) return;
            if (show_empty && !mount_status.empty()) {
                apply_hint_style(roles.hint_default, FontWeight::Medium, 13);
                if (last_list_hint_text.view() == mount_status.view()) return;
                last_list_hint_text.assign(mount_status.view());
                set_label_slot(handles.list_hint, text_slots.list_hint, mount_status.c_str());
                return;
            }
            const char* hint = "";
            char buf[160]{};
            if (show_group_hint) {
                apply_hint_style(roles.hint_accent, FontWeight::Medium, 13);
                const char* label = (library_tab == LibraryTab::Albums)
                    ? ((count == 1) ? "album" : "albums")
                    : ((count == 1) ? "artist" : "artists");
                std::snprintf(buf, sizeof(buf), "%d %s | Select to browse",
                              count, label);
                if (list_shuffle_enabled) {
                    append_library_status_suffix(buf, sizeof(buf), "Shuffled");
                }
                hint = buf;
            } else if (show_context_hint) {
                FixedString<96> summary_text{};
                utf8_truncate_with_ellipsis_to(summary_text, library_context_key.view(), 28);
                char path_hint_buf[80]{};
                const bool has_path_hint = format_library_path_bar_hint(path_hint_buf, sizeof(path_hint_buf));
                if (!summary_text.empty()) {
                    apply_hint_style(roles.hint_context, FontWeight::Medium, 13);
                    if (has_path_hint) {
                        std::snprintf(buf, sizeof(buf), "%s | %s",
                                      summary_text.c_str(), path_hint_buf);
                        hint = buf;
                    } else {
                        hint = summary_text.c_str();
                    }
                } else if (has_path_hint) {
                    apply_hint_style(roles.hint_accent, FontWeight::Medium, 13);
                    hint = path_hint_buf;
                }
            } else if (show_tracks_hint) {
                apply_hint_style(roles.hint_default, FontWeight::Medium, 13);
                const char* track_label = (count == 1) ? "track" : "tracks";
                if (list_shuffle_enabled) {
                    std::snprintf(buf, sizeof(buf), "%d %s | Shuffled",
                                  count, track_label);
                } else {
                    std::snprintf(buf, sizeof(buf), "%d %s",
                                  count, track_label);
                }
                hint = buf;
            } else {
                apply_hint_style(roles.hint_default,
                                 !fs_ready ? FontWeight::Medium : FontWeight::Regular,
                                 13);
                if (!fs_ready) {
                    hint = "FS not ready";
                } else {
                    const char* empty_hint = "";
                    if (library_has_context()) {
                        empty_hint = (library_tab == LibraryTab::Albums)
                            ? "No tracks in this album"
                            : "No tracks by this artist";
                    } else {
                        switch (library_tab) {
                        case LibraryTab::Albums:
                            empty_hint = "No albums in library";
                            break;
                        case LibraryTab::Artists:
                            empty_hint = "No artists in library";
                            break;
                        case LibraryTab::Songs:
                        default:
                            empty_hint = "No tracks in library";
                            break;
                        }
                    }
                    char path_hint_buf[80]{};
                    if (format_library_path_bar_hint(path_hint_buf, sizeof(path_hint_buf))) {
                        std::snprintf(buf, sizeof(buf), "%s | %s", empty_hint, path_hint_buf);
                        hint = buf;
                    } else {
                        hint = empty_hint;
                    }
                }
            }
            if (last_list_hint_text.view() == hint) return;
            last_list_hint_text.assign(hint);
            set_label_slot(handles.list_hint, text_slots.list_hint, hint);
        }
