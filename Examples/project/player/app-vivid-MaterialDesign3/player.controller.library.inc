        void refresh_library() {
            if (handles.nav_bar) {
                access.set_visible(handles.nav_bar, true);
            }
            if (handles.bottom_bar) {
                access.set_visible(handles.bottom_bar, true);
            }
            update_nav_page_indicator();
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

        int list_index_to_track(int list_index) const noexcept {
            if (list_index < 0) return -1;
            if (list_order.empty()) return list_index;
            if (list_index >= static_cast<int>(list_order.size())) return -1;
            return list_order[static_cast<std::size_t>(list_index)];
        }

        int track_index_to_list(int track_index) const noexcept {
            if (track_index < 0) return -1;
            if (list_order.empty()) return track_index;
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
            const auto* titles = self->storage.track_titles;
            if (!titles) return "";
            const int track_index = self->list_index_to_track(static_cast<int>(index));
            if (track_index < 0 || track_index >= static_cast<int>(titles->size())) return "";
            return (*titles)[static_cast<std::size_t>(track_index)].c_str();
        }

        static const char* list_view_subtitle(const void* ctx, std::uint16_t index) noexcept {
            auto* self = static_cast<const PlayerController*>(ctx);
            if (!self) return "";
            const auto* subtitles = self->storage.track_subtitles;
            if (!subtitles) return "";
            const int track_index = self->list_index_to_track(static_cast<int>(index));
            if (track_index < 0 || track_index >= static_cast<int>(subtitles->size())) return "";
            return (*subtitles)[static_cast<std::size_t>(track_index)].c_str();
        }

        static ::ui::scene::ImageId list_view_icon(const void* ctx, std::uint16_t index) noexcept {
            auto* self = static_cast<PlayerController*>(const_cast<void*>(ctx));
            if (!self) return ::ui::scene::invalid_image_id();
            const auto* tracks = self->storage.tracks;
            if (!tracks) return ::ui::scene::invalid_image_id();
            const int track_index = self->list_index_to_track(static_cast<int>(index));
            const bool is_active = track_index == self->track_index;
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
            const auto* labels = storage.track_labels;
            if (!labels) return;
            const std::size_t count = labels->size();
            list_order.reserve(count);
            for (std::size_t i = 0; i < count; ++i) {
                list_order.push_back(static_cast<int>(i));
            }
            if (count <= 1) return;
            auto cmp = [&](int a, int b) noexcept {
                const auto& left = (*labels)[static_cast<std::size_t>(a)];
                const auto& right = (*labels)[static_cast<std::size_t>(b)];
                const int res = compare_ci(left.view(), right.view());
                return (list_sort == ListSort::NameAsc) ? (res < 0) : (res > 0);
            };
            std::sort(list_order.begin(), list_order.end(), cmp);
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
            const auto* tracks = storage.tracks;
            const int count = tracks ? static_cast<int>(tracks->size()) : 0;
            rebuild_list_order();
            access.set_list_view_source(handles.list,
                                         static_cast<std::uint16_t>(count),
                                         this,
                                         &PlayerController::list_view_text);
            access.set_list_view_subtitle_source(handles.list, this, &PlayerController::list_view_subtitle);
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
            const auto* tracks = storage.tracks;
            const int count = tracks ? static_cast<int>(tracks->size()) : 0;
            char buf[64]{};
            if (count > 0) {
                std::snprintf(buf, sizeof(buf), "Tracks (%d)", count);
                if (last_list_title_text.view() == buf) return;
                last_list_title_text.assign(buf);
                set_label_slot(handles.list_title, text_slots.list_title, buf);
            } else {
                if (last_list_title_text.view() == "Tracks") return;
                last_list_title_text.assign("Tracks");
                set_label_slot(handles.list_title, text_slots.list_title, "Tracks");
            }
            last_list_count = count;
        }

        void update_list_path() {
            if (!access.valid() || !handles.list_path) return;
            const auto* tracks = storage.tracks;
            if (!tracks || tracks->size() == 0) {
                set_label_slot(handles.list_path, text_slots.list_path, "/");
                return;
            }
            bool all_music = true;
            for (std::size_t i = 0; i < tracks->size(); ++i) {
                const auto& path = (*tracks)[i];
                if (!path.view().starts_with("/music")) {
                    all_music = false;
                    break;
                }
            }
            const char* label = all_music ? "/music" : "/";
            set_label_slot(handles.list_path, text_slots.list_path, label);
        }

        void update_list_sort_label() {
            if (!access.valid() || !handles.list_sort) return;
            const char* label = (list_sort == ListSort::NameAsc) ? "Sort A-Z" : "Sort Z-A";
            set_label_slot(handles.list_sort, text_slots.list_sort, label);
        }

        void update_list_placeholder() {
            if (!access.valid() || !handles.list_hint) return;
            const auto* tracks = storage.tracks;
            const int count = tracks ? static_cast<int>(tracks->size()) : 0;
            const bool show = (count == 0);
            access.set_visible(handles.list_hint, show);
            if (!show) return;
            if (!mount_status.empty()) {
                if (last_list_hint_text.view() == mount_status.view()) return;
                last_list_hint_text.assign(mount_status.view());
                set_label_slot(handles.list_hint, text_slots.list_hint, mount_status.c_str());
                return;
            }
            const char* hint = fs_ready ? "No tracks in /music or /" : "FS not ready";
            if (last_list_hint_text.view() == hint) return;
            last_list_hint_text.assign(hint);
            set_label_slot(handles.list_hint, text_slots.list_hint, hint);
        }
