import audio.player;
import charm.core.config;
import charm.core.container;
import charm.core.event;
import charm.core.factory;
import charm.core.gui;
import charm.gfx.canvas;
import charm.gfx.framebuffer;
import charm.gfx.color;
import charm.widgets.button;
import charm.widgets.label;
import charm.widgets.progress;
import fs_core;
import fs_errno;
import fs_stream;
import fs_ramfs;
import fs_vfs;
import util.core;

#include <SDL3/SDL.h>

#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <span>
#include <string>
#include <thread>

namespace {
    constexpr const char* kDefaultHostTrack = "../../assets/beautiful-trick.flac";
    constexpr const char* kDefaultVfsTrack = "/music/beautiful-trick.flac";
    constexpr int kUiPadding = 24;
    constexpr int kCoverSize = 320;

    struct UiHandles {
        WidgetHandle root{};
        WidgetHandle cover{};
        WidgetHandle title{};
        WidgetHandle subtitle{};
        WidgetHandle status{};
        WidgetHandle progress{};
        WidgetHandle time{};
        WidgetHandle btn_play{};
        WidgetHandle btn_stop{};
    };

    struct PlayerUiContext {
        audio::AudioPlayer* player{nullptr};
        UiFactory* factory{nullptr};
        UiHandles handles{};
        bool playing{false};
        bool track_ready{false};
        std::chrono::steady_clock::time_point start{};
        int duration_sec{180};
        const char* track_path{nullptr};
        bool updating{false};

        void set_label(WidgetHandle h, const char* text) {
            if (!factory) return;
            if (auto* label = factory->get_label(h)) {
                label->set_text(text);
            }
        }

        void set_status(const char* text) {
            set_label(handles.status, text);
        }

        void set_time_label(int elapsed_sec) {
            char buf[32]{};
            const int total = duration_sec;
            const int cur_m = elapsed_sec / 60;
            const int cur_s = elapsed_sec % 60;
            const int total_m = total / 60;
            const int total_s = total % 60;
            std::snprintf(buf, sizeof(buf), "%d:%02d / %d:%02d", cur_m, cur_s, total_m, total_s);
            set_label(handles.time, buf);
        }

        void update_progress() {
            if (!playing || updating) return;
            const auto now = std::chrono::steady_clock::now();
            const int elapsed = static_cast<int>(std::chrono::duration_cast<std::chrono::seconds>(now - start).count());
            const int clamped = (elapsed > duration_sec) ? duration_sec : elapsed;
            const int value = (duration_sec > 0)
                ? static_cast<int>((clamped * 100) / duration_sec)
                : 0;

            if (auto* bar = factory->get_progress(handles.progress)) {
                bar->set_value(value);
            }
            set_time_label(clamped);
        }

        void start_playback() {
            if (!player || !track_path || !track_ready) return;
            (void)player->stop();
            auto res = player->play(track_path);
            if (!res) {
                set_status("Play failed");
                return;
            }
            playing = true;
            start = std::chrono::steady_clock::now();
            set_status("Playing");
            set_time_label(0);
            if (auto* bar = factory->get_progress(handles.progress)) {
                bar->set_value(0);
            }
        }

        void stop_playback() {
            if (player) {
                (void)player->stop();
            }
            playing = false;
            set_status("Stopped");
        }
    };

    void on_play_clicked(void* ctx) {
        auto* app = static_cast<PlayerUiContext*>(ctx);
        if (!app) return;
        app->start_playback();
    }

    void on_stop_clicked(void* ctx) {
        auto* app = static_cast<PlayerUiContext*>(ctx);
        if (!app) return;
        app->stop_playback();
    }

    Event::Key map_key(SDL_Keycode key) {
        switch (key) {
        case SDLK_TAB: return Event::Key::Tab;
        case SDLK_RETURN: return Event::Key::Enter;
        case SDLK_SPACE: return Event::Key::Space;
        case SDLK_ESCAPE: return Event::Key::Escape;
        case SDLK_UP: return Event::Key::Up;
        case SDLK_DOWN: return Event::Key::Down;
        case SDLK_LEFT: return Event::Key::Left;
        case SDLK_RIGHT: return Event::Key::Right;
        default: return Event::Key::Unknown;
        }
    }

