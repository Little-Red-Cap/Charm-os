export module player.app;

import audio.player;
import audio.result;

export namespace player {
    struct AppConfig {
        audio::PlayerConfig player_config{};
    };

    class App {
    public:
        explicit App(AppConfig config)
            : player_(config.player_config) {}

        audio::Result<void> play(const char* path) { return player_.play(path); }
        audio::Result<void> stop() { return player_.stop(); }

        void tick() { player_.tick(); }
        bool is_running() const noexcept { return player_.is_running(); }

    private:
        audio::AudioPlayer player_;
    };
}
