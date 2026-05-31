module;

#include <utility>

export module player.runtime_shell;

import player.app;
import charm.system.clock;
import charm.ui.scene;
import player.display;
import player.input;
import player.platform;
import player.runtime;

export namespace player {
    struct PlayerRuntimeShellConfig {
        PlayerDisplaySink* display_sink{nullptr};
        PlayerRuntimeHooks hooks{};
        ::ui::scene::Scene::OverlayFn overlay_fn{nullptr};
        void* overlay_ctx{nullptr};
    };

    template <typename Controller, typename Page>
    class PlayerRuntimeShell {
    public:
        explicit PlayerRuntimeShell(PlayerRuntime<Controller, Page>& runtime,
                                    PlayerRuntimeShellConfig config = {}) noexcept
            : runtime_(&runtime),
              config_(std::move(config)) {}

        [[nodiscard]] bool valid() const noexcept { return runtime_ != nullptr; }

        [[nodiscard]] PlayerRuntime<Controller, Page>* runtime() noexcept { return runtime_; }
        [[nodiscard]] const PlayerRuntime<Controller, Page>* runtime() const noexcept { return runtime_; }

        [[nodiscard]] App* app() noexcept { return runtime_ ? runtime_->app() : nullptr; }
        [[nodiscard]] const App* app() const noexcept { return runtime_ ? runtime_->app() : nullptr; }

        [[nodiscard]] PlayerPlatform* platform() noexcept { return runtime_ ? runtime_->platform() : nullptr; }
        [[nodiscard]] const PlayerPlatform* platform() const noexcept {
            return runtime_ ? runtime_->platform() : nullptr;
        }

        [[nodiscard]] Controller* controller() noexcept { return runtime_ ? runtime_->controller() : nullptr; }
        [[nodiscard]] const Controller* controller() const noexcept {
            return runtime_ ? runtime_->controller() : nullptr;
        }

        [[nodiscard]] ::ui::scene::Scene* scene() noexcept {
            return runtime_ ? &runtime_->scene_ref() : nullptr;
        }
        [[nodiscard]] const ::ui::scene::Scene* scene() const noexcept {
            return runtime_ ? &runtime_->scene_ref() : nullptr;
        }

        [[nodiscard]] ::ui::scene::Scene& scene_ref() noexcept {
            return runtime_->scene_ref();
        }

        [[nodiscard]] const ::ui::scene::Scene& scene_ref() const noexcept {
            return runtime_->scene_ref();
        }

        [[nodiscard]] float t_sec() const noexcept {
            return runtime_ ? runtime_->t_sec() : 0.0f;
        }

        [[nodiscard]] const PlayerRuntimeShellConfig& config() const noexcept { return config_; }

        void bind_display_sink(PlayerDisplaySink* display_sink) noexcept {
            config_.display_sink = display_sink;
        }

        void bind_overlay(::ui::scene::Scene::OverlayFn overlay_fn, void* overlay_ctx) noexcept {
            config_.overlay_fn = overlay_fn;
            config_.overlay_ctx = overlay_ctx;
        }

        void set_hooks(PlayerRuntimeHooks hooks) noexcept {
            config_.hooks = hooks;
        }

        [[nodiscard]] bool bootstrap() {
            return runtime_ && runtime_->bootstrap();
        }

        void dispatch_input(const PlayerInputEvent& ev) {
            if (!runtime_) {
                return;
            }
            runtime_->dispatch_input(ev);
        }

        void step(charm::system::ClockTick now_us) {
            if (!runtime_) {
                return;
            }
            runtime_->tick(now_us, config_.hooks);
        }

        [[nodiscard]] bool render() {
            return runtime_ &&
                   runtime_->render(config_.display_sink, config_.overlay_fn, config_.overlay_ctx);
        }

        [[nodiscard]] bool frame(charm::system::ClockTick now_us) {
            step(now_us);
            return render();
        }

        void shutdown() noexcept {
            if (!runtime_) {
                return;
            }
            runtime_->shutdown();
        }

    private:
        PlayerRuntime<Controller, Page>* runtime_{nullptr};
        PlayerRuntimeShellConfig config_{};
    };
}