    bool dispatch_sdl_event(Gui& gui, const SDL_Event& evt) {
        switch (evt.type) {
        case SDL_EVENT_MOUSE_MOTION:
            gui.dispatch_event(Event::mouse(Event::Type::MouseMove, evt.motion.x, evt.motion.y));
            return true;
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
            gui.dispatch_event(Event::mouse(Event::Type::MouseDown, evt.button.x, evt.button.y, evt.button.button));
            return true;
        case SDL_EVENT_MOUSE_BUTTON_UP:
            gui.dispatch_event(Event::mouse(Event::Type::MouseUp, evt.button.x, evt.button.y, evt.button.button));
            return true;
        case SDL_EVENT_MOUSE_WHEEL:
            gui.dispatch_event(Event::wheel(evt.wheel.x, evt.wheel.y, evt.wheel.y));
            return true;
        case SDL_EVENT_KEY_DOWN:
            gui.dispatch_event(Event::key(Event::Type::KeyDown, map_key(evt.key.key)));
            return true;
        case SDL_EVENT_KEY_UP:
            gui.dispatch_event(Event::key(Event::Type::KeyUp, map_key(evt.key.key)));
            return true;
        default:
            return false;
        }
    }

    UiHandles build_ui(UiFactory& factory, PlayerUiContext& ctx) {
        UiHandles h{};
        h.root = factory.create_container();
        auto* root = factory.get_container(h.root);
        root->set_rect({0, 0, screen_width, screen_height});
        root->set_background({18, 20, 28, 255});

        h.cover = factory.create_container();
        if (auto* cover = factory.get_container(h.cover)) {
            cover->set_rect({(screen_width - kCoverSize) / 2, kUiPadding * 2, kCoverSize, kCoverSize});
            cover->set_background({40, 44, 60, 255});
        }

        h.title = factory.create_label("Beautiful Trick");
        if (auto* title = factory.get_label(h.title)) {
            title->set_color({236, 238, 246, 255});
            title->set_pos(kUiPadding, kUiPadding * 2 + kCoverSize + 20);
        }

        h.subtitle = factory.create_label("FELT · FLAC");
        if (auto* sub = factory.get_label(h.subtitle)) {
            sub->set_color({156, 162, 188, 255});
            sub->set_pos(kUiPadding, kUiPadding * 2 + kCoverSize + 46);
        }

        h.progress = factory.create_progress();
        if (auto* bar = factory.get_progress(h.progress)) {
            bar->set_rect({kUiPadding, kUiPadding * 2 + kCoverSize + 90, screen_width - kUiPadding * 2, 16});
            bar->set_range(0, 100);
            bar->set_value(0);
        }

        h.time = factory.create_label("0:00 / 3:00");
        if (auto* time = factory.get_label(h.time)) {
            time->set_color({136, 142, 166, 255});
            time->set_pos(kUiPadding, kUiPadding * 2 + kCoverSize + 120);
        }

        h.status = factory.create_label("Stopped");
        if (auto* status = factory.get_label(h.status)) {
            status->set_color({120, 200, 170, 255});
            status->set_pos(kUiPadding, kUiPadding * 2 + kCoverSize + 150);
        }

        h.btn_play = factory.create_button("Play");
        if (auto* play = factory.get_button(h.btn_play)) {
            play->set_pos(kUiPadding, screen_height - 140);
            play->set_size(140, 48);
            play->set_on_click(Callback{&on_play_clicked, &ctx});
        }

        h.btn_stop = factory.create_button("Stop");
        if (auto* stop = factory.get_button(h.btn_stop)) {
            stop->set_pos(screen_width - kUiPadding - 140, screen_height - 140);
            stop->set_size(140, 48);
            stop->set_on_click(Callback{&on_stop_clicked, &ctx});
        }

        factory.link(h.root, h.cover);
        factory.link(h.root, h.title);
        factory.link(h.root, h.subtitle);
        factory.link(h.root, h.progress);
        factory.link(h.root, h.time);
        factory.link(h.root, h.status);
        factory.link(h.root, h.btn_play);
        factory.link(h.root, h.btn_stop);

        return h;
    }

