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
            update_list_placeholder();
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

        static constexpr int kLibraryTabRadius = 19;
        static constexpr int kLibraryPathRadius = 10;

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

        static std::string_view library_artist_from_stem(std::string_view stem) noexcept {
            const auto sep = stem.find(" - ");
            if (sep == std::string_view::npos || sep == 0) return {};
            return stem.substr(0, sep);
        }

        static std::string_view library_title_from_stem(std::string_view stem) noexcept {
            const auto sep = stem.find(" - ");
            if (sep == std::string_view::npos) return stem;
            const auto title_start = sep + 3;
            if (title_start >= stem.size()) return stem;
            return stem.substr(title_start);
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

        std::string_view library_group_title_for_track(int track_index) const noexcept {
            const auto* tracks = storage.tracks;
            if (!tracks || track_index < 0 || track_index >= static_cast<int>(tracks->size())) {
                return {};
            }
            const auto path = (*tracks)[static_cast<std::size_t>(track_index)].view();
            const std::string_view stem = library_track_stem(path);
            const std::string_view artist = library_artist_from_stem(stem);
            const std::string_view album = library_parent_folder(path);
            switch (library_tab) {
            case LibraryTab::Albums:
                return library_album_label(album);
            case LibraryTab::Artists:
                return library_artist_label(artist);
            case LibraryTab::Songs:
            default:
                return {};
            }
        }

        bool library_context_matches_track(int track_index) const noexcept {
            if (!library_has_context()) return true;
            return compare_ci(library_group_title_for_track(track_index), library_context_key.view()) == 0;
        }

        int library_group_track_count_for_row(int row_index) const noexcept {
            if (!library_shows_group_rows()) return 0;
            if (row_index < 0 || row_index >= static_cast<int>(list_order.size())) return 0;
            const auto key = library_group_title_for_track(list_order[static_cast<std::size_t>(row_index)]);
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

        void activate_list_index(int list_index) {
            if (list_index < 0 || list_index >= static_cast<int>(list_order.size())) return;
            if (library_shows_group_rows()) {
                if (list_index < static_cast<int>(list_display_titles.size())) {
                    library_context_key.assign(list_display_titles[static_cast<std::size_t>(list_index)].view());
                    refresh_list_view();
                }
                return;
            }
            const int track_idx = list_index_to_track(list_index);
            if (track_idx >= 0) {
                select_track_index(track_idx);
            }
        }

        void apply_library_chip_style(WidgetHandle handle, bool active, int radius,
                                      bool elevated = false) {
            if (!access.valid() || !handle) return;
            ::ui::scene::PillSurfaceSpec spec{};
            spec.bg_color = active ? kUiTabActive : kUiTabBg;
            spec.border_color = active ? kUiTabActive : kUiButtonBorder;
            spec.corner_radius = radius;
            if (active || elevated) {
                spec.shadow.enabled = true;
                spec.shadow.color = kUiCardShadow;
                spec.shadow.offset_x = 0;
                spec.shadow.offset_y = active ? 2 : 3;
                spec.shadow.spread = active ? 2 : 3;
                spec.shadow.radius = active ? 10 : 16;
            }
            if (active) {
                spec.inner_stroke_enabled = true;
                spec.inner_stroke_color = kUiButtonHover;
                spec.inner_stroke_width = 1;
            }
            access.set_style_override(handle, ::ui::scene::make_pill_surface_patch(spec));
        }

        void update_library_state_visuals() {
            apply_library_chip_style(handles.list_tab_songs, library_tab == LibraryTab::Songs, kLibraryTabRadius);
            apply_library_chip_style(handles.list_tab_albums, library_tab == LibraryTab::Albums, kLibraryTabRadius);
            apply_library_chip_style(handles.list_tab_artist, library_tab == LibraryTab::Artists, kLibraryTabRadius);
            apply_library_chip_style(handles.list_shuffle, list_shuffle_enabled, kLibraryTabRadius, true);

            if (handles.list_path_bg) {
                const bool active = (library_tab != LibraryTab::Songs) || list_shuffle_enabled;
                const auto patch = ::ui::scene::make_pill_surface_patch({
                    .bg_color = active ? kUiButtonHover : kUiButtonBg,
                    .border_color = active ? kUiTabActive : kUiButtonBorder,
                    .corner_radius = kLibraryPathRadius,
                });
                access.set_style_override(handles.list_path_bg, patch);
            }
            if (handles.list_path) {
                StylePatch patch{};
                patch.has_font_color = true;
                patch.font_color = ((library_tab != LibraryTab::Songs) || list_shuffle_enabled)
                    ? kUiOk
                    : kUiListTitle;
                access.set_style_override(handles.list_path, patch);
            }
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
            list_display_titles.clear();
            list_display_subtitles.clear();
            list_display_tails.clear();

            const std::size_t count = list_order.size();
            list_display_titles.resize(count);
            list_display_subtitles.resize(count);
            list_display_tails.resize(count);
            for (std::size_t i = 0; i < count; ++i) {
                const int track_index = list_order[i];
                const auto* tracks = storage.tracks;
                if (!tracks || track_index < 0 || track_index >= static_cast<int>(tracks->size())) {
                    list_display_titles[i].assign("Unknown Track");
                    list_display_subtitles[i].assign("Track");
                    list_display_tails[i].assign("");
                    continue;
                }
                const auto path = (*tracks)[static_cast<std::size_t>(track_index)].view();
                const std::string_view stem = library_track_stem(path);
                const std::string_view artist = library_artist_from_stem(stem);
                const std::string_view title = library_title_from_stem(stem);
                const std::string_view album = library_parent_folder(path);
                const std::string_view format = format_from_path(path);

                std::string_view display_title{};
                std::string_view display_subtitle{};
                std::string_view display_tail{};
                if (library_shows_group_rows()) {
                    display_title = library_group_title_for_track(track_index);
                    char buf[32]{};
                    std::snprintf(buf, sizeof(buf), "%d tracks", library_group_track_count_for_row(static_cast<int>(i)));
                    list_display_titles[i].assign(display_title.empty() ? std::string_view{"Library"} : display_title);
                    list_display_subtitles[i].assign(buf);
                    list_display_tails[i].assign("");
                    player::font_cache::ensure_text(list_display_titles[i].view());
                    player::font_cache::ensure_text(list_display_subtitles[i].view());
                    continue;
                }

                display_tail = format;

                switch (library_tab) {
                case LibraryTab::Songs:
                    display_title = title.empty() ? stem : title;
                    if (!artist.empty()) {
                        display_subtitle = artist;
                    } else if (!album.empty()) {
                        display_subtitle = album;
                    } else {
                        display_subtitle = format.empty() ? std::string_view{"Track"} : format;
                    }
                    break;
                case LibraryTab::Albums:
                    display_title = title.empty() ? (stem.empty() ? std::string_view{"Track"} : stem) : title;
                    if (!artist.empty()) {
                        display_subtitle = artist;
                    } else {
                        display_subtitle = format.empty() ? std::string_view{"Track"} : format;
                    }
                    break;
                case LibraryTab::Artists:
                    display_title = title.empty() ? (stem.empty() ? std::string_view{"Track"} : stem) : title;
                    if (!album.empty()) {
                        display_subtitle = album;
                    } else {
                        display_subtitle = format.empty() ? std::string_view{"Track"} : format;
                    }
                    break;
                }

                if (display_title.empty()) display_title = "Unknown Track";
                if (display_subtitle.empty()) display_subtitle = "Track";
                list_display_titles[i].assign(display_title);
                list_display_subtitles[i].assign(display_subtitle);
                list_display_tails[i].assign(display_tail);
                player::font_cache::ensure_text(list_display_titles[i].view());
                player::font_cache::ensure_text(list_display_subtitles[i].view());
                player::font_cache::ensure_text(list_display_tails[i].view());
            }
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
            const std::size_t row = static_cast<std::size_t>(index);
            if (row < self->list_display_titles.size()) {
                return self->list_display_titles[row].c_str();
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
            const std::size_t row = static_cast<std::size_t>(index);
            if (row < self->list_display_subtitles.size()) {
                return self->list_display_subtitles[row].c_str();
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
            const std::size_t row = static_cast<std::size_t>(index);
            if (row < self->list_display_tails.size()) {
                return self->list_display_tails[row].c_str();
            }
            const int track_index = self->list_index_to_track(static_cast<int>(index));
            if (track_index < 0) return "";
            const auto* tracks = self->storage.tracks;
            if (!tracks || track_index >= static_cast<int>(tracks->size())) return "";
            return format_from_path((*tracks)[static_cast<std::size_t>(track_index)].view()).data();
        }

        static ::ui::scene::ImageId list_view_tail_icon(const void* ctx, std::uint16_t index) noexcept {
            auto* self = static_cast<PlayerController*>(const_cast<void*>(ctx));
            if (!self) return ::ui::scene::invalid_image_id();
            if (self->track_index < 0) return ::ui::scene::invalid_image_id();
            const int row_track = self->list_index_to_track(static_cast<int>(index));
            if (row_track < 0) return ::ui::scene::invalid_image_id();

            bool is_current_row = false;
            if (self->library_shows_group_rows()) {
                const auto row_group = self->library_group_title_for_track(row_track);
                const auto current_group = self->library_group_title_for_track(self->track_index);
                is_current_row = !row_group.empty() && compare_ci(row_group, current_group) == 0;
            } else {
                is_current_row = row_track == self->track_index;
            }

            if (!is_current_row) return ::ui::scene::invalid_image_id();
            return self->is_paused() ? self->icons.pause : self->icons.play;
        }

        static ::ui::scene::ImageId list_view_icon(const void* ctx, std::uint16_t index) noexcept {
            auto* self = static_cast<PlayerController*>(const_cast<void*>(ctx));
            if (!self) return ::ui::scene::invalid_image_id();
            const auto* tracks = self->storage.tracks;
            if (!tracks) return ::ui::scene::invalid_image_id();
            const int track_index = self->list_index_to_track(static_cast<int>(index));
            const bool is_active = !self->library_shows_group_rows() && track_index == self->track_index;
            if (track_index < 0 || track_index >= static_cast<int>(tracks->size())) {
                return ::ui::scene::invalid_image_id();
            }
            const auto& path = (*tracks)[static_cast<std::size_t>(track_index)];
            if (!is_audio_path(path.view())) {
                return self->icons.folder;
            }
            std::string_view cover_path{};
            if (track_index < static_cast<int>(self->list_cover_paths.size())) {
                cover_path = self->list_cover_paths[static_cast<std::size_t>(track_index)].view();
            }
            const auto cover_id = self->resolve_list_cover_icon(path.view(), cover_path);
            if (::ui::scene::image_id_valid(cover_id)) return cover_id;
            if (is_active) {
                return self->is_paused() ? self->icons.pause : self->icons.play;
            }
            return self->icons.play;
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

        void sync_list_selection() {
            if (!access.valid() || !handles.list) return;
            const auto* labels = storage.track_labels;
            if (!labels) return;
            if (track_index < 0 || track_index >= static_cast<int>(labels->size())) return;
            ignore_list_select = true;
            const int list_index = track_index_to_list(track_index);
            access.set_list_view_active(handles.list, list_index);
            if (list_index >= 0) {
                access.set_list_view_selected(handles.list, list_index);
                last_list_selected = list_index;
            }
            ignore_list_select = false;
        }

        void refresh_list_view() {
            if (!access.valid() || !handles.list) return;
            rebuild_list_order();
            const int count = static_cast<int>(list_order.size());
            access.set_list_view_source(handles.list,
                                         static_cast<std::uint16_t>(count),
                                         this,
                                         &PlayerController::list_view_text);
            access.set_list_view_subtitle_source(handles.list, this, &PlayerController::list_view_subtitle);
            access.set_list_view_tail_source(handles.list, this, &PlayerController::list_view_tail);
            access.set_list_view_tail_icon_source(handles.list, this, &PlayerController::list_view_tail_icon, 18);
            access.set_list_view_icon_source(handles.list, this, &PlayerController::list_view_icon, 32);
            access.set_list_row_height(handles.list, 72);
            access.set_scroll_step(handles.list, 72);
            if (count > 0) {
                sync_list_selection();
            } else {
                access.set_list_view_active(handles.list, -1);
                last_list_selected = -1;
            }
            update_list_title();
            update_list_path();
            update_list_sort_label();
            update_list_placeholder();
            update_library_state_visuals();
        }

        void set_library_tab(LibraryTab tab) {
            if (library_tab == tab) {
                if (library_has_context()) {
                    library_context_key.clear();
                    refresh_list_view();
                    return;
                }
                update_library_state_visuals();
                return;
            }
            library_tab = tab;
            library_context_key.clear();
            refresh_list_view();
        }

        void toggle_library_shuffle() {
            list_shuffle_enabled = !list_shuffle_enabled;
            if (list_shuffle_enabled) {
                auto seed = static_cast<std::uint32_t>(charm::system::ClockCaps::TimeSource::now());
                if (seed == 0) seed = 0x5A17D3C1u;
                list_shuffle_seed = seed;
            }
            refresh_list_view();
        }

        void reset_library_context() {
            if (library_has_context()) {
                library_context_key.clear();
                refresh_list_view();
                return;
            }
            if (list_shuffle_enabled) {
                list_shuffle_enabled = false;
                refresh_list_view();
                return;
            }
            if (library_tab != LibraryTab::Songs) {
                library_tab = LibraryTab::Songs;
                refresh_list_view();
                return;
            }
            update_library_state_visuals();
            update_list_path();
        }

        void apply_storage_view(StorageView view) {
            storage = view;
            fs_ready = storage.fs_ready;
            mount_status.assign(storage.mount_status);
            clear_list_cover_cache();
            refresh_track_labels();
            build_list_cover_paths();
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
            const char* base = library_tab_title(library_tab);
            char buf[64]{};
            if (count > 0) {
                std::snprintf(buf, sizeof(buf), "%s (%d)", base, count);
                if (last_list_title_text.view() == buf) return;
                last_list_title_text.assign(buf);
                set_label_slot(handles.list_title, text_slots.list_title, buf);
            } else {
                if (last_list_title_text.view() == base) return;
                last_list_title_text.assign(base);
                set_label_slot(handles.list_title, text_slots.list_title, base);
            }
            last_list_count = count;
        }

        void update_list_path() {
            if (!access.valid() || !handles.list_path) return;
            FixedString<192> path;
            path.assign("/library/");
            path.append(library_tab_path(library_tab));
            if (library_has_context()) {
                path.append("/");
                path.append(library_context_key.view());
            }
            if (list_shuffle_enabled) {
                path.append("/shuffle");
            }
            set_label_slot(handles.list_path, text_slots.list_path, path.c_str());
        }

        void update_list_sort_label() {
            if (!access.valid() || !handles.list_sort) return;
            const char* label = (list_sort == ListSort::NameAsc) ? "Sort A-Z" : "Sort Z-A";
            set_label_slot(handles.list_sort, text_slots.list_sort, label);
        }

        void update_list_placeholder() {
            if (!access.valid() || !handles.list_hint) return;
            const int count = static_cast<int>(list_order.size());
            const bool show = (count == 0);
            access.set_visible(handles.list_hint, show);
            if (!show) return;
            if (!mount_status.empty()) {
                if (last_list_hint_text.view() == mount_status.view()) return;
                last_list_hint_text.assign(mount_status.view());
                set_label_slot(handles.list_hint, text_slots.list_hint, mount_status.c_str());
                return;
            }
            const char* hint = fs_ready ? "No items in current library view" : "FS not ready";
            if (last_list_hint_text.view() == hint) return;
            last_list_hint_text.assign(hint);
            set_label_slot(handles.list_hint, text_slots.list_hint, hint);
        }
