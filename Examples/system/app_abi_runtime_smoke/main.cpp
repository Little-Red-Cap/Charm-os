#include "charm_app_api.h"
#include "charm_app_runtime.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string_view>

extern "C" int hello_app_main(const CharmAppApi* api, int argc, char** argv);
extern "C" int player_min_main(const CharmAppApi* api, int argc, char** argv);

namespace {

namespace app_abi = charm::app_abi;

struct HostRuntime {
    std::array<char, 768> console{};
    std::size_t console_used{0};
    std::uint32_t present_count{0};
    std::uint32_t input_poll_count{0};
    std::uint32_t last_present_bytes{0};
    int requested_exit{0};
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
    return 4321U;
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

void app_exit(int code) {
    if (g_runtime != nullptr) {
        g_runtime->requested_exit = code;
    }
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

struct SourceEntry {
    app_abi::AppImage image{};
    CharmAppMainFn entry{nullptr};
    app_abi::AppRunCode load_code{app_abi::AppRunCode::ok};
    int backend_error{0};
};

struct SourceCtx {
    std::array<SourceEntry, 4> entries{};
};

const app_abi::AppImage* find_image(void* ctx, std::string_view name) noexcept {
    auto* source = static_cast<SourceCtx*>(ctx);
    if (source == nullptr) {
        return nullptr;
    }
    for (const auto& entry : source->entries) {
        if (entry.image.name == name) {
            return &entry.image;
        }
    }
    return nullptr;
}

app_abi::AppLoadResult load_image(void* ctx,
                                  const app_abi::AppImage& image,
                                  const app_abi::AppLoadBuffer&) noexcept {
    auto* source = static_cast<SourceCtx*>(ctx);
    if (source == nullptr) {
        return app_abi::AppLoadResult{.code = app_abi::AppRunCode::invalid_argument};
    }
    for (const auto& entry : source->entries) {
        if (&entry.image != &image) {
            continue;
        }
        if (entry.load_code != app_abi::AppRunCode::ok) {
            return app_abi::AppLoadResult{
                .code = entry.load_code,
                .backend_error = entry.backend_error,
            };
        }
        return app_abi::AppLoadResult{
            .code = app_abi::AppRunCode::ok,
            .image = app_abi::LoadedAppImage::from_entry(image.name, image.format, entry.entry),
        };
    }
    return app_abi::AppLoadResult{.code = app_abi::AppRunCode::image_not_found};
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

bool expect_result(const app_abi::AppRunResult& result,
                   app_abi::AppRunStage stage,
                   app_abi::AppRunCode code,
                   const char* message) {
    return expect(result.stage == stage, message) &&
           expect(result.code == code, message);
}

} // namespace

int main() {
    HostRuntime runtime{};
    g_runtime = &runtime;

    SourceCtx source_ctx{
        .entries = {
            SourceEntry{
                .image = app_abi::AppImage{.name = "hello_app", .format = app_abi::AppImageFormat::function},
                .entry = hello_app_main,
            },
            SourceEntry{
                .image = app_abi::AppImage{.name = "player_min", .format = app_abi::AppImageFormat::function},
                .entry = player_min_main,
            },
            SourceEntry{
                .image = app_abi::AppImage{.name = "broken_load", .format = app_abi::AppImageFormat::elf},
                .entry = hello_app_main,
                .load_code = app_abi::AppRunCode::load_failed,
                .backend_error = 77,
            },
            SourceEntry{
                .image = app_abi::AppImage{.name = "missing_abi", .format = app_abi::AppImageFormat::function},
                .entry = nullptr,
            },
        },
    };

    app_abi::AppImageSource source{
        .ctx = &source_ctx,
        .find = find_image,
        .load = load_image,
    };
    CharmAppApi api = make_api();
    app_abi::AppRuntime<> app_runtime{};

    bool ok = true;
    const auto prepared = app_runtime.prepare(app_abi::AppRunConfig{
        .source = &source,
        .api = &api,
        .name = "hello_app",
        .arg_text = "demo",
    });
    ok = expect(prepared.ready, "prepare reports ready") && ok;
    ok = expect_result(prepared.result, app_abi::AppRunStage::start, app_abi::AppRunCode::ok,
                       "prepare stops before app entry") && ok;
    ok = expect(!prepared.result.exited && prepared.image.entry == hello_app_main,
                "prepare materializes entry without exit") && ok;
    ok = expect(prepared.api == &api && prepared.argc == 2 &&
                    std::string_view{prepared.argv[0]} == "hello_app" &&
                    std::string_view{prepared.argv[1]} == "demo" &&
                    prepared.argv[2] == nullptr,
                "prepare builds argv without calling entry") && ok;
    ok = expect(runtime.console_used == 0U && runtime.present_count == 0U,
                "prepare does not execute app code") && ok;

    const auto broken_prepare = app_runtime.prepare(app_abi::AppRunConfig{
        .source = &source,
        .api = &api,
        .name = "broken_load",
    });
    ok = expect(!broken_prepare.ready, "prepare reports load failure as not ready") && ok;
    ok = expect_result(broken_prepare.result, app_abi::AppRunStage::load,
                       app_abi::AppRunCode::load_failed,
                       "prepare load failure keeps load stage") && ok;
    ok = expect(broken_prepare.result.backend_error == 77,
                "prepare preserves backend error") && ok;

    const auto hello = app_runtime.run(app_abi::AppRunConfig{
        .source = &source,
        .api = &api,
        .name = "hello_app",
        .arg_text = "demo",
    });
    ok = expect(hello.exited && hello.exit_code == 0, "hello_app exits successfully") && ok;
    ok = expect_result(hello, app_abi::AppRunStage::exit, app_abi::AppRunCode::ok,
                       "hello_app reaches exit stage") && ok;
    ok = expect(contains(runtime, "hello_app: argv1=demo"),
                "runtime builds argc/argv") && ok;

    const auto player = app_runtime.run(app_abi::AppRunConfig{
        .source = &source,
        .api = &api,
        .name = "player_min",
    });
    ok = expect(player.exited && player.exit_code == 0, "player_min exits successfully") && ok;
    ok = expect(runtime.present_count == 1, "player_min presents one frame") && ok;
    ok = expect(runtime.input_poll_count == 1, "player_min polls input") && ok;
    ok = expect(runtime.last_present_bytes == 16U * 16U * 4U,
                "player_min uses bounded raster payload") && ok;

    const auto missing = app_runtime.run(app_abi::AppRunConfig{
        .source = &source,
        .api = &api,
        .name = "no_such_app",
    });
    ok = expect_result(missing, app_abi::AppRunStage::lookup,
                       app_abi::AppRunCode::image_not_found,
                       "missing image fails at lookup") && ok;

    const auto missing_source = app_runtime.run(app_abi::AppRunConfig{
        .source = nullptr,
        .api = &api,
        .name = "hello_app",
    });
    ok = expect_result(missing_source, app_abi::AppRunStage::lookup,
                       app_abi::AppRunCode::invalid_argument,
                       "missing source fails at lookup") && ok;

    app_abi::AppImageSource missing_find_source = source;
    missing_find_source.find = nullptr;
    const auto missing_find = app_runtime.run(app_abi::AppRunConfig{
        .source = &missing_find_source,
        .api = &api,
        .name = "hello_app",
    });
    ok = expect_result(missing_find, app_abi::AppRunStage::lookup,
                       app_abi::AppRunCode::invalid_argument,
                       "missing find callback fails at lookup") && ok;

    app_abi::AppImageSource missing_load_source = source;
    missing_load_source.load = nullptr;
    const auto missing_load = app_runtime.run(app_abi::AppRunConfig{
        .source = &missing_load_source,
        .api = &api,
        .name = "hello_app",
    });
    ok = expect_result(missing_load, app_abi::AppRunStage::lookup,
                       app_abi::AppRunCode::invalid_argument,
                       "missing load callback fails at lookup") && ok;

    const auto broken = app_runtime.run(app_abi::AppRunConfig{
        .source = &source,
        .api = &api,
        .name = "broken_load",
    });
    ok = expect_result(broken, app_abi::AppRunStage::load,
                       app_abi::AppRunCode::load_failed,
                       "load failure is reported at load stage") && ok;
    ok = expect(broken.backend_error == 77, "backend error is preserved") && ok;

    const auto missing_abi = app_runtime.run(app_abi::AppRunConfig{
        .source = &source,
        .api = &api,
        .name = "missing_abi",
    });
    ok = expect_result(missing_abi, app_abi::AppRunStage::abi,
                       app_abi::AppRunCode::abi_missing,
                       "missing entry fails at abi stage") && ok;

    CharmAppApi bad_api = api;
    bad_api.magic = 0;
    const auto bad_api_result = app_runtime.run(app_abi::AppRunConfig{
        .source = &source,
        .api = &bad_api,
        .name = "hello_app",
    });
    ok = expect_result(bad_api_result, app_abi::AppRunStage::abi,
                       app_abi::AppRunCode::abi_mismatch,
                       "bad API fails at abi stage") && ok;

    CharmAppApi bad_version = api;
    bad_version.version = 0;
    const auto bad_version_result = app_runtime.run(app_abi::AppRunConfig{
        .source = &source,
        .api = &bad_version,
        .name = "hello_app",
    });
    ok = expect_result(bad_version_result, app_abi::AppRunStage::abi,
                       app_abi::AppRunCode::abi_mismatch,
                       "bad API version fails at abi stage") && ok;

    CharmAppApi bad_size = api;
    bad_size.size = sizeof(CharmAppApi) - 1U;
    const auto bad_size_result = app_runtime.run(app_abi::AppRunConfig{
        .source = &source,
        .api = &bad_size,
        .name = "hello_app",
    });
    ok = expect_result(bad_size_result, app_abi::AppRunStage::abi,
                       app_abi::AppRunCode::abi_mismatch,
                       "bad API size fails at abi stage") && ok;

    const auto argv_overflow = app_runtime.run(app_abi::AppRunConfig{
        .source = &source,
        .api = &api,
        .name = "hello_app",
        .arg_text = "a b c d e f g h i j k l m n o p q",
    });
    ok = expect_result(argv_overflow, app_abi::AppRunStage::argv,
                       app_abi::AppRunCode::argv_overflow,
                       "too many args fail at argv stage") && ok;

    const auto arg_blob_overflow = app_runtime.run(app_abi::AppRunConfig{
        .source = &source,
        .api = &api,
        .name = "hello_app",
        .arg_text = "abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz"
                    "abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz"
                    "abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz",
    });
    ok = expect_result(arg_blob_overflow, app_abi::AppRunStage::argv,
                       app_abi::AppRunCode::argv_overflow,
                       "too large arg blob fails at argv stage") && ok;

    SourceCtx long_name_source_ctx{
        .entries = {
            SourceEntry{
                .image = app_abi::AppImage{
                    .name = "this_app_name_is_intentionally_longer_than_the_runtime_name_buffer",
                    .format = app_abi::AppImageFormat::function,
                },
                .entry = hello_app_main,
            },
        },
    };
    app_abi::AppImageSource long_name_source{
        .ctx = &long_name_source_ctx,
        .find = find_image,
        .load = load_image,
    };
    const auto name_too_long = app_runtime.run(app_abi::AppRunConfig{
        .source = &long_name_source,
        .api = &api,
        .name = "this_app_name_is_intentionally_longer_than_the_runtime_name_buffer",
    });
    ok = expect_result(name_too_long, app_abi::AppRunStage::argv,
                       app_abi::AppRunCode::argv_name_too_long,
                       "too long image name fails at argv stage") && ok;

    if (!ok) {
        return 1;
    }
    std::puts("[app-abi-runtime-smoke] ok");
    return 0;
}