    bool write_file_to_vfs(std::string_view vfs_path, const char* host_path) {
        if (!host_path || !*host_path) return false;
        std::FILE* fp = std::fopen(host_path, "rb");
        if (!fp) return false;

        fs::File f{};
        auto st = fs::vfs_open(vfs_path, f);
        if (!st) {
            std::fclose(fp);
            return false;
        }

        std::array<util::u8, 4096> buf{};
        while (true) {
            const std::size_t n = std::fread(buf.data(), 1, buf.size(), fp);
            if (n == 0) break;
            auto w = fs::write(f, std::span<const util::u8>(buf.data(), n));
            if (!w) {
                std::fclose(fp);
                return false;
            }
        }
        (void)fs::flush(f);
        std::fclose(fp);
        return true;
    }

    bool setup_vfs_from_host(const char* host_path, std::string_view vfs_path) {
        static fs::RamFs<4096, 64, 4096> ramfs;
        static fs::MountOps ops{
            .open = +[](std::string_view path, fs::File& f) noexcept { return ramfs.open(path, f); },
            .flush = +[](fs::Mount*) noexcept { return fs::Status{fs::Err::ok}; },
            .unmount = +[](fs::Mount*, bool) noexcept { return fs::Status{fs::Err::ok}; },
            .unlink = +[](fs::Mount*, std::string_view path) noexcept { return ramfs.unlink(path); },
            .rename = +[](fs::Mount*, std::string_view from, std::string_view to) noexcept { return ramfs.rename(from, to); },
            .truncate = +[](fs::Mount*, std::string_view path, util::u64 size) noexcept { return ramfs.truncate(path, size); },
            .mkdir = +[](fs::Mount*, std::string_view path) noexcept { return ramfs.mkdir(path); },
            .list = +[](fs::Mount*, std::string_view path, void* ctx, fs::MountOps::ListFn fn) noexcept {
                return ramfs.list(path, ctx, fn);
            }
        };
        static fs::Mount mount{&ops, &ramfs};

        fs::clear_mounts();
        (void)fs::add_mount("/", &mount);

        return write_file_to_vfs(vfs_path, host_path);
    }
}

int main(int argc, char** argv) {
    const char* host_track = (argc >= 2) ? argv[1] : kDefaultHostTrack;

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)) {
        std::printf("[player] SDL_Init failed\n");
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow("Charm Player", screen_width, screen_height, 0);
    if (!window) {
        std::printf("[player] SDL_CreateWindow failed\n");
        SDL_Quit();
        return 1;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(window, nullptr);
    if (!renderer) {
        std::printf("[player] SDL_CreateRenderer failed\n");
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    SDL_Texture* texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGB24, SDL_TEXTUREACCESS_STREAMING,
                                             screen_width, screen_height);
    if (!texture) {
        std::printf("[player] SDL_CreateTexture failed\n");
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    DefaultFrameBuffer fb;
    DefaultCanvas canvas(fb);

    UiFactory factory;
    audio::PlayerConfig cfg{};
    audio::AudioPlayer player(cfg);

    PlayerUiContext ctx{};
    ctx.player = &player;
    ctx.factory = &factory;
    ctx.track_path = kDefaultVfsTrack;

    ctx.handles = build_ui(factory, ctx);
    ctx.set_time_label(0);

    if (setup_vfs_from_host(host_track, kDefaultVfsTrack)) {
        ctx.track_ready = true;
        ctx.set_status("Ready");
    } else {
        ctx.track_ready = false;
        ctx.set_status("Load failed");
    }

    Gui gui(canvas, factory, ctx.handles.root);

    bool running = true;
    while (running) {
        SDL_Event evt{};
        while (SDL_PollEvent(&evt)) {
            if (evt.type == SDL_EVENT_QUIT) {
                running = false;
                break;
            }
            dispatch_sdl_event(gui, evt);
        }

        player.tick();
        ctx.update_progress();

        canvas.clear({18, 20, 28, 255});
        gui.render();

        SDL_UpdateTexture(texture, nullptr, fb.data(), screen_width * 3);
        SDL_RenderClear(renderer);
        SDL_RenderTexture(renderer, texture, nullptr, nullptr);
        SDL_RenderPresent(renderer);

        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }

    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
