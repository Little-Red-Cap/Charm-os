        static void format_listening_duration_long(int total_sec, char* out, std::size_t out_size) noexcept {
            if (!out || out_size == 0) return;
            if (total_sec <= 0) {
                std::snprintf(out, out_size, "0 s");
                return;
            }
            const int hours = total_sec / 3600;
            const int minutes = (total_sec % 3600) / 60;
            const int seconds = total_sec % 60;
            if (hours > 0 && minutes > 0) {
                std::snprintf(out, out_size, "%d h %02d m", hours, minutes);
            } else if (hours > 0) {
                std::snprintf(out, out_size, "%d h", hours);
            } else if (minutes > 0) {
                std::snprintf(out, out_size, "%d m", minutes);
            } else {
                std::snprintf(out, out_size, "%d s", seconds);
            }
        }

        static void format_listening_duration_compact(int total_sec, char* out, std::size_t out_size) noexcept {
            if (!out || out_size == 0) return;
            if (total_sec <= 0) {
                std::snprintf(out, out_size, "0 s");
                return;
            }
            const int hours = total_sec / 3600;
            const int minutes = (total_sec % 3600) / 60;
            const int seconds = total_sec % 60;
            if (hours > 0 && minutes > 0) {
                std::snprintf(out, out_size, "%dh %dm", hours, minutes);
            } else if (hours > 0) {
                std::snprintf(out, out_size, "%dh", hours);
            } else if (minutes > 0) {
                std::snprintf(out, out_size, "%dm", minutes);
            } else {
                std::snprintf(out, out_size, "%ds", seconds);
            }
        }

        static std::string_view home_track_parent_path(std::string_view path) noexcept {
            const auto slash = path.find_last_of("/\\");
            if (slash == std::string_view::npos) return {};
            return path.substr(0, slash);
        }

        auto home_cover_slots() const noexcept {
            return std::array<::ui::scene::ImageSlotHandles, kHomeCollageSlots>{
                ::ui::scene::ImageSlotHandles{handles.home_cover_big, {}},
                ::ui::scene::ImageSlotHandles{handles.home_cover_left, {}},
                ::ui::scene::ImageSlotHandles{handles.home_cover_right, {}},
                ::ui::scene::ImageSlotHandles{handles.home_cover_small, {}},
                ::ui::scene::ImageSlotHandles{handles.home_cover_bottom_left, {}},
                ::ui::scene::ImageSlotHandles{handles.home_cover_bottom_right, {}},
            };
        }

        static player::DefaultCoverVariant home_collage_default_cover_variant(std::size_t slot) noexcept {
            switch (slot) {
                case 1:
                    return player::DefaultCoverVariant::HomeOrbitDisc;
                case 2:
                    return player::DefaultCoverVariant::HomeOrbitStack;
                case 3:
                    return player::DefaultCoverVariant::HomeCenterPill;
                case 4:
                    return player::DefaultCoverVariant::HomeBottomCard;
                case 5:
                    return player::DefaultCoverVariant::HomeBottomCut;
                case 0:
                default:
                    return player::DefaultCoverVariant::HomeHeroPill;
            }
        }

        static player::DefaultCoverVariant home_card_default_cover_variant(std::size_t index) noexcept {
            switch (index % 3) {
                case 1:
                    return player::DefaultCoverVariant::HomeOrbitStack;
                case 2:
                    return player::DefaultCoverVariant::HomeBottomCard;
                case 0:
                default:
                    return player::DefaultCoverVariant::HomeOrbitDisc;
            }
        }

        auto home_daily_mix_slots() const noexcept {
            return std::array<::ui::scene::ImageSlotHandles, 3>{
                ::ui::scene::ImageSlotHandles{
                    handles.home_daily_mix_covers[0],
                    handles.home_daily_mix_preview_plates[0]},
                ::ui::scene::ImageSlotHandles{
                    handles.home_daily_mix_covers[1],
                    handles.home_daily_mix_preview_plates[1]},
                ::ui::scene::ImageSlotHandles{
                    handles.home_daily_mix_covers[2],
                    handles.home_daily_mix_preview_plates[2]},
            };
        }

        auto home_daily_mix_preview_plate_handles() const noexcept {
            return handles.home_daily_mix_preview_plates;
        }

        auto home_recently_played_slots() const noexcept {
            return std::array<::ui::scene::ImageSlotHandles, 3>{
                ::ui::scene::ImageSlotHandles{
                    handles.home_recently_played_covers[0],
                    handles.home_recently_played_preview_plates[0]},
                ::ui::scene::ImageSlotHandles{
                    handles.home_recently_played_covers[1],
                    handles.home_recently_played_preview_plates[1]},
                ::ui::scene::ImageSlotHandles{
                    handles.home_recently_played_covers[2],
                    handles.home_recently_played_preview_plates[2]},
            };
        }

        auto home_recently_played_preview_plate_handles() const noexcept {
            return handles.home_recently_played_preview_plates;
        }

        enum class HomeCollageResolveResult : std::uint8_t {
            Skipped,
            Assigned,
            RetryLater,
        };

        struct HomeResolvedCollageImages {
            std::array<::ui::scene::ImageId, kHomeCollageSlots> images{};
            std::array<FixedString<260>, kHomeCollageSlots> keys{};
            std::array<FixedString<260>, kHomeCollageSlots> attempted_keys{};
            std::size_t count{0};
            std::size_t attempted_count{0};

            bool empty() const noexcept {
                return count == 0;
            }

            bool contains_key(std::string_view key) const noexcept {
                for (std::size_t i = 0; i < count; ++i) {
                    if (keys[i].view() == key) return true;
                }
                return false;
            }

            bool contains_attempted_key(std::string_view key) const noexcept {
                for (std::size_t i = 0; i < attempted_count; ++i) {
                    if (attempted_keys[i].view() == key) return true;
                }
                return false;
            }

            bool contains_any_key(std::string_view key) const noexcept {
                return contains_key(key) || contains_attempted_key(key);
            }

            void mark_attempted(std::string_view key) noexcept {
                if (key.empty()) return;
                if (contains_attempted_key(key)) return;
                if (attempted_count >= attempted_keys.size()) return;
                attempted_keys[attempted_count].assign(key);
                ++attempted_count;
            }

            void push(::ui::scene::ImageId image_id, std::string_view key) {
                if (count >= images.size()) return;
                images[count] = image_id;
                keys[count].assign(key);
                mark_attempted(key);
                ++count;
            }

            ::ui::scene::ImageId image_for_slot(std::size_t slot_index) const noexcept {
                return images[slot_index % count];
            }
        };

        struct HomeCollageTrackRef {
            int track_index{-1};
            FixedString<260> track_path{};
            FixedString<260> cover_path{};
            FixedString<260> identity_key{};

            bool valid() const noexcept {
                return track_index >= 0
                    && !track_path.empty()
                    && !cover_path.empty()
                    && !identity_key.empty();
            }
        };

        struct HomeCollageTrackPlan {
            std::array<HomeCollageTrackRef, kHomeCollageSlots> refs{};
            std::size_t count{0};

            bool empty() const noexcept {
                return count == 0;
            }

            bool full() const noexcept {
                return count >= refs.size();
            }

            bool contains_key(std::string_view key) const noexcept {
                for (std::size_t i = 0; i < count; ++i) {
                    if (refs[i].identity_key.view() == key) return true;
                }
                return false;
            }

            void push(const HomeCollageTrackRef& ref) noexcept {
                if (full()) return;
                refs[count] = ref;
                ++count;
            }

            void repeat_to_fill_slots() noexcept {
                if (empty()) return;
                const std::size_t unique_count = count;
                while (count < refs.size()) {
                    const std::size_t repeat_index = count % unique_count;
                    refs[count] = refs[repeat_index];
                    ++count;
                }
            }

            std::array<int, kHomeCollageSlots> track_indices() const noexcept {
                std::array<int, kHomeCollageSlots> out{};
                out.fill(-1);
                for (std::size_t i = 0; i < refs.size(); ++i) {
                    out[i] = refs[i].track_index;
                }
                return out;
            }
        };

        struct HomeRecentTrackPlan {
            std::array<HomeCollageTrackRef, 3> refs{};
            std::size_t count{0};

            bool empty() const noexcept {
                return count == 0;
            }

            bool full() const noexcept {
                return count >= refs.size();
            }

            bool contains_key(std::string_view key) const noexcept {
                for (std::size_t i = 0; i < count; ++i) {
                    if (refs[i].identity_key.view() == key) return true;
                }
                return false;
            }

            void push(const HomeCollageTrackRef& ref) noexcept {
                if (full()) return;
                refs[count] = ref;
                ++count;
            }

            void repeat_to_fill_slots() noexcept {
                if (empty()) return;
                const std::size_t unique_count = count;
                while (count < refs.size()) {
                    refs[count] = refs[count % unique_count];
                    ++count;
                }
            }

            std::array<int, 3> track_indices() const noexcept {
                std::array<int, 3> out{};
                out.fill(-1);
                for (std::size_t i = 0; i < refs.size(); ++i) {
                    out[i] = refs[i].track_index;
                }
                return out;
            }
        };

        bool home_image_slot_ready(const ::ui::scene::ImageSlotHandles& slot) const noexcept {
            return slot.image && access.kind(slot.image) == WidgetKind::Image;
        }

        bool home_collage_selection_changed(const std::array<int, kHomeCollageSlots>& next_track_indices,
                                            bool force) const noexcept {
            if (force) return true;
            for (std::size_t i = 0; i < kHomeCollageSlots; ++i) {
                if (next_track_indices[i] != last_home_collage_track_indices[i]) {
                    return true;
                }
            }
            return false;
        }

        bool home_recent_selection_changed(const std::array<int, 3>& next_track_indices,
                                           bool force) const noexcept {
            if (force) return true;
            for (std::size_t i = 0; i < next_track_indices.size(); ++i) {
                if (next_track_indices[i] != last_home_recent_track_indices[i]) {
                    return true;
                }
            }
            return false;
        }

        int home_collage_start_track_index(int track_count) const noexcept {
            if (track_count <= 0) return -1;
            return (track_index >= 0 && track_index < track_count) ? track_index : 0;
        }

        static bool build_home_collage_identity_key(std::string_view track_path,
                                                    std::string_view cover_path,
                                                    FixedString<260>& out_key) noexcept {
            out_key.clear();
            if (track_path.empty() || cover_path.empty()) return false;
            if (cover_path != track_path) {
                out_key.assign(cover_path);
                return !out_key.empty();
            }

            const std::string_view parent = home_track_parent_path(track_path);
            if (!parent.empty()) {
                out_key.assign(parent);
                return true;
            }

            out_key.assign(track_path);
            return true;
        }

        bool resolve_home_collage_track_ref(int idx, HomeCollageTrackRef& out) const {
            out = {};
            const auto* tracks = storage.tracks;
            if (!tracks || idx < 0 || idx >= static_cast<int>(tracks->size())) return false;

            const std::string_view track_path = (*tracks)[static_cast<std::size_t>(idx)].view();
            if (track_path.empty()) return false;

            out.track_index = idx;
            out.track_path.assign(track_path);
            if (!resolve_cover_path_for_track(track_path, out.cover_path) || out.cover_path.empty()) {
                out.cover_path.assign(track_path);
            }
            if (out.cover_path.empty()) return false;

            return build_home_collage_identity_key(
                out.track_path.view(),
                out.cover_path.view(),
                out.identity_key);
        }

        bool try_push_home_collage_track_candidate(HomeCollageTrackPlan& plan,
                                                   int candidate,
                                                   int track_count) const {
            if (plan.full()) return false;
            if (candidate < 0 || candidate >= track_count) return false;

            HomeCollageTrackRef ref{};
            if (!resolve_home_collage_track_ref(candidate, ref) || ref.identity_key.empty()) {
                return false;
            }
            if (plan.contains_key(ref.identity_key.view())) {
                return false;
            }

            plan.push(ref);
            return true;
        }

        HomeCollageTrackPlan build_home_collage_track_plan() const {
            HomeCollageTrackPlan plan{};
            const auto* tracks = storage.tracks;
            if (!tracks || tracks->size() == 0) return plan;

            const int count = static_cast<int>(tracks->size());
            const int start = home_collage_start_track_index(count);

            (void)try_push_home_collage_track_candidate(plan, start, count);
            for (int offset = 1; offset < count && !plan.full(); ++offset) {
                (void)try_push_home_collage_track_candidate(plan, (start + offset) % count, count);
            }
            if (plan.empty()) {
                for (int idx = 0; idx < count && !plan.full(); ++idx) {
                    HomeCollageTrackRef ref{};
                    ref.track_index = idx;
                    plan.push(ref);
                }
            }
            plan.repeat_to_fill_slots();
            return plan;
        }

        int recent_track_storage_index(std::string_view path) const noexcept {
            const auto* tracks = storage.tracks;
            if (!tracks || path.empty()) return -1;
            for (std::size_t i = 0; i < tracks->size(); ++i) {
                if ((*tracks)[i].view() == path) {
                    return static_cast<int>(i);
                }
            }
            return -1;
        }

        bool try_push_home_recent_track_candidate(HomeRecentTrackPlan& plan,
                                                  int candidate) const {
            if (plan.full()) return false;
            HomeCollageTrackRef ref{};
            if (!resolve_home_collage_track_ref(candidate, ref) || ref.identity_key.empty()) {
                return false;
            }
            if (plan.contains_key(ref.identity_key.view())) {
                return false;
            }
            plan.push(ref);
            return true;
        }

        HomeRecentTrackPlan build_home_recent_track_plan() const {
            HomeRecentTrackPlan plan{};
            const auto* tracks = storage.tracks;
            if (!tracks || tracks->size() == 0) return plan;

            for (std::size_t i = 0;
                 i < recent_track_history.tracks.size() && !plan.full();
                 ++i) {
                const int idx =
                    recent_track_storage_index(recent_track_history.tracks[i].path.view());
                if (idx >= 0) {
                    (void)try_push_home_recent_track_candidate(plan, idx);
                }
            }
            plan.repeat_to_fill_slots();
            return plan;
        }

        HomeCollageResolveResult try_assign_home_collage_slot_image(
            const ::ui::scene::ImageSlotHandles& image_slot,
            const HomeCollageTrackRef& track_ref,
            HomeResolvedCollageImages& resolved) {
            if (!track_ref.valid()) return HomeCollageResolveResult::Skipped;
            if (resolved.contains_any_key(track_ref.identity_key.view())) {
                return HomeCollageResolveResult::Skipped;
            }

            const auto image_id = resolve_list_cover_icon(
                track_ref.track_path.view(),
                track_ref.cover_path.view());
            if (!::ui::scene::image_id_valid(image_id)) {
                if (!::ui::gfx::image_registry_locked()) {
                    resolved.mark_attempted(track_ref.identity_key.view());
                }
                return ::ui::gfx::image_registry_locked()
                    ? HomeCollageResolveResult::RetryLater
                    : HomeCollageResolveResult::Skipped;
            }

            set_image_slot_if_present(image_slot, image_id, false);
            resolved.push(image_id, track_ref.identity_key.view());
            return HomeCollageResolveResult::Assigned;
        }

        void apply_home_collage_repeat_cover(const ::ui::scene::ImageSlotHandles& image_slot,
                                             std::size_t slot_index,
                                             const HomeResolvedCollageImages& resolved) noexcept {
            if (resolved.empty()) return;
            set_image_slot_if_present(image_slot, resolved.image_for_slot(slot_index), false);
        }

        void apply_home_collage_default_cover(const ::ui::scene::ImageSlotHandles& image_slot,
                                              std::size_t slot_index) noexcept {
            set_image_slot_if_present(
                image_slot,
                default_cover_image_id(home_collage_default_cover_variant(slot_index)),
                false);
        }

        template <std::size_t N>
        void sync_home_preview_card_slots(const std::array<::ui::scene::ImageSlotHandles, N>& slots,
                                          std::size_t start_index,
                                          const HomeResolvedCollageImages& resolved,
                                          bool retry_needed) {
            for (std::size_t i = 0; i < slots.size(); ++i) {
                const auto& slot = slots[i];
                if (!home_image_slot_ready(slot)) continue;
                if (resolved.empty()) {
                    if (!retry_needed) {
                        set_image_slot_if_present(
                            slot,
                            default_cover_image_id(home_card_default_cover_variant(start_index + i)));
                    }
                    continue;
                }
                set_image_slot_if_present(slot, resolved.image_for_slot(start_index + i));
            }
        }

        void apply_home_hero_title_style_override(WidgetHandle handle, const rgba& color) noexcept {
            if (!access.valid() || !handle) return;
            ::ui::scene::TextStyleSpec spec{};
            spec.color = color;
            spec.explicit_font = &get_player_font_px_variant(
                64, FontWeight::Regular, "wght636_wdth152_ROND50_XTRA520_YOPQ90_YTLC505");
            access.set_style_override(handle, ::ui::scene::make_text_style_patch(spec));
        }

        void apply_home_mix_title_style_override(WidgetHandle handle, const rgba& color) noexcept {
            if (!access.valid() || !handle) return;
            ::ui::scene::TextStyleSpec spec{};
            spec.color = color;
            spec.explicit_font = &get_player_font_px_variant(
                20, FontWeight::Regular, "wght630_wdth136_GRAD40_ROND100_XTRA520_YOPQ90_YTLC505");
            access.set_style_override(handle, ::ui::scene::make_text_style_patch(spec));
        }

        void apply_home_mix_subtitle_style_override(WidgetHandle handle, const rgba& color) noexcept {
            if (!access.valid() || !handle) return;
            ::ui::scene::TextStyleSpec spec{};
            spec.color = color;
            spec.explicit_font = &get_player_font_px_variant(14, FontWeight::Regular, "ROND100");
            access.set_style_override(handle, ::ui::scene::make_text_style_patch(spec));
        }

        void apply_home_daily_mix_header_gradient() noexcept {
            if (!access.valid() || !handles.home_daily_mix_header_band) return;
            StylePatch patch{};
            patch.has_gradient_enabled = true;
            patch.gradient_enabled = true;
            patch.has_gradient_start = true;
            patch.gradient_start = kUiHomeDailyMixTop;
            patch.has_gradient_end = true;
            patch.gradient_end = kUiHomeDailyMixBottom;
            patch.has_gradient_direction = true;
            patch.gradient_direction = 1;
            patch.has_border_color = true;
            patch.border_color = {0, 0, 0, 0};
            patch.has_border_width = true;
            patch.border_width = 0;
            patch.has_corner_radius = true;
            patch.corner_radius = 24;
            access.set_style_override(handles.home_daily_mix_header_band, patch);
        }

        void apply_home_theme() noexcept {
            if (!access.valid()) return;
            const auto roles = player::cover_theme::derive_home_color_roles(now_playing_roles);

            if (handles.home_backdrop) {
                StylePatch patch{};
                patch.has_bg_color = true;
                patch.bg_color = roles.backdrop;
                patch.has_border_color = true;
                patch.border_color = {0, 0, 0, 0};
                patch.has_border_width = true;
                patch.border_width = 0;
                patch.has_corner_radius = true;
                patch.corner_radius = 0;
                access.set_style_override(handles.home_backdrop, patch);
            }

            apply_home_hero_title_style_override(handles.home_title_top, roles.hero_title);
            apply_home_hero_title_style_override(handles.home_title_bottom, roles.hero_title);
            apply_home_mix_subtitle_style_override(handles.home_subtitle, roles.hero_subtitle);
            access.recolor_surface(handles.home_daily_mix_card,
                                   roles.daily_mix_card.bg,
                                   roles.daily_mix_card.border,
                                   roles.daily_mix_card.fg);
            access.recolor_surface(handles.home_recently_played_card,
                                   roles.recently_played_card.bg,
                                   roles.recently_played_card.border,
                                   roles.recently_played_card.fg);
            access.recolor_surface(handles.home_stats_probe_card,
                                   roles.stats_card.bg,
                                   roles.stats_card.border,
                                   roles.stats_card.fg);
            apply_home_mix_title_style_override(handles.home_daily_mix_title, roles.daily_mix_title);
            apply_home_mix_subtitle_style_override(handles.home_daily_mix_subtitle, roles.daily_mix_subtitle);
            access.set_text_color(handles.home_daily_mix_body, roles.daily_mix_body);
            access.set_text_color(handles.home_recently_played_title, roles.recently_played_title);
            access.set_text_color(handles.home_recently_played_body, roles.recently_played_body);
            access.set_text_color(handles.home_stats_title, roles.stats_title);
            access.set_text_color(handles.home_stats_subtitle, roles.stats_subtitle);
            access.set_text_color(handles.home_stats_plays_label, roles.stats_meta);
            access.set_text_color(handles.home_stats_avg_label, roles.stats_meta);
            access.set_text_color(handles.home_stats_total, roles.stats_value);
            access.set_text_color(handles.home_stats_plays, roles.stats_value);
            access.set_text_color(handles.home_stats_avg, roles.stats_value);

            access.recolor_surface(handles.home_play, roles.hero_play.bg, roles.hero_play.border, roles.hero_play.fg);
            apply_home_daily_mix_header_gradient();
            access.recolor_surface(handles.home_daily_mix_chip,
                                   roles.daily_mix_chip.bg,
                                   roles.daily_mix_chip.border,
                                   roles.daily_mix_chip.fg);
            access.recolor_surface(handles.home_recently_played_chip,
                                   roles.recently_played_chip.bg,
                                   roles.recently_played_chip.border,
                                   roles.recently_played_chip.fg);
            access.recolor_surface(handles.home_recently_played_action,
                                   roles.recently_played_action.bg,
                                   roles.recently_played_action.border,
                                   roles.recently_played_action.fg);
            access.recolor_surface(handles.home_stats_header_band,
                                   roles.stats_band.bg,
                                   roles.stats_band.border,
                                   roles.stats_band.fg);
            access.recolor_surface(handles.home_stats_action,
                                   roles.stats_action.bg,
                                   roles.stats_action.border,
                                   roles.stats_action.fg);
            for (std::size_t i = 0; i < handles.home_daily_mix_preview_plates.size(); ++i) {
                const auto& role = (i == 1) ? roles.daily_mix_preview_plate_focus
                                            : roles.daily_mix_preview_plate;
                access.recolor_surface(handles.home_daily_mix_preview_plates[i],
                                       role.bg,
                                       role.border,
                                       role.fg);
            }
            for (std::size_t i = 0; i < handles.home_recently_played_preview_plates.size(); ++i) {
                const auto& role =
                    ((i == 0) || (i + 1 == handles.home_recently_played_preview_plates.size()))
                        ? roles.recently_played_preview_plate_focus
                        : roles.recently_played_preview_plate;
                access.recolor_surface(handles.home_recently_played_preview_plates[i],
                                       role.bg,
                                       role.border,
                                       role.fg);
            }

            const auto stamp = player::time_utils::current_week_stamp();
            for (std::size_t i = 0; i < handles.home_stats_bars.size(); ++i) {
                const WidgetHandle bar = handles.home_stats_bars[i];
                if (!bar) continue;
                StylePatch patch{};
                patch.has_bg_color = true;
                patch.bg_color = (static_cast<int>(i) == stamp.day_index)
                    ? roles.stats_bar_active
                    : roles.stats_bar;
                patch.has_border_color = true;
                patch.border_color = {0, 0, 0, 0};
                patch.has_border_width = true;
                patch.border_width = 0;
                patch.has_corner_radius = true;
                patch.corner_radius = handles.home_stats_bar_slots[i].w / 2;
                access.set_style_override(bar, patch);
            }
        }

        void clear_home_collage_images() {
            if (!access.valid()) return;
            const auto clear_slots = [&](const auto& slots, bool show_plate_when_empty) {
                for (const auto& slot : slots) {
                    if (!home_image_slot_ready(slot)) continue;
                    clear_image_slot_if_present(slot, show_plate_when_empty);
                }
            };
            clear_slots(home_cover_slots(), false);
            clear_slots(home_daily_mix_slots(), true);
            clear_slots(home_recently_played_slots(), true);
        }

        void sync_home_collage_images(bool force = false) {
            if (!access.valid()) return;

            const auto track_plan = build_home_collage_track_plan();
            const auto next_track_indices = track_plan.track_indices();
            if (!home_collage_selection_changed(next_track_indices, force)) return;

            const auto* tracks = storage.tracks;
            if (!tracks || tracks->size() == 0 || track_plan.empty()) {
                clear_home_collage_images();
                last_home_collage_track_indices = next_track_indices;
                return;
            }

            const int count = static_cast<int>(tracks->size());
            const int start = home_collage_start_track_index(count);

            bool retry_needed = false;
            HomeResolvedCollageImages resolved{};
            const auto cover_slots = home_cover_slots();

            for (std::size_t i = 0; i < kHomeCollageSlots; ++i) {
                const auto image_slot = cover_slots[i];
                if (!home_image_slot_ready(image_slot)) continue;

                bool assigned = false;
                const auto& preferred_ref = track_plan.refs[i];
                const auto preferred_result = try_assign_home_collage_slot_image(
                    image_slot,
                    preferred_ref,
                    resolved);
                if (preferred_result == HomeCollageResolveResult::Assigned) {
                    assigned = true;
                } else if (preferred_result == HomeCollageResolveResult::RetryLater) {
                    retry_needed = true;
                }

                if (!assigned && !retry_needed) {
                    for (int offset = 0; offset < count; ++offset) {
                        const int candidate = (start + offset) % count;
                        if (candidate == preferred_ref.track_index) continue;
                        HomeCollageTrackRef fallback_ref{};
                        if (!resolve_home_collage_track_ref(candidate, fallback_ref)) continue;
                        const auto result = try_assign_home_collage_slot_image(
                            image_slot,
                            fallback_ref,
                            resolved);
                        if (result == HomeCollageResolveResult::Assigned) {
                            assigned = true;
                            break;
                        }
                        if (result == HomeCollageResolveResult::RetryLater) {
                            retry_needed = true;
                            break;
                        }
                    }
                }

                if (!assigned && !resolved.empty()) {
                    apply_home_collage_repeat_cover(image_slot, i, resolved);
                    assigned = true;
                }

                if (!assigned && !retry_needed) {
                    apply_home_collage_default_cover(image_slot, i);
                }
            }

            sync_home_preview_card_slots(home_daily_mix_slots(), 0, resolved, retry_needed);
            sync_home_preview_card_slots(home_recently_played_slots(), 3, resolved, retry_needed);

            if (!retry_needed) {
                last_home_collage_track_indices = next_track_indices;
            }
        }

        void apply_home_recent_default_cover(const ::ui::scene::ImageSlotHandles& image_slot,
                                             std::size_t slot_index) noexcept {
            set_image_slot_if_present(
                image_slot,
                default_cover_image_id(home_card_default_cover_variant(slot_index + 3)),
                true);
        }

        void clear_home_recent_images() {
            if (!access.valid()) return;
            for (const auto& slot : home_recently_played_slots()) {
                if (!home_image_slot_ready(slot)) continue;
                clear_image_slot_if_present(slot, true);
            }
        }

        void sync_home_recent_text() {
            const std::size_t count = recent_track_history.tracks.size();
            FixedString<192> body{};
            FixedString<32> meta{};

            if (count == 0) {
                body.assign("Play a track to build history");
                meta.assign("No plays");
            } else {
                std::string_view path = recent_track_history.tracks[0].path.view();
                const int idx = recent_track_storage_index(path);
                if (idx >= 0
                    && storage.track_titles
                    && idx < static_cast<int>(storage.track_titles->size())
                    && !(*storage.track_titles)[static_cast<std::size_t>(idx)].empty()) {
                    body.assign((*storage.track_titles)[static_cast<std::size_t>(idx)].view());
                    strip_audio_extension(body);
                } else {
                    const auto track = player::derive_media_track(path);
                    body.assign(track.title);
                }
                body.assign(library_trim_space(body.view()));
                if (body.empty()) {
                    body.assign("Last played track");
                }

                char meta_buf[32]{};
                std::snprintf(meta_buf,
                              sizeof(meta_buf),
                              "%u recent",
                              static_cast<unsigned>(count));
                meta.assign(meta_buf);
            }

            if (last_home_recent_body_text.view() != body.view()) {
                last_home_recent_body_text.assign(body.view());
                set_label_slot(handles.home_recently_played_body,
                               text_slots.home_recent_body,
                               last_home_recent_body_text.c_str());
            }
            if (last_home_recent_meta_text.view() != meta.view()) {
                last_home_recent_meta_text.assign(meta.view());
                set_label_slot(handles.home_recently_played_chip,
                               text_slots.home_recent_meta,
                               last_home_recent_meta_text.c_str());
            }
        }

        void sync_home_recent_images(bool force = false) {
            if (!access.valid()) return;

            const auto recent_plan = build_home_recent_track_plan();
            const auto next_track_indices = recent_plan.track_indices();
            if (!home_recent_selection_changed(next_track_indices, force)) return;

            if (recent_plan.empty()) {
                clear_home_recent_images();
                last_home_recent_track_indices = next_track_indices;
                return;
            }

            bool retry_needed = false;
            HomeResolvedCollageImages resolved{};
            const auto slots = home_recently_played_slots();
            for (std::size_t i = 0; i < slots.size(); ++i) {
                const auto image_slot = slots[i];
                if (!home_image_slot_ready(image_slot)) continue;

                const auto result = try_assign_home_collage_slot_image(
                    image_slot,
                    recent_plan.refs[i],
                    resolved);
                if (result == HomeCollageResolveResult::RetryLater) {
                    retry_needed = true;
                    continue;
                }
                if (result == HomeCollageResolveResult::Assigned) {
                    continue;
                }
                if (!resolved.empty()) {
                    apply_home_collage_repeat_cover(image_slot, i, resolved);
                } else {
                    apply_home_recent_default_cover(image_slot, i);
                }
            }

            if (!retry_needed) {
                last_home_recent_track_indices = next_track_indices;
            }
        }

        void sync_home_recent_content(bool force = false) {
            sync_home_recent_text();
            sync_home_recent_images(force);
        }

        bool home_page_active() const noexcept {
            return current_page == PlayerPage::Home;
        }

        void refresh_home_runtime_content() {
            update_duration_from_player();
            update_info_label();
        }

        void refresh_home_collage_content(bool force = false) {
            sync_home_collage_images(force);
        }

        void refresh_home_recent_content(bool force = false) {
            sync_home_recent_content(force);
        }

        void refresh_home_stats_content() {
            sync_weekly_listening_stats_card();
        }

        void refresh_home_visual_content() noexcept {
            apply_home_theme();
        }

        void refresh_home_dashboard_content(bool force_collage = false) {
            refresh_home_collage_content(force_collage);
            refresh_home_recent_content(force_collage);
            refresh_home_stats_content();
        }

        void refresh_home_body_content(bool force_collage = false) {
            refresh_home_runtime_content();
            refresh_home_dashboard_content(force_collage);
            refresh_home_visual_content();
        }

        void refresh_home_collage_if_active(bool force = false) {
            if (!home_page_active()) return;
            refresh_home_collage_content(force);
        }

        void refresh_home_recent_if_active(bool force = false) {
            if (!home_page_active()) return;
            refresh_home_recent_content(force);
        }

        void refresh_home_stats_if_active() {
            if (!home_page_active()) return;
            refresh_home_stats_content();
        }

        void refresh_home_dashboard_if_active(bool force_collage = false) {
            if (!home_page_active()) return;
            refresh_home_dashboard_content(force_collage);
        }

        void refresh_home_visual_if_active() noexcept {
            if (!home_page_active()) return;
            refresh_home_visual_content();
        }

        static std::uint64_t weekly_listening_now_ms() noexcept {
            return charm::system::ClockCaps::TimeSource::now();
        }

        bool weekly_listening_week_loaded() const noexcept {
            return weekly_listening_stats.week_key != 0;
        }

        bool weekly_listening_stats_dirty() const noexcept {
            return weekly_listening_stats.dirty;
        }

        bool weekly_listening_stats_needs_persist() const noexcept {
            return weekly_listening_stats.persist_dirty;
        }

        bool weekly_listening_session_active() const noexcept {
            return weekly_listening_stats.active;
        }

        void mark_weekly_listening_stats_changed(bool persist = true) noexcept {
            weekly_listening_stats.dirty = true;
            if (persist) {
                weekly_listening_stats.persist_dirty = true;
            }
        }

        void clear_weekly_listening_stats_dirty() noexcept {
            weekly_listening_stats.dirty = false;
        }

        void mark_weekly_listening_stats_persisted(std::uint64_t now_ms) noexcept {
            weekly_listening_stats.persist_dirty = false;
            weekly_listening_stats.last_persist_ms = now_ms;
        }

        void set_weekly_listening_session_active(bool active,
                                                 std::uint64_t now_ms = 0) noexcept {
            weekly_listening_stats.active = active;
            weekly_listening_stats.last_tick_ms = active ? now_ms : 0;
        }

        void reset_weekly_listening_week_data(int week_key) noexcept {
            weekly_listening_stats.week_key = week_key;
            weekly_listening_stats.seconds.fill(0);
            weekly_listening_stats.total_plays = 0;
            weekly_listening_stats.last_tick_ms = 0;
            weekly_listening_stats.last_persist_ms = 0;
        }

        void restore_weekly_listening_week_data(const ListeningStatsWeekRecord& rec) noexcept {
            weekly_listening_stats.week_key = rec.week_key;
            weekly_listening_stats.seconds = rec.seconds;
            weekly_listening_stats.total_plays = rec.total_plays;
            weekly_listening_stats.last_tick_ms = 0;
            weekly_listening_stats.last_persist_ms = 0;
        }

        void add_weekly_listening_time_slice(int day_index, int add_sec) noexcept {
            if (add_sec <= 0) return;
            if (day_index < 0
                || day_index >= static_cast<int>(weekly_listening_stats.seconds.size())) {
                return;
            }
            weekly_listening_stats.seconds[static_cast<std::size_t>(day_index)] += add_sec;
            weekly_listening_stats.last_tick_ms += static_cast<std::uint64_t>(add_sec) * 1000;
            mark_weekly_listening_stats_changed();
        }

        void increment_weekly_listening_play_count() noexcept {
            ++weekly_listening_stats.total_plays;
            mark_weekly_listening_stats_changed();
        }

        bool should_flush_weekly_listening_stats(std::uint64_t now_ms) const noexcept {
            if (!weekly_listening_stats_needs_persist()) return false;
            return !weekly_listening_session_active()
                || weekly_listening_stats.last_persist_ms == 0
                || now_ms - weekly_listening_stats.last_persist_ms >= 15000;
        }

        void refresh_home_stats_if_dirty() {
            if (!weekly_listening_stats_dirty()) return;
            refresh_home_stats_if_active();
        }

        void flush_weekly_listening_stats_if_needed(std::uint64_t now_ms) {
            if (!should_flush_weekly_listening_stats(now_ms)) return;
            (void)flush_weekly_listening_stats_history();
        }

        void reset_weekly_listening_stats_for_week(int week_key,
                                                   bool persist = true) noexcept {
            reset_weekly_listening_week_data(week_key);
            mark_weekly_listening_stats_changed(persist);
        }

        void load_weekly_listening_stats_from_history(int week_key) noexcept {
            const auto now_ms = weekly_listening_now_ms();
            if (const auto* rec = player::find_listening_stats_week(weekly_listening_history, week_key)) {
                restore_weekly_listening_week_data(*rec);
                mark_weekly_listening_stats_changed(false);
            } else {
                reset_weekly_listening_stats_for_week(week_key, false);
            }
            mark_weekly_listening_stats_persisted(now_ms);
        }

        void persist_current_weekly_listening_stats() noexcept {
            if (!weekly_listening_week_loaded()) return;
            ListeningStatsWeekRecord rec{};
            rec.week_key = weekly_listening_stats.week_key;
            rec.seconds = weekly_listening_stats.seconds;
            rec.total_plays = weekly_listening_stats.total_plays;
            player::upsert_listening_stats_week(weekly_listening_history, rec);
        }

        void ensure_weekly_listening_week() noexcept {
            const auto stamp = player::time_utils::current_week_stamp();
            if (weekly_listening_stats.week_key != stamp.key) {
                if (weekly_listening_week_loaded() && weekly_listening_stats_needs_persist()) {
                    persist_current_weekly_listening_stats();
                    (void)player::save_listening_stats_history(weekly_listening_history);
                }
                const bool active = weekly_listening_session_active();
                load_weekly_listening_stats_from_history(stamp.key);
                set_weekly_listening_session_active(active, weekly_listening_now_ms());
            }
        }

        void load_weekly_listening_stats_history() {
            if (!fs_ready) return;
            weekly_listening_history = player::load_listening_stats_history();
            const auto stamp = player::time_utils::current_week_stamp();
            load_weekly_listening_stats_from_history(stamp.key);
            refresh_home_dashboard_if_active(true);
        }

        bool flush_weekly_listening_stats_history() {
            if (!fs_ready) return false;
            ensure_weekly_listening_week();
            persist_current_weekly_listening_stats();
            const bool ok = player::save_listening_stats_history(weekly_listening_history);
            if (ok) {
                mark_weekly_listening_stats_persisted(weekly_listening_now_ms());
            }
            return ok;
        }

        bool recent_track_history_needs_persist() const noexcept {
            return recent_track_history_dirty;
        }

        bool should_flush_recent_track_history(std::uint64_t now_ms) const noexcept {
            if (!recent_track_history_needs_persist()) return false;
            return last_recent_track_persist_ms == 0
                || now_ms - last_recent_track_persist_ms >= 5000;
        }

        void mark_recent_track_history_changed() noexcept {
            recent_track_history_dirty = true;
        }

        void mark_recent_track_history_persisted(std::uint64_t now_ms) noexcept {
            recent_track_history_dirty = false;
            last_recent_track_persist_ms = now_ms;
        }

        void load_recent_track_history() {
            if (!fs_ready) return;
            recent_track_history = player::load_recent_track_history();
            recent_track_history_dirty = false;
            last_recent_track_persist_ms = weekly_listening_now_ms();
            refresh_home_recent_if_active(true);
        }

        bool flush_recent_track_history() {
            if (!fs_ready) return false;
            if (!recent_track_history_needs_persist()) return true;
            const bool ok = player::save_recent_track_history(recent_track_history);
            if (ok) {
                mark_recent_track_history_persisted(weekly_listening_now_ms());
            }
            return ok;
        }

        void flush_recent_track_history_if_needed(std::uint64_t now_ms) {
            if (!should_flush_recent_track_history(now_ms)) return;
            (void)flush_recent_track_history();
        }

        void record_current_track_as_recent() {
            if (!fs_ready || !playback.track_ready()) return;
            const char* path = playback.track_path();
            if (!path || !*path) return;

            const std::uint64_t now_ms = weekly_listening_now_ms();
            player::upsert_recent_track(recent_track_history, path, now_ms);
            mark_recent_track_history_changed();
            last_home_recent_track_indices.fill(-2);
            refresh_home_recent_if_active(true);
            flush_recent_track_history_if_needed(now_ms);
        }

        void sync_weekly_listening_stats_card() {
            if (!access.valid()) return;
            ensure_weekly_listening_week();

            int total_sec = 0;
            int max_sec = 0;
            for (int value : weekly_listening_stats.seconds) {
                total_sec += value;
                if (value > max_sec) max_sec = value;
            }
            const int avg_sec = (total_sec > 0) ? ((total_sec + 6) / 7) : 0;

            char total_buf[32]{};
            char plays_buf[16]{};
            char avg_buf[32]{};
            format_listening_duration_long(total_sec, total_buf, sizeof(total_buf));
            std::snprintf(plays_buf, sizeof(plays_buf), "%d", weekly_listening_stats.total_plays);
            format_listening_duration_compact(avg_sec, avg_buf, sizeof(avg_buf));

            if (last_home_stats_total_text.view() != total_buf) {
                last_home_stats_total_text.assign(total_buf);
                set_label_slot(handles.home_stats_total, text_slots.home_stats_total, total_buf);
            }
            if (last_home_stats_plays_text.view() != plays_buf) {
                last_home_stats_plays_text.assign(plays_buf);
                set_label_slot(handles.home_stats_plays, text_slots.home_stats_plays, plays_buf);
            }
            if (last_home_stats_avg_text.view() != avg_buf) {
                last_home_stats_avg_text.assign(avg_buf);
                set_label_slot(handles.home_stats_avg, text_slots.home_stats_avg, avg_buf);
            }

            for (std::size_t i = 0; i < handles.home_stats_bars.size(); ++i) {
                const WidgetHandle bar = handles.home_stats_bars[i];
                if (!bar) continue;
                const Rect slot = handles.home_stats_bar_slots[i];
                const int seconds = weekly_listening_stats.seconds[i];
                if (max_sec <= 0 || seconds <= 0) {
                    access.set_visible(bar, false);
                    continue;
                }
                access.set_visible(bar, true);
                const int fill_h = std::max<int>(
                    8,
                    static_cast<int>((slot.h * seconds + max_sec - 1) / max_sec));
                access.set_rect(bar, {slot.x, slot.y + slot.h - fill_h, slot.w, fill_h});
            }

            clear_weekly_listening_stats_dirty();
        }

        void tick_weekly_listening_stats() {
            ensure_weekly_listening_week();
            const std::uint64_t now_ms = weekly_listening_now_ms();
            if (weekly_listening_session_active()) {
                if (weekly_listening_stats.last_tick_ms == 0) {
                    set_weekly_listening_session_active(true, now_ms);
                } else if (now_ms > weekly_listening_stats.last_tick_ms) {
                    const std::uint64_t delta_ms = now_ms - weekly_listening_stats.last_tick_ms;
                    const int add_sec = static_cast<int>(delta_ms / 1000);
                    if (add_sec > 0) {
                        const auto stamp = player::time_utils::current_week_stamp();
                        if (weekly_listening_stats.week_key != stamp.key) {
                            load_weekly_listening_stats_from_history(stamp.key);
                            set_weekly_listening_session_active(true, now_ms);
                        }
                        add_weekly_listening_time_slice(stamp.day_index, add_sec);
                    }
                }
            }
            refresh_home_stats_if_dirty();
            flush_weekly_listening_stats_if_needed(now_ms);
            flush_recent_track_history_if_needed(now_ms);
        }

        void begin_weekly_listening_session(bool count_play) {
            ensure_weekly_listening_week();
            if (count_play) {
                increment_weekly_listening_play_count();
            }
            set_weekly_listening_session_active(true, weekly_listening_now_ms());
            refresh_home_stats_if_dirty();
        }

        void end_weekly_listening_session() {
            tick_weekly_listening_stats();
            set_weekly_listening_session_active(false);
            flush_weekly_listening_stats_if_needed(weekly_listening_now_ms());
        }
