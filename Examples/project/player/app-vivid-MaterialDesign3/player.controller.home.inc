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
