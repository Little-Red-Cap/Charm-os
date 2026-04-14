        void update_debug_overlay() {
#if CHARM_PLAYER_DEBUG_UI
            if (!access.valid() || !handles.debug_text) return;
            const auto now_ms = charm::system::ClockCaps::TimeSource::now();
            if (last_debug_tick_ms != 0) {
                const auto dt = now_ms - last_debug_tick_ms;
                if (dt < 500) return;
            }
            last_debug_tick_ms = now_ms;

            audio::PlayerSnapshot snap{};
            if (!playback.snapshot(snap)) return;
            const auto& fmt = snap.output_fmt;
            const std::uint64_t frame_size = fmt.frame_size();
            const std::uint64_t bytes_per_sec = frame_size * fmt.rate;
            const std::uint64_t water_ms = bytes_per_sec ? (snap.water_bytes * 1000 / bytes_per_sec) : 0;
            const std::uint64_t low_ms = bytes_per_sec ? (snap.low_water * 1000 / bytes_per_sec) : 0;
            const std::uint64_t high_ms = bytes_per_sec ? (snap.high_water * 1000 / bytes_per_sec) : 0;

            const auto pump_min_ms = (snap.pump.has_water && bytes_per_sec)
                ? (snap.pump.water_min * 1000 / bytes_per_sec)
                : 0;
            const auto pump_max_ms = (snap.pump.has_water && bytes_per_sec)
                ? (snap.pump.water_max * 1000 / bytes_per_sec)
                : 0;

            char buf[192]{};
            const auto font_stats = player::font_cache::ready()
                ? player::font_cache::stats()
                : player::font_cache::Stats{};
            const bool font_on = player::font_cache::ready();
            std::uint32_t missing_glyphs = 0;
            std::uint32_t missing_fallbacks = 0;
            std::uint32_t utf8_replaces = 0;
#if defined(VIVID_SOA_TRACE_INPUT)
            missing_glyphs = missing_glyph_count();
            missing_fallbacks = missing_glyph_fallback_count();
            utf8_replaces = utf8_replacement_count();
#endif
            std::snprintf(buf, sizeof(buf),
                          "water %llums (%llu..%llu) pump %llu..%llu underrun %llu/%llu font %s %u/%u/%u glyph %u/%u/%u",
                          static_cast<unsigned long long>(water_ms),
                          static_cast<unsigned long long>(low_ms),
                          static_cast<unsigned long long>(high_ms),
                          static_cast<unsigned long long>(pump_min_ms),
                          static_cast<unsigned long long>(pump_max_ms),
                          static_cast<unsigned long long>(snap.stats.underrun_count),
                          static_cast<unsigned long long>(snap.pump.underrun_count),
                          font_on ? "on" : "off",
                          static_cast<unsigned>(font_stats.requests),
                          static_cast<unsigned>(font_stats.cached),
                          static_cast<unsigned>(font_stats.missing),
                          static_cast<unsigned>(missing_glyphs),
                          static_cast<unsigned>(missing_fallbacks),
                          static_cast<unsigned>(utf8_replaces));
            if (last_debug_text.view() == buf) return;
            last_debug_text.assign(buf);
            set_label_slot(handles.debug_text, text_slots.debug_text, buf);

            if (dbg_style_button == static_cast<std::size_t>(-1)) {
                dbg_style_button = perf_overlay_debug_channel("style.button");
                dbg_style_progress = perf_overlay_debug_channel("style.progress");
            }
            {
                const auto mask = style_kind_state_mask(WidgetKind::Button);
                const auto count = style_kind_state_count(WidgetKind::Button);
                const auto offset = style_kind_state_offset(WidgetKind::Button);
                char line[96]{};
                std::snprintf(line, sizeof(line),
                              "btn mask=0x%02X cnt=%u off=%u",
                              static_cast<unsigned>(mask),
                              static_cast<unsigned>(count),
                              static_cast<unsigned>(offset));
                set_perf_overlay_debug_channel(dbg_style_button, line);
            }
            {
                const auto mask = style_kind_state_mask(WidgetKind::ProgressBarSimple);
                const auto count = style_kind_state_count(WidgetKind::ProgressBarSimple);
                const auto offset = style_kind_state_offset(WidgetKind::ProgressBarSimple);
                char line[96]{};
                std::snprintf(line, sizeof(line),
                              "prog mask=0x%02X cnt=%u off=%u",
                              static_cast<unsigned>(mask),
                              static_cast<unsigned>(count),
                              static_cast<unsigned>(offset));
                set_perf_overlay_debug_channel(dbg_style_progress, line);
            }
#else
            (void)this;
#endif
        }

        void sync_progress_value(int value) {
            if (!access.valid()) return;
            if (handles.progress_visual) {
                access.set_value(handles.progress_visual, value);
            }
        }

        int progress_value_from_x(int x) const {
            if (!access.valid()) return 0;
            const Rect r = access.world_rect(handles.progress);
            if (r.w <= 1) return 0;
            const int dx = std::clamp(x - r.x, 0, r.w);
            return (dx * 100) / r.w;
        }

        int progress_sec_from_value(int value) const {
            const int duration_sec = playback.duration_sec();
            if (duration_sec <= 0) return 0;
            const int clamped = std::clamp(value, 0, 100);
            return (clamped * duration_sec) / 100;
        }

        struct PendingActions {
            bool prev{false};
            bool next{false};
            bool toggle_play{false};
            bool cycle_mode{false};
            bool seek{false};
            int seek_sec{0};
            bool select{false};
            int select_index{-1};
            bool list_action{false};
            int list_action_index{-1};
        };

        void update_progress_drag(int x) {
            progress_drag_value = progress_value_from_x(x);
            progress_drag_sec = progress_sec_from_value(progress_drag_value);
            sync_progress_value(progress_drag_value);
            set_time_label(progress_drag_sec);
        }

        void end_progress_drag(bool apply_seek, PendingActions& actions) {
            if (!progress_dragging) return;
            progress_dragging = false;
            if (apply_seek) {
                actions.seek = true;
                actions.seek_sec = progress_drag_sec;
            }
        }

        #include "player.controller.playback.inc"
        void cycle_list_sort() {
            close_list_action_menu();
            if (list_shuffle_enabled) {
                list_shuffle_enabled = false;
            }
            list_sort = (list_sort == ListSort::NameAsc) ? ListSort::NameDesc : ListSort::NameAsc;
            refresh_list_view();
        }

        void focus_list() {
            if (!access.valid() || !handles.list) return;
            access.set_focused(handles.list, true);
        }

        void nav_list(int delta) {
            if (!access.valid() || !handles.list) return;
            const int count = static_cast<int>(list_order.size());
            if (count <= 0) return;
            int selected = access.list_view_selected(handles.list);
            if (selected < 0) selected = 0;
            selected += delta;
            if (selected < 0) selected = 0;
            if (selected >= count) selected = count - 1;
            access.set_list_view_selected(handles.list, selected);
        }

        void nav_list_activate() {
            if (!access.valid() || !handles.list) return;
            const int selected = access.list_view_selected(handles.list);
            activate_list_index(selected);
        }

        void sync_eq_values() {
            if (!access.valid()) return;
            bool changed = false;
            for (std::size_t i = 0; i < kEqBands; ++i) {
                if (!handles.eq_sliders[i] || !handles.eq_values[i]) continue;
                const int value = access.value(handles.eq_sliders[i]);
                if (value == last_eq_values[i]) continue;
                last_eq_values[i] = value;
                eq_values[i] = value;
                changed = true;
                char buf[16]{};
                std::snprintf(buf, sizeof(buf), "%+d", value);
                set_label_slot(handles.eq_values[i], text_slots.eq_values[i], buf);
            }
            if (!changed) return;
            static constexpr std::array<std::uint32_t, kEqBands> kEqFreqs{
                60, 250, 1000, 4000, 16000
            };
            eq_config.enabled = true;
            eq_config.band_count = static_cast<std::uint8_t>(kEqBands);
            for (std::size_t i = 0; i < kEqBands; ++i) {
                eq_config.bands[i].freq_hz = kEqFreqs[i];
                eq_config.bands[i].gain_db = static_cast<float>(eq_values[i]);
                eq_config.bands[i].q = 1.0f;
            }
            FixedString<128> status;
            if (!playback.set_eq(eq_config, status) && !status.empty()) {
                set_status(status.c_str());
            }
        }

        void sync_volume_value() {
            if (!access.valid() || !handles.volume_slider || !handles.volume_value) return;
            const int value = access.value(handles.volume_slider);
            if (value == last_volume_value) return;
            last_volume_value = value;
            char buf[16]{};
            std::snprintf(buf, sizeof(buf), "%d", value);
            set_label_slot(handles.volume_value, text_slots.volume_value, buf);
            FixedString<128> status;
            if (!playback.set_volume(value, status) && !status.empty()) {
                set_status(status.c_str());
            }
        }

        void sync_dsp_controls() {
            if (!access.valid()) return;
            if (handles.dc_switch) {
                const int enabled = access.checked(handles.dc_switch) ? 1 : 0;
                if (enabled != last_dc_enabled) {
                    last_dc_enabled = enabled;
                    FixedString<128> status;
                    if (!playback.set_dc_block(enabled != 0, status) && !status.empty()) {
                        set_status(status.c_str());
                    }
                }
            }
            if (!handles.clip_switch || !handles.clip_slider || !handles.clip_value) return;
            const int enabled = access.checked(handles.clip_switch) ? 1 : 0;
            const int threshold = access.value(handles.clip_slider);
            bool changed = false;
            if (enabled != last_clip_enabled) {
                last_clip_enabled = enabled;
                changed = true;
            }
            if (threshold != last_clip_threshold) {
                last_clip_threshold = threshold;
                char buf[16]{};
                std::snprintf(buf, sizeof(buf), "%d", threshold);
                set_label_slot(handles.clip_value, text_slots.clip_value, buf);
                changed = true;
            }
            if (!changed) return;
            FixedString<128> status;
            if (!playback.set_soft_clip(enabled != 0, threshold, status) && !status.empty()) {
                set_status(status.c_str());
            }
        }

        void process_input_events() {
            if (!access.valid()) return;
            PendingActions actions{};
            const std::size_t count = access.input_event_count();
            for (std::size_t i = 0; i < count; ++i) {
                const auto& item = access.input_event(i);
                const auto target = item.target;
                const auto type = item.event.type;
                if (current_page == PlayerPage::Probe) {
                    if (type == Event::Type::MouseDown || type == Event::Type::MouseUp
                        || type == Event::Type::DragStart || type == Event::Type::DragEnd) {
                        dismiss_probe();
                    }
                    continue;
                }
                if (current_page == PlayerPage::NowPlaying) {
                    if (target == handles.progress) {
                        if (type == Event::Type::MouseDown) {
                            progress_dragging = true;
                            update_progress_drag(item.event.x);
                        } else if (type == Event::Type::DragStart || type == Event::Type::DragMove
                                   || type == Event::Type::MouseMove) {
                            if (progress_dragging) {
                                update_progress_drag(item.event.x);
                            }
                        } else if (type == Event::Type::MouseUp || type == Event::Type::DragEnd) {
                            if (progress_dragging) {
                                update_progress_drag(item.event.x);
                                end_progress_drag(true, actions);
                            }
                        } else if (type == Event::Type::Cancel) {
                            end_progress_drag(false, actions);
                        }
                        continue;
                    }
                if (type == Event::Type::MouseUp) {
                    if (target == handles.now_back) {
                        set_page(return_page);
                    } else if (target == handles.btn_prev) {
                        actions.prev = true;
                    } else if (target == handles.btn_next) {
                        actions.next = true;
                    } else if (target == handles.btn_pause) {
                        actions.toggle_play = true;
                    } else if (target == handles.btn_mode) {
                        actions.cycle_mode = true;
#if defined(CHARM_PLAYER_DEBUG_UI)
                    } else if (target == handles.cover) {
                        cycle_palette_style();
                    } else if (target == handles.now_backdrop) {
                        cycle_cover_theme();
#endif
                    }
                }
                } else {
                    if (type == Event::Type::MouseUp) {
                        if (target == handles.list_action_scrim) {
                            close_list_action_menu();
                        } else if (target == handles.list_action_primary) {
                            invoke_list_action_primary();
                        } else if (target == handles.list_action_secondary) {
                            invoke_list_action_secondary();
                        } else if (target == handles.list_action_tertiary) {
                            invoke_list_action_tertiary();
                        } else if (target == handles.bottom_play) {
                            actions.toggle_play = true;
                        } else if (target == handles.bottom_next) {
                            actions.next = true;
                        } else if (target == handles.bottom_hit || target == handles.bottom_bar
                                   || target == handles.bottom_cover
                                   || target == handles.bottom_title || target == handles.bottom_subtitle) {
                            close_list_action_menu();
                            set_page(PlayerPage::NowPlaying);
                        } else if (target == handles.nav_home) {
                            close_list_action_menu();
                            set_page(PlayerPage::Home);
                        } else if (target == handles.nav_library) {
                            close_list_action_menu();
                            set_page(PlayerPage::Library);
                        } else if (target == handles.list_tab_songs) {
                            set_library_tab(LibraryTab::Songs);
                        } else if (target == handles.list_tab_albums) {
                            set_library_tab(LibraryTab::Albums);
                        } else if (target == handles.list_tab_artist) {
                            set_library_tab(LibraryTab::Artists);
                        } else if (target == handles.list_shuffle) {
                            toggle_library_shuffle();
                        } else if (target == handles.list_path_hit) {
                            reset_library_context();
                        } else if (target == handles.list_sort) {
                            cycle_list_sort();
                        }
                    }
                }
            }

            if (handles.list) {
                const int selected = access.list_view_selected(handles.list);
                if (selected >= 0 && selected != last_list_selected) {
                    last_list_selected = selected;
                    if (!ignore_list_select) {
                        actions.select = true;
                        actions.select_index = selected;
                    }
                }
                const int action_index = access.consume_list_view_tail_action(handles.list);
                if (action_index >= 0) {
                    actions.list_action = true;
                    actions.list_action_index = action_index;
                }
            }

            apply_actions(actions);
            sync_eq_values();
            sync_volume_value();
            sync_dsp_controls();
        }

