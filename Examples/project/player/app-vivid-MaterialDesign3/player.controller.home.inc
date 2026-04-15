        struct WeekStamp {
            int key{0};
            int day_index{0};
        };

        static void local_time_from_epoch(std::time_t epoch, std::tm& out) noexcept {
#if defined(_WIN32)
            localtime_s(&out, &epoch);
#else
            localtime_r(&epoch, &out);
#endif
        }

        static WeekStamp current_week_stamp() noexcept {
            const std::time_t now_epoch = std::time(nullptr);
            std::tm local{};
            local_time_from_epoch(now_epoch, local);
            const int monday_index = (local.tm_wday + 6) % 7;
            const std::time_t monday_epoch = now_epoch - static_cast<std::time_t>(monday_index) * 24 * 60 * 60;
            std::tm monday{};
            local_time_from_epoch(monday_epoch, monday);
            const int key = (monday.tm_year + 1900) * 10000
                + (monday.tm_mon + 1) * 100
                + monday.tm_mday;
            return WeekStamp{key, monday_index};
        }

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

        auto home_cover_handles() const noexcept {
            return std::array<WidgetHandle, kHomeCollageSlots>{
                handles.home_cover_big,
                handles.home_cover_left,
                handles.home_cover_right,
                handles.home_cover_small,
                handles.home_cover_bottom_left,
                handles.home_cover_bottom_right,
            };
        }

        auto home_daily_mix_cover_handles() const noexcept {
            return handles.home_daily_mix_covers;
        }

        auto home_recently_played_cover_handles() const noexcept {
            return handles.home_recently_played_covers;
        }

        void clear_home_collage_images() {
            if (!access.valid()) return;
            const auto clear_range = [&](const auto& handles_range) {
                for (WidgetHandle handle : handles_range) {
                    if (handle && access.kind(handle) == WidgetKind::Image) {
                        access.set_image(handle, ::ui::scene::invalid_image_id());
                    }
                }
            };
            clear_range(home_cover_handles());
            clear_range(home_daily_mix_cover_handles());
            clear_range(home_recently_played_cover_handles());
        }

        bool home_collage_cover_key_for_track(int idx,
                                              FixedString<260>& out_cover_path,
                                              FixedString<260>& out_key) const {
            const auto* tracks = storage.tracks;
            if (!tracks || idx < 0 || idx >= static_cast<int>(tracks->size())) return false;
            const std::string_view track_path = (*tracks)[static_cast<std::size_t>(idx)].view();
            if (track_path.empty()) return false;

            if (!resolve_cover_path_for_track(track_path, out_cover_path) || out_cover_path.empty()) {
                out_cover_path.assign(track_path);
            }
            if (out_cover_path.empty()) return false;

            if (out_cover_path.view() != track_path) {
                out_key.assign(out_cover_path.view());
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

        void rebuild_home_collage_track_indices() {
            home_collage_track_indices.fill(-1);
            const auto* tracks = storage.tracks;
            if (!tracks || tracks->size() == 0) return;

            const int count = static_cast<int>(tracks->size());
            int start = track_index;
            if (start < 0 || start >= count) start = 0;

            std::array<FixedString<260>, kHomeCollageSlots> used_keys{};
            std::size_t used_count = 0;
            std::size_t out_count = 0;

            const auto try_push = [&](int candidate) -> bool {
                if (candidate < 0 || candidate >= count) return false;
                FixedString<260> cover_path;
                FixedString<260> key;
                if (!home_collage_cover_key_for_track(candidate, cover_path, key) || key.empty()) {
                    return false;
                }
                for (std::size_t i = 0; i < used_count; ++i) {
                    if (used_keys[i].view() == key.view()) {
                        return false;
                    }
                }
                used_keys[used_count++].assign(key.view());
                home_collage_track_indices[out_count++] = candidate;
                return true;
            };

            (void)try_push(start);
            for (int offset = 1; offset < count && out_count < kHomeCollageSlots; ++offset) {
                (void)try_push((start + offset) % count);
            }
            if (out_count == 0) {
                for (int idx = 0; idx < count && out_count < kHomeCollageSlots; ++idx) {
                    home_collage_track_indices[out_count++] = idx;
                }
            }
            if (out_count == 0) return;

            const std::size_t unique_count = out_count;
            while (out_count < kHomeCollageSlots) {
                home_collage_track_indices[out_count] =
                    home_collage_track_indices[out_count % unique_count];
                ++out_count;
            }
        }

        void sync_home_collage_images(bool force = false) {
            if (!access.valid()) return;

            rebuild_home_collage_track_indices();
            bool changed = force;
            for (std::size_t i = 0; i < kHomeCollageSlots; ++i) {
                if (home_collage_track_indices[i] != last_home_collage_track_indices[i]) {
                    changed = true;
                    break;
                }
            }
            if (!changed) return;

            const auto* tracks = storage.tracks;
            if (!tracks || tracks->size() == 0) {
                clear_home_collage_images();
                last_home_collage_track_indices = home_collage_track_indices;
                return;
            }

            const int count = static_cast<int>(tracks->size());
            int start = track_index;
            if (start < 0 || start >= count) start = 0;

            bool retry_needed = false;
            std::array<int, kHomeCollageSlots> resolved_indices{};
            resolved_indices.fill(-1);
            std::array<::ui::scene::ImageId, kHomeCollageSlots> resolved_images{};
            std::array<int, kHomeCollageSlots> resolved_unique_indices{};
            std::array<FixedString<260>, kHomeCollageSlots> used_keys{};
            std::size_t resolved_count = 0;
            const auto cover_handles = home_cover_handles();

            const auto try_assign = [&](WidgetHandle handle, std::size_t slot, int candidate) -> int {
                if (candidate < 0 || candidate >= count) return 0;

                FixedString<260> cover_path;
                FixedString<260> cover_key;
                if (!home_collage_cover_key_for_track(candidate, cover_path, cover_key) || cover_key.empty()) {
                    return 0;
                }
                for (std::size_t i = 0; i < resolved_count; ++i) {
                    if (used_keys[i].view() == cover_key.view()) {
                        return 0;
                    }
                }

                const std::string_view track_path = (*tracks)[static_cast<std::size_t>(candidate)].view();
                const auto image_id = resolve_list_cover_icon(track_path, cover_path.view());
                if (::ui::scene::image_id_valid(image_id)) {
                    access.set_image(handle, image_id);
                    resolved_indices[slot] = candidate;
                    resolved_images[resolved_count] = image_id;
                    resolved_unique_indices[resolved_count] = candidate;
                    used_keys[resolved_count].assign(cover_key.view());
                    ++resolved_count;
                    return 1;
                }
                return ::ui::gfx::image_registry_locked() ? -1 : 0;
            };

            for (std::size_t i = 0; i < kHomeCollageSlots; ++i) {
                const WidgetHandle handle = cover_handles[i];
                if (!handle || access.kind(handle) != WidgetKind::Image) continue;

                bool assigned = false;
                const int preferred_idx = home_collage_track_indices[i];
                const int preferred_result = try_assign(handle, i, preferred_idx);
                if (preferred_result > 0) {
                    assigned = true;
                } else if (preferred_result < 0) {
                    retry_needed = true;
                }

                if (!assigned && !retry_needed) {
                    for (int offset = 0; offset < count; ++offset) {
                        const int candidate = (start + offset) % count;
                        if (candidate == preferred_idx) continue;
                        const int result = try_assign(handle, i, candidate);
                        if (result > 0) {
                            assigned = true;
                            break;
                        }
                        if (result < 0) {
                            retry_needed = true;
                            break;
                        }
                    }
                }

                if (!assigned && resolved_count > 0) {
                    const std::size_t repeat_index = i % resolved_count;
                    access.set_image(handle, resolved_images[repeat_index]);
                    resolved_indices[i] = resolved_unique_indices[repeat_index];
                    assigned = true;
                }

                if (!assigned && !retry_needed) {
                    access.set_image(handle, ::ui::scene::invalid_image_id());
                }
            }

            const auto sync_card_covers = [&](const auto& handles_range, std::size_t start_index) {
                for (std::size_t i = 0; i < handles_range.size(); ++i) {
                    const WidgetHandle handle = handles_range[i];
                    if (!handle || access.kind(handle) != WidgetKind::Image) continue;
                    if (resolved_count == 0) {
                        if (!retry_needed) {
                            access.set_image(handle, ::ui::scene::invalid_image_id());
                        }
                        continue;
                    }
                    access.set_image(handle, resolved_images[(start_index + i) % resolved_count]);
                }
            };

            sync_card_covers(home_daily_mix_cover_handles(), 0);
            sync_card_covers(home_recently_played_cover_handles(), 3);

            if (!retry_needed) {
                last_home_collage_track_indices = home_collage_track_indices;
            }
        }

        void reset_weekly_listening_stats_for_week(int week_key) noexcept {
            weekly_listening_stats.week_key = week_key;
            weekly_listening_stats.seconds.fill(0);
            weekly_listening_stats.total_plays = 0;
            weekly_listening_stats.last_tick_ms = 0;
            weekly_listening_stats.last_persist_ms = 0;
            weekly_listening_stats.dirty = true;
            weekly_listening_stats.persist_dirty = true;
        }

        void load_weekly_listening_stats_from_history(int week_key) noexcept {
            if (const auto* rec = player::find_listening_stats_week(weekly_listening_history, week_key)) {
                weekly_listening_stats.week_key = rec->week_key;
                weekly_listening_stats.seconds = rec->seconds;
                weekly_listening_stats.total_plays = rec->total_plays;
                weekly_listening_stats.last_tick_ms = 0;
                weekly_listening_stats.dirty = true;
                weekly_listening_stats.persist_dirty = false;
            } else {
                reset_weekly_listening_stats_for_week(week_key);
                weekly_listening_stats.persist_dirty = false;
            }
            weekly_listening_stats.last_persist_ms = charm::system::ClockCaps::TimeSource::now();
        }

        void persist_current_weekly_listening_stats() noexcept {
            if (weekly_listening_stats.week_key == 0) return;
            ListeningStatsWeekRecord rec{};
            rec.week_key = weekly_listening_stats.week_key;
            rec.seconds = weekly_listening_stats.seconds;
            rec.total_plays = weekly_listening_stats.total_plays;
            player::upsert_listening_stats_week(weekly_listening_history, rec);
        }

        void ensure_weekly_listening_week() noexcept {
            const WeekStamp stamp = current_week_stamp();
            if (weekly_listening_stats.week_key != stamp.key) {
                if (weekly_listening_stats.week_key != 0 && weekly_listening_stats.persist_dirty) {
                    persist_current_weekly_listening_stats();
                    (void)player::save_listening_stats_history(weekly_listening_history);
                }
                const bool active = weekly_listening_stats.active;
                load_weekly_listening_stats_from_history(stamp.key);
                weekly_listening_stats.active = active;
                weekly_listening_stats.last_tick_ms = active ? charm::system::ClockCaps::TimeSource::now() : 0;
            }
        }

        void load_weekly_listening_stats_history() {
            if (!fs_ready) return;
            weekly_listening_history = player::load_listening_stats_history();
            const WeekStamp stamp = current_week_stamp();
            load_weekly_listening_stats_from_history(stamp.key);
            sync_weekly_listening_stats_card();
            sync_home_collage_images(true);
        }

        bool flush_weekly_listening_stats_history() {
            if (!fs_ready) return false;
            ensure_weekly_listening_week();
            persist_current_weekly_listening_stats();
            const bool ok = player::save_listening_stats_history(weekly_listening_history);
            if (ok) {
                weekly_listening_stats.persist_dirty = false;
                weekly_listening_stats.last_persist_ms = charm::system::ClockCaps::TimeSource::now();
            }
            return ok;
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
                const int fill_h = std::max(8, (slot.h * seconds + max_sec - 1) / max_sec);
                access.set_rect(bar, {slot.x, slot.y + slot.h - fill_h, slot.w, fill_h});
            }

            weekly_listening_stats.dirty = false;
        }

        void tick_weekly_listening_stats() {
            ensure_weekly_listening_week();
            if (weekly_listening_stats.active) {
                const std::uint64_t now_ms = charm::system::ClockCaps::TimeSource::now();
                if (weekly_listening_stats.last_tick_ms == 0) {
                    weekly_listening_stats.last_tick_ms = now_ms;
                } else if (now_ms > weekly_listening_stats.last_tick_ms) {
                    const std::uint64_t delta_ms = now_ms - weekly_listening_stats.last_tick_ms;
                    const int add_sec = static_cast<int>(delta_ms / 1000);
                    if (add_sec > 0) {
                        const WeekStamp stamp = current_week_stamp();
                        if (weekly_listening_stats.week_key != stamp.key) {
                            load_weekly_listening_stats_from_history(stamp.key);
                            weekly_listening_stats.active = true;
                            weekly_listening_stats.last_tick_ms = now_ms;
                        }
                        weekly_listening_stats.seconds[stamp.day_index] += add_sec;
                        weekly_listening_stats.last_tick_ms += static_cast<std::uint64_t>(add_sec) * 1000;
                        weekly_listening_stats.dirty = true;
                        weekly_listening_stats.persist_dirty = true;
                    }
                }
            }
            if (weekly_listening_stats.dirty) {
                sync_weekly_listening_stats_card();
            }
            if (weekly_listening_stats.persist_dirty) {
                const std::uint64_t now_ms = charm::system::ClockCaps::TimeSource::now();
                if (!weekly_listening_stats.active
                    || weekly_listening_stats.last_persist_ms == 0
                    || now_ms - weekly_listening_stats.last_persist_ms >= 15000) {
                    (void)flush_weekly_listening_stats_history();
                }
            }
        }

        void begin_weekly_listening_session(bool count_play) {
            ensure_weekly_listening_week();
            if (count_play) {
                ++weekly_listening_stats.total_plays;
                weekly_listening_stats.dirty = true;
                weekly_listening_stats.persist_dirty = true;
            }
            weekly_listening_stats.active = true;
            weekly_listening_stats.last_tick_ms = charm::system::ClockCaps::TimeSource::now();
            if (weekly_listening_stats.dirty) {
                sync_weekly_listening_stats_card();
            }
        }

        void end_weekly_listening_session() {
            tick_weekly_listening_stats();
            weekly_listening_stats.active = false;
            weekly_listening_stats.last_tick_ms = 0;
            if (weekly_listening_stats.persist_dirty) {
                (void)flush_weekly_listening_stats_history();
            }
        }
