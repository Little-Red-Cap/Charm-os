export module player.hqzy.app_state;

export namespace player::hqzy {
    enum class Page : unsigned char {
        now_playing = 0,
        metrics = 1,
        controls = 2,
        files = 3
    };

    struct FileEntry {
        char name[40]{};
        bool is_dir{false};
    };

    struct TrackInfo {
        const char* title{"Unknown"};
        const char* artist{"-"};
        const char* format{"44.1kHz 2ch"};
    };

    struct AppState {
        Page page{Page::now_playing};
        bool playing{false};
        bool paused{false};
        bool fs_ready{true};
        bool audio_ready{true};
        unsigned char progress{0};
        unsigned char buffer{80};
        unsigned char volume{40};
        unsigned long uptime_ms{0};
        unsigned long frames{0};
        TrackInfo track{};
        char list_dir[16]{};
        FileEntry entries[24]{};
        unsigned char entry_count{0};
        unsigned char entry_selected{0};
        bool list_ready{false};
        bool list_error{false};
    };

    inline void init(AppState& s) noexcept {
        s.page = Page::now_playing;
        s.playing = false;
        s.paused = false;
        s.fs_ready = true;
        s.audio_ready = true;
        s.progress = 0;
        s.buffer = 80;
        s.volume = 40;
        s.uptime_ms = 0;
        s.frames = 0;
        s.track = TrackInfo{};
        s.list_dir[0] = '\0';
        s.entry_count = 0;
        s.entry_selected = 0;
        s.list_ready = false;
        s.list_error = false;
    }
}
