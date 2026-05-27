        void set_start_page(PlayerPage page) noexcept {
            start_page = page;
            if (page != PlayerPage::Probe) default_page = page;
        }

        void set_page(PlayerPage page) noexcept {
#if CHARM_PLAYER_LAYERED_TRANSITIONS
            if (now_playing_transition.active
                && page_transition_state != PageTransitionState::Finishing
                && page_transition_state != PageTransitionState::Aborting) {
                abort_now_playing_transition(page);
            }
#endif
            const bool page_changed = current_page != page;
            if (page == PlayerPage::NowPlaying && current_page != PlayerPage::NowPlaying) {
                return_page = current_page;
            } else if (page != PlayerPage::NowPlaying) {
                return_page = page;
            }
            current_page = page;
            if (!access.valid()) return;
            sync_page_root_visibility(page);
            set_handle_visible(handles.debug_text, false);
            sync_page_chrome(page);
            set_now_playing_transition_root_visible(false);
            if (!page_changed || !page_has_show_refresh_hook(page)) {
                refresh_page(page);
            }
        }

        void update_nav_page_indicator_for(PlayerPage page) {
            if (!access.valid()) return;
            auto set_indicator_hidden = [&](WidgetHandle h) {
                if (!h) return;
                access.set_visible(h, false);
            };
            auto set_nav_label = [&](WidgetHandle h, bool active) {
                if (!h) return;
                StylePatch patch{};
                patch.has_font_color = true;
                patch.font_color = active ? kUiOk : rgba{kUiTitle.r, kUiTitle.g, kUiTitle.b, 178};
                access.set_style_override(h, patch);
            };
            auto set_nav_icon = [&](WidgetHandle h,
                                    bool active,
                                    ::ui::scene::ImageId inactive_icon,
                                    ::ui::scene::ImageId active_icon) {
                if (!h) return;
                access.set_button_icon(h, active ? active_icon : inactive_icon);
            };
            set_indicator_hidden(handles.nav_home_indicator);
            set_indicator_hidden(handles.nav_search_indicator);
            set_indicator_hidden(handles.nav_library_indicator);

            const bool home_active = page == PlayerPage::Home;
            const bool library_active = page == PlayerPage::Library;
            set_nav_label(handles.nav_home_label, home_active);
            set_nav_label(handles.nav_search_label, false);
            set_nav_label(handles.nav_library_label, library_active);
            set_nav_icon(handles.nav_home_icon, home_active, icons.home, icons.home_active);
            set_nav_icon(handles.nav_search_icon, false, icons.search, icons.search_active);
            set_nav_icon(handles.nav_library_icon, library_active, icons.folder, icons.folder_active);
        }

        static void on_show_page(::ui::scene::SceneAccess& access,
                                 WidgetHandle root,
                                 void* ctx) noexcept {
            (void)access;
            (void)root;
            auto* hook = static_cast<PageRefreshHookContext*>(ctx);
            if (!hook || !hook->self) return;
            hook->self->refresh_page(hook->page);
        }

        void init_page_layer(::ui::scene::PageLayer& layer,
                             WidgetHandle root,
                             void (*on_show)(::ui::scene::SceneAccess&, WidgetHandle, void*) noexcept = nullptr,
                             void* hook_ctx = nullptr) noexcept {
            if (!root) return;
            layer.set_root(root);
            layer.set_hooks(::ui::scene::PageHooks{
                .on_show = on_show,
                .ctx = hook_ctx,
            });
            layer.set_visible(access, false);
        }

        void init_page_binding(PageBinding binding) noexcept {
            if (!binding.layer) return;
            if (binding.show_ctx) {
                *binding.show_ctx = PageRefreshHookContext{this, binding.page};
                init_page_layer(*binding.layer,
                                binding.root_handle(),
                                &PlayerController::on_show_page,
                                binding.show_ctx);
                return;
            }
            init_page_layer(*binding.layer, binding.root_handle());
        }

        void init_pages() noexcept {
            for (const auto page : kManagedPages) {
                init_page_binding(page_binding_for(page));
            }
            set_page(start_page);
        }

        void dismiss_probe() noexcept {
            set_page(default_page);
        }
