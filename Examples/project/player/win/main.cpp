import player.app;

#include <SDL3/SDL.h>

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <thread>

static std::uint32_t parse_u32(const char* value, std::uint32_t fallback) {
    if (!value || !*value) return fallback;
    char* end = nullptr;
    const auto parsed = std::strtoul(value, &end, 10);
    if (!end || *end != '\0') return fallback;
    return static_cast<std::uint32_t>(parsed);
}

static void print_usage() {
    std::printf("usage: charm-player-win [file.wav|file.flac|file.mp3] [seconds]\n");
}

int main(int argc, char** argv) {
    const char* path = nullptr;
    std::uint32_t duration_sec = 0;
    if (argc >= 2) {
        path = argv[1];
    }
    if (argc >= 3) {
        duration_sec = parse_u32(argv[2], 0);
    }

    player::AppConfig config{};
    player::App app(config);

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::printf("[player] SDL_Init(video) failed\n");
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow("Charm Player", 480, 960, 0);
    if (!window) {
        std::printf("[player] SDL_CreateWindow failed\n");
        SDL_Quit();
        return 1;
    }

    if (path && *path != '\0') {
        auto res = app.play(path);
        if (!res) {
            std::printf("[player] play failed\n");
            SDL_DestroyWindow(window);
            SDL_Quit();
            return 1;
        }
    } else {
        print_usage();
    }

    const auto start = std::chrono::steady_clock::now();
    bool running = true;
    while (running) {
        SDL_Event evt{};
        while (SDL_PollEvent(&evt)) {
            if (evt.type == SDL_EVENT_QUIT) {
                running = false;
            }
        }

        if (app.is_running()) {
            app.tick();
        }

        if (duration_sec > 0) {
            const auto now = std::chrono::steady_clock::now();
            const auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - start).count();
            if (elapsed >= duration_sec) {
                (void)app.stop();
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
