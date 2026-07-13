namespace player {
    class PlayerMd3Runtime {
    public:
        [[nodiscard]] bool ready() const noexcept { return app_.has_value(); }
        [[nodiscard]] bool has_track() const noexcept {
            return app_ && app_->storage_state().has_tracks;
        }

        bool prepare(charm::system::Clock& clock, PlayerMd3AppInit init) {
            shutdown();
            app_.emplace(
                init.app_config, clock, init.storage_binding, init.audio_bindings);
            (void)app_->scan_storage();
            return true;
        }

        template <typename Controller, typename Page>
        bool bind_controller(Controller& controller,
                             const PlayerMd3RuntimeConfig<Page>& config) {
            if (!app_) return false;
            const auto cover_binding = config.resolved_cover_resource_binding();
            if constexpr (requires { controller.cover_resource_binding = cover_binding; }) {
                controller.cover_resource_binding = cover_binding;
            }
            if constexpr (requires {
                controller.bind_track_probe_source(config.probe_source_binding);
            }) {
                controller.bind_track_probe_source(config.probe_source_binding);
            }
            app_->bind_player(controller);
            if constexpr (requires { controller.set_start_page(config.start_page); }) {
                controller.set_start_page(config.start_page);
            }
            apply_storage_view_compat(controller);
            return true;
        }

        template <typename Builder, typename Controller>
        void build_ui(Builder& builder, Controller& controller) {
            if (app_) app_->bind_ui(builder, controller);
        }

        template <typename Controller, typename Page>
        bool start(Controller& controller, const PlayerMd3RuntimeConfig<Page>& config) {
            if (!app_) return false;
            if constexpr (requires { controller.set_page(config.start_page); }) {
                controller.set_page(config.start_page);
            }
            return app_->bootstrap_player(
                controller, config.initial_track_index, config.auto_start);
        }

        template <typename Controller>
        void tick(Controller& controller) {
            if (!app_) return;
            app_->tick();
            controller.tick_player(app_->player());
        }

        template <typename Scene, typename Controller, typename Event>
        void dispatch_raw_input(Scene& scene,
                                Controller& controller,
                                const Event& event) {
            if (app_) app_->dispatch_raw_input(scene, controller, event);
        }

        template <typename Controller>
        void shutdown(Controller& controller) noexcept {
            if (app_) {
                app_->shutdown(controller);
                app_.reset();
            }
        }

        void shutdown() noexcept {
            if (app_) {
                app_->shutdown();
                app_.reset();
            }
        }

    private:
        template <typename Controller>
        void apply_storage_view_compat(Controller& controller) {
            if constexpr (requires {
                controller.apply_storage_view(app_->storage_view(), false);
            }) {
                controller.apply_storage_view(app_->storage_view(), false);
            } else {
                controller.apply_storage_view(app_->storage_view());
            }
        }

        std::optional<App> app_{};
    };
}
