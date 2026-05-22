    void print_host_feature_summary() noexcept {
        std::printf("[player.features] profile=%s host_ui=%d host_storage=%d host_cover_decode=%d host_file_fonts=%d playback_log=%d\n",
                    player::host_features::profile,
                    player::host_features::host_ui ? 1 : 0,
                    player::host_features::host_storage ? 1 : 0,
                    player::host_features::host_cover_decode ? 1 : 0,
                    player::host_features::host_file_fonts ? 1 : 0,
                    player::host_features::playback_log ? 1 : 0);
        std::printf("[player.resources] font_mode=%s font=%s font_px=%d/%d/%d host_vhd=%s\n",
                    player::host_features::host_file_fonts ? "file" : "builtin",
                    player::product_config::default_font_path,
                    player::product_config::default_font_small_px,
                    player::product_config::default_font_normal_px,
                    player::product_config::default_font_large_px,
                    player::product_config::host_default_vhd_path);
    }

    struct PreviewOptions {
        std::string screenshot_path{};
        std::string screenshot_gif_path{};
        bool screenshot_verbose{false};
        int screenshot_wait_frames{0};
        bool screenshot_exit{false};
        player::PlayerPage start_page{player::PlayerPage::Home};
        bool start_page_set{false};
        std::optional<player::LibraryTab> library_tab_override{};
        std::string library_context_override{};
        bool library_open_first_group{false};
        int library_select_index{-1};
        bool library_open_info{false};
        int library_open_info_index{-1};
        bool library_open_action_menu{false};
        int library_open_action_menu_index{-1};
        int track_index_override{0};
        bool ui_ci{false};
        std::string font_ttf_path{};
        std::string font_fallback_ttf_path{};
        bool disable_system_font_fallback{false};
        int font_small_px{0};
        int font_normal_px{0};
        int font_large_px{0};
        int home_scroll_y{-1};
    };

    int parse_int(std::string_view value) {
        int out = 0;
        const char* first = value.data();
        const char* last = value.data() + value.size();
        const auto result = std::from_chars(first, last, out);
        if (result.ec != std::errc{} || result.ptr != last) {
            return 0;
        }
        return out;
    }

    int parse_non_negative_int(std::string_view value) {
        return std::max(0, parse_int(value));
    }

    bool parse_player_page(std::string_view value, player::PlayerPage& out) noexcept {
        if (value == "probe") {
            out = player::PlayerPage::Probe;
            return true;
        }
        if (value == "home") {
            out = player::PlayerPage::Home;
            return true;
        }
        if (value == "now") {
            out = player::PlayerPage::NowPlaying;
            return true;
        }
        if (value == "library") {
            out = player::PlayerPage::Library;
            return true;
        }
        return false;
    }

    bool parse_library_tab(std::string_view value, player::LibraryTab& out) noexcept {
        if (value == "songs") {
            out = player::LibraryTab::Songs;
            return true;
        }
        if (value == "albums") {
            out = player::LibraryTab::Albums;
            return true;
        }
        if (value == "artists") {
            out = player::LibraryTab::Artists;
            return true;
        }
        return false;
    }

    PreviewOptions parse_preview_options(int argc, char** argv) {
        PreviewOptions out{};
        for (int i = 1; i < argc; ++i) {
            const std::string_view arg = argv[i] ? argv[i] : "";
            if (arg.rfind("--screenshot=", 0) == 0) {
                out.screenshot_path.assign(arg.substr(13));
            } else if (arg.rfind("--screenshot-gif=", 0) == 0) {
                out.screenshot_gif_path.assign(arg.substr(17));
            } else if (arg.rfind("--screenshot-page=", 0) == 0) {
                if (parse_player_page(arg.substr(18), out.start_page)) {
                    out.start_page_set = true;
                }
            } else if (arg.rfind("--page=", 0) == 0) {
                if (parse_player_page(arg.substr(7), out.start_page)) {
                    out.start_page_set = true;
                }
            } else if (arg.rfind("--library-tab=", 0) == 0) {
                player::LibraryTab tab{};
                if (parse_library_tab(arg.substr(14), tab)) {
                    out.library_tab_override = tab;
                }
            } else if (arg.rfind("--library-context=", 0) == 0) {
                out.library_context_override.assign(arg.substr(18));
            } else if (arg == "--library-open-first-group") {
                out.library_open_first_group = true;
            } else if (arg.rfind("--library-select-index=", 0) == 0) {
                out.library_select_index = parse_int(arg.substr(23));
            } else if (arg == "--library-open-info") {
                out.library_open_info = true;
            } else if (arg.rfind("--library-open-info-index=", 0) == 0) {
                out.library_open_info = true;
                out.library_open_info_index = parse_int(arg.substr(26));
            } else if (arg == "--library-open-action-menu") {
                out.library_open_action_menu = true;
            } else if (arg.rfind("--library-open-action-menu-index=", 0) == 0) {
                out.library_open_action_menu = true;
                out.library_open_action_menu_index = parse_int(arg.substr(33));
            } else if (arg.rfind("--track-index=", 0) == 0) {
                out.track_index_override = parse_non_negative_int(arg.substr(14));
            } else if (arg == "--screenshot-verbose") {
                out.screenshot_verbose = true;
            } else if (arg == "--screenshot-exit") {
                out.screenshot_exit = true;
            } else if (arg.rfind("--screenshot-frame=", 0) == 0) {
                out.screenshot_wait_frames = parse_non_negative_int(arg.substr(19));
            } else if (arg == "--ui-ci") {
                out.ui_ci = true;
            } else if (arg.rfind("--font-ttf=", 0) == 0) {
                out.font_ttf_path.assign(arg.substr(11));
            } else if (arg.rfind("--font-fallback-ttf=", 0) == 0) {
                out.font_fallback_ttf_path.assign(arg.substr(20));
            } else if (arg == "--font-disable-system-fallback") {
                out.disable_system_font_fallback = true;
            } else if (arg.rfind("--font-small=", 0) == 0) {
                out.font_small_px = parse_non_negative_int(arg.substr(13));
            } else if (arg.rfind("--font-normal=", 0) == 0) {
                out.font_normal_px = parse_non_negative_int(arg.substr(14));
            } else if (arg.rfind("--font-large=", 0) == 0) {
                out.font_large_px = parse_non_negative_int(arg.substr(13));
            } else if (arg.rfind("--home-scroll=", 0) == 0) {
                out.home_scroll_y = parse_non_negative_int(arg.substr(14));
            }
        }
        if (!out.start_page_set && (!out.screenshot_path.empty() || !out.screenshot_gif_path.empty())) {
            out.start_page = player::PlayerPage::NowPlaying;
        }
        return out;
    }

    player::AppConfig make_app_config(const PreviewOptions& options) {
        g_player_cfg.output_mode = audio::OutputMode::fixed_rate;
        g_player_cfg.fixed_rate = 48000;

        player::AppConfig app_cfg{g_player_cfg};
        if constexpr (player::host_features::host_file_fonts) {
            if (!options.font_ttf_path.empty()) {
                app_cfg.ttf_path.assign(options.font_ttf_path);
            } else {
                app_cfg.ttf_path.assign(player::product_config::default_font_path);
            }
            if (!options.font_fallback_ttf_path.empty()) {
                app_cfg.ttf_fallback_path.assign(options.font_fallback_ttf_path);
            }
        }
        if (options.font_small_px > 0) {
            app_cfg.ttf_small_px = options.font_small_px;
        }
        if (options.font_normal_px > 0) {
            app_cfg.ttf_normal_px = options.font_normal_px;
        }
        if (options.font_large_px > 0) {
            app_cfg.ttf_large_px = options.font_large_px;
        }
        return app_cfg;
    }

    void log_library_info_preview(const PreviewOptions& options, bool opened) {
        const char* info_title = g_platform.scene_ref().text(g_ctx.handles.list_info_title);
        const char* info_subtitle = g_platform.scene_ref().text(g_ctx.handles.list_info_subtitle);
        const char* info_meta = g_platform.scene_ref().text(g_ctx.handles.list_info_meta);
        const char* info_path_title = g_platform.scene_ref().text(g_ctx.handles.list_info_path_title);
        const char* info_path = g_platform.scene_ref().text(g_ctx.handles.list_info_path);
        const char* info_path_detail = g_platform.scene_ref().text(g_ctx.handles.list_info_path_detail);
        const char* info_hint = g_platform.scene_ref().text(g_ctx.handles.list_info_hint);
        const Rect scrim_rect = g_platform.scene_ref().world_rect(g_ctx.handles.list_info_scrim);
        const Rect card_rect = g_platform.scene_ref().world_rect(g_ctx.handles.list_info_card);
        std::fprintf(stderr,
                     "[preview] library_open_info flag=1 opened=%d selected=%d request=%d title=%s subtitle=%s meta=%s path_title=%s path=%s detail=%s hint=%s scrim=%d,%d,%d,%d card=%d,%d,%d,%d\n",
                     opened ? 1 : 0,
                     g_ctx.last_list_selected,
                     options.library_open_info_index,
                     info_title ? info_title : "",
                     info_subtitle ? info_subtitle : "",
                     info_meta ? info_meta : "",
                     info_path_title ? info_path_title : "",
                     info_path ? info_path : "",
                     info_path_detail ? info_path_detail : "",
                     info_hint ? info_hint : "",
                     scrim_rect.x, scrim_rect.y, scrim_rect.w, scrim_rect.h,
                     card_rect.x, card_rect.y, card_rect.w, card_rect.h);
    }

    void log_library_action_menu_preview(const PreviewOptions& options, bool opened) {
        const char* menu_title = g_platform.scene_ref().text(g_ctx.handles.list_action_title);
        const Rect card_rect = g_platform.scene_ref().world_rect(g_ctx.handles.list_action_card);
        const char* item0 = g_platform.scene_ref().text(g_ctx.handles.list_action_items[0]);
        const char* item1 = g_platform.scene_ref().text(g_ctx.handles.list_action_items[1]);
        const char* item2 = g_platform.scene_ref().text(g_ctx.handles.list_action_items[2]);
        std::fprintf(stderr,
                     "[preview] library_open_action_menu flag=1 opened=%d selected=%d request=%d title=%s items=[%s|%s|%s] card=%d,%d,%d,%d\n",
                     opened ? 1 : 0,
                     g_ctx.last_list_selected,
                     options.library_open_action_menu_index,
                     menu_title ? menu_title : "",
                     item0 ? item0 : "",
                     item1 ? item1 : "",
                     item2 ? item2 : "",
                     card_rect.x, card_rect.y, card_rect.w, card_rect.h);
    }

    void apply_library_preview_options(const PreviewOptions& options) {
        if (options.library_tab_override.has_value()) {
            g_ctx.set_library_tab(*options.library_tab_override);
        }
        if (!options.library_context_override.empty()) {
            (void)g_ctx.set_library_context_for_preview(options.library_context_override);
        } else if (options.library_open_first_group) {
            (void)g_ctx.open_first_library_group_for_preview();
        }
        if (options.library_select_index >= 0) {
            (void)g_ctx.set_library_selected_index_for_preview(options.library_select_index);
        }
        if (options.library_open_info) {
            const bool opened = g_ctx.open_library_info_popup_for_preview(options.library_open_info_index);
            if (options.screenshot_verbose) {
                log_library_info_preview(options, opened);
            }
        } else if (options.screenshot_verbose) {
            std::fprintf(stderr,
                         "[preview] library_open_info flag=0 selected=%d request=%d\n",
                         g_ctx.last_list_selected,
                         options.library_open_info_index);
        }
        if (options.library_open_action_menu) {
            const bool opened =
                g_ctx.open_library_action_menu_for_preview(options.library_open_action_menu_index);
            if (options.screenshot_verbose) {
                log_library_action_menu_preview(options, opened);
            }
        } else if (options.screenshot_verbose) {
            std::fprintf(stderr,
                         "[preview] library_open_action_menu flag=0 selected=%d request=%d\n",
                         g_ctx.last_list_selected,
                         options.library_open_action_menu_index);
        }
    }
