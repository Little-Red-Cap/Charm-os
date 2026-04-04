        void set_start_page(PlayerPage page) noexcept {
            start_page = page;
            if (page != PlayerPage::Probe) default_page = page;
        }

        void set_page(PlayerPage page) noexcept {
            if (page == PlayerPage::NowPlaying && current_page != PlayerPage::NowPlaying) {
                return_page = current_page;
            } else if (page != PlayerPage::NowPlaying) {
                return_page = page;
            }
            current_page = page;
            if (!access.valid()) return;
            if (page_probe_layer.root()) {
                if (page == PlayerPage::Probe) page_probe_layer.show(access);
                else page_probe_layer.hide(access);
            } else if (handles.page_probe) {
                access.set_visible(handles.page_probe, page == PlayerPage::Probe);
            }
            if (page_home_layer.root()) {
                if (page == PlayerPage::Home) page_home_layer.show(access);
                else page_home_layer.hide(access);
            } else if (handles.page_home) {
                access.set_visible(handles.page_home, page == PlayerPage::Home);
            }
            if (page_now_layer.root()) {
                if (page == PlayerPage::NowPlaying) page_now_layer.show(access);
                else page_now_layer.hide(access);
            } else if (handles.page_now_playing) {
                access.set_visible(handles.page_now_playing, page == PlayerPage::NowPlaying);
            }
            if (page_library_layer.root()) {
                if (page == PlayerPage::Library) page_library_layer.show(access);
                else page_library_layer.hide(access);
            } else if (handles.page_library) {
                access.set_visible(handles.page_library, page == PlayerPage::Library);
            }
            if (handles.debug_text) {
                access.set_visible(handles.debug_text, false);
            }
            if (handles.bottom_bar) {
                const bool show_bottom = (page == PlayerPage::Home || page == PlayerPage::Library);
                access.set_visible(handles.bottom_bar, show_bottom);
            }
            if (handles.nav_bar) {
                const bool show_nav = (page == PlayerPage::Home || page == PlayerPage::Library);
                access.set_visible(handles.nav_bar, show_nav);
            }
            update_nav_page_indicator();
            if (page == PlayerPage::Library) {
                refresh_library();
            } else if (page == PlayerPage::Home) {
                refresh_home();
            } else if (page == PlayerPage::NowPlaying) {
                refresh_now_playing();
            }
        }

        void update_nav_page_indicator() {
            if (!access.valid()) return;
            auto set_indicator = [&](WidgetHandle h, bool active) {
                if (!h) return;
                StylePatch patch{};
                patch.has_bg_color = true;
                patch.bg_color = active ? kUiOk : rgba{0, 0, 0, 0};
                patch.has_border_color = true;
                patch.border_color = active ? kUiOk : rgba{0, 0, 0, 0};
                access.set_style_override(h, patch);
            };
            set_indicator(handles.nav_home_indicator, current_page == PlayerPage::Home);
            set_indicator(handles.nav_search_indicator, false);
            set_indicator(handles.nav_library_indicator, current_page == PlayerPage::Library);
        }

        static void on_show_home(::ui::scene::SceneAccess& access,
                                 WidgetHandle root,
                                 void* ctx) noexcept {
            (void)access;
            (void)root;
            auto* self = static_cast<PlayerController*>(ctx);
            if (!self) return;
            self->refresh_home();
        }

        static void on_show_now_playing(::ui::scene::SceneAccess& access,
                                        WidgetHandle root,
                                        void* ctx) noexcept {
            (void)access;
            (void)root;
            auto* self = static_cast<PlayerController*>(ctx);
            if (!self) return;
            self->refresh_now_playing();
            self->set_time_label(self->playback.current_sec());
        }

        static void on_show_library(::ui::scene::SceneAccess& access,
                                    WidgetHandle root,
                                    void* ctx) noexcept {
            (void)access;
            (void)root;
            auto* self = static_cast<PlayerController*>(ctx);
            if (!self) return;
            self->refresh_library();
        }

        void init_pages() noexcept {
            if (handles.page_probe) {
                page_probe_layer.set_root(handles.page_probe);
                page_probe_layer.set_visible(access, false);
            }
            if (handles.page_home) {
                page_home_layer.set_root(handles.page_home);
                page_home_layer.set_hooks(::ui::scene::PageHooks{
                    .on_show = &PlayerController::on_show_home,
                    .ctx = this,
                });
                page_home_layer.set_visible(access, false);
            }
            if (handles.page_now_playing) {
                page_now_layer.set_root(handles.page_now_playing);
                page_now_layer.set_hooks(::ui::scene::PageHooks{
                    .on_show = &PlayerController::on_show_now_playing,
                    .ctx = this,
                });
                page_now_layer.set_visible(access, false);
            }
            if (handles.page_library) {
                page_library_layer.set_root(handles.page_library);
                page_library_layer.set_hooks(::ui::scene::PageHooks{
                    .on_show = &PlayerController::on_show_library,
                    .ctx = this,
                });
                page_library_layer.set_visible(access, false);
            }
            if (handles.btn_mode) {
                access.set_visible(handles.btn_mode, false);
            }
            set_page(start_page);
        }

        void dismiss_probe() noexcept {
            set_page(default_page);
        }
