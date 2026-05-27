#include "charm_app_api.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string_view>

extern "C" int hello_app_main(const CharmAppApi* api, int argc, char** argv);
extern "C" int player_min_main(const CharmAppApi* api, int argc, char** argv);

namespace {

struct HostRuntime {
    std::array<char, 512> console{};
    std::size_t console_used{0};
    std::uint32_t present_count{0};
    std::uint32_t input_poll_count{0};
    std::uint32_t last_present_bytes{0};
};

HostRuntime* g_runtime = nullptr;

int console_write(const char* text, const std::size_t len) {
    if (g_runtime == nullptr || text == nullptr) {
        return -1;
    }
    const std::size_t remaining = g_runtime->console.size() - g_runtime->console_used;
    const std::size_t copy = len < remaining ? len : remaining;
    if (copy != 0U) {
        std::memcpy(g_runtime->console.data() + g_runtime->console_used, text, copy);
        g_runtime->console_used += copy;
    }
    return static_cast<int>(len);
}

std::uint32_t now_ms() {
    return 1234U;
}

int display_describe(CharmAppDisplayMode* out_mode) {
    if (out_mode == nullptr) {
        return CHARM_APP_STATUS_INVALID_ARGUMENT;
    }
    *out_mode = CharmAppDisplayMode{
        .width = 720U,
        .height = 1280U,
        .stride_bytes = 720U * 4U,
        .format = CHARM_APP_PIXEL_FORMAT_ARGB8888,
    };
    return CHARM_APP_STATUS_OK;
}

int display_present(const void* pixels, const std::uint32_t bytes) {
    if (g_runtime == nullptr || pixels == nullptr || bytes == 0U) {
        return CHARM_APP_STATUS_INVALID_ARGUMENT;
    }
    ++g_runtime->present_count;
    g_runtime->last_present_bytes = bytes;
    return CHARM_APP_STATUS_OK;
}

int input_poll(CharmAppInputState* out_state) {
    if (g_runtime == nullptr || out_state == nullptr) {
        return CHARM_APP_STATUS_INVALID_ARGUMENT;
    }
    ++g_runtime->input_poll_count;
    *out_state = CharmAppInputState{
        .encoder1_delta = 1,
        .encoder2_delta = -1,
        .encoder1_pressed = 0,
        .encoder2_pressed = 1,
        .pointer_detected = 1,
        .pointer_down = 0,
        .pointer_x = 10,
        .pointer_y = 20,
        .pointer_max_x = 720,
        .pointer_max_y = 1280,
    };
    return CHARM_APP_STATUS_OK;
}

int storage_open(const char*, int, int) {
    return -1;
}

int storage_read(int, void*, std::size_t) {
    return -1;
}

int storage_write(int, const void*, std::size_t) {
    return -1;
}

int storage_close(int) {
    return -1;
}

int afe_configure(std::uint32_t, std::uint32_t) {
    return CHARM_APP_STATUS_UNSUPPORTED;
}

int afe_read(void*, std::size_t) {
    return CHARM_APP_STATUS_UNSUPPORTED;
}

void app_exit(int) {
}

CharmAppApi make_api() {
    return CharmAppApi{
        .magic = CHARM_APP_API_MAGIC,
        .version = CHARM_APP_API_VERSION,
        .size = sizeof(CharmAppApi),
        .flags = 0,
        .console = CharmAppConsoleApi{.write = console_write},
        .time = CharmAppTimeApi{.now_ms = now_ms},
        .display = CharmAppDisplayApi{.describe = display_describe, .present = display_present},
        .input = CharmAppInputApi{.poll = input_poll},
        .storage = CharmAppStorageApi{
            .open = storage_open,
            .read = storage_read,
            .write = storage_write,
            .close = storage_close,
        },
        .afe = CharmAppAfeApi{.configure = afe_configure, .read = afe_read},
        .app = CharmAppControlApi{.exit = app_exit},
    };
}

bool contains(const HostRuntime& runtime, const std::string_view needle) {
    const std::string_view haystack{runtime.console.data(), runtime.console_used};
    return haystack.find(needle) != std::string_view::npos;
}

bool expect(const bool condition, const char* message) {
    if (!condition) {
        std::printf("[ERR] %s\n", message);
        return false;
    }
    return true;
}

} // namespace

int main() {
    HostRuntime runtime{};
    g_runtime = &runtime;
    CharmAppApi api = make_api();

    char hello0[] = "hello_app";
    char hello1[] = "demo";
    char* hello_argv[] = {hello0, hello1, nullptr};
    int hello_rc = hello_app_main(&api, 2, hello_argv);

    bool ok = true;
    ok = expect(hello_rc == 0, "hello_app returns 0") && ok;
    ok = expect(contains(runtime, "hello_app: charm_app_main entered"),
                "hello_app writes through console capability") && ok;
    ok = expect(contains(runtime, "hello_app: argv1=demo"),
                "hello_app receives argc/argv") && ok;

    char player0[] = "player_min";
    char* player_argv[] = {player0, nullptr};
    int player_rc = player_min_main(&api, 1, player_argv);
    ok = expect(player_rc == 0, "player_min returns 0") && ok;
    ok = expect(runtime.present_count == 1, "player_min presents one raster payload") && ok;
    ok = expect(runtime.last_present_bytes == 16U * 16U * 4U,
                "player_min uses bounded app-local raster payload") && ok;
    ok = expect(runtime.input_poll_count == 1, "player_min polls input once") && ok;
    ok = expect(contains(runtime, "player_min: presented one frame"),
                "player_min reports success through console") && ok;

    if (!ok) {
        return 1;
    }
    std::puts("[app-abi-host-smoke] ok");
    return 0;
}
