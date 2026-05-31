#include "charm_app_api.h"
#include "charm_app_received_image.hpp"
#include "charm_app_runtime.hpp"
#include "charm_app_staged_runtime.hpp"
#include "charm_app_store.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <span>
#include <string_view>

namespace {

namespace app_abi = charm::app_abi;

enum class RuntimeDomainKind : std::uint8_t {
    host,
    h747_cm7,
    future_remote,
};

struct RuntimeDomainDescriptor {
    RuntimeDomainKind kind{};
    std::string_view name{};
    bool remote{false};
};

struct HostRuntime {
    std::array<char, 768> console{};
    std::size_t console_used{0};
    std::uint32_t now_calls{0};
    int argc_seen{0};
    int exit_requested{-1};
};

struct DomainLoadCtx {
    const RuntimeDomainDescriptor* domain{nullptr};
    CharmAppMainFn entry{nullptr};
    app_abi::AppRunCode load_code{app_abi::AppRunCode::ok};
    int backend_error{0};
    RuntimeDomainKind last_domain{RuntimeDomainKind::host};
    app_abi::AppImageFormat last_format{app_abi::AppImageFormat::function};
    std::uint32_t load_count{0};
};

struct SpanReaderCtx {
    std::span<const std::byte> bytes{};
};

HostRuntime* g_host = nullptr;

int console_write(const char* text, const std::size_t len) {
    if (g_host == nullptr || text == nullptr) {
        return -1;
    }
    const auto remaining = g_host->console.size() - g_host->console_used;
    const auto copy = len < remaining ? len : remaining;
    if (copy != 0U) {
        std::memcpy(g_host->console.data() + g_host->console_used, text, copy);
        g_host->console_used += copy;
    }
    return static_cast<int>(len);
}

std::uint32_t now_ms() {
    if (g_host != nullptr) {
        ++g_host->now_calls;
    }
    return 2468U;
}

int unsupported_display_describe(CharmAppDisplayMode*) {
    return CHARM_APP_STATUS_UNSUPPORTED;
}

int unsupported_display_present(const void*, std::uint32_t) {
    return CHARM_APP_STATUS_UNSUPPORTED;
}

int unsupported_input_poll(CharmAppInputState*) {
    return CHARM_APP_STATUS_UNSUPPORTED;
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
    if (g_host != nullptr) {
        g_host->exit_requested = code;
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
        .display = CharmAppDisplayApi{
            .describe = unsupported_display_describe,
            .present = unsupported_display_present,
        },
        .input = CharmAppInputApi{.poll = unsupported_input_poll},
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

extern "C" int resident_platform_app_main(const CharmAppApi* api, int argc, char** argv) {
    if (g_host == nullptr || api == nullptr || argv == nullptr || argc < 1) {
        return 90;
    }
    if (api->console.write == nullptr || api->time.now_ms == nullptr) {
        return 91;
    }
    if (api->time.now_ms() != 2468U) {
        return 92;
    }
    constexpr char kMessage[] = "resident image platform app\n";
    if (api->console.write(kMessage, sizeof(kMessage) - 1U) < 0) {
        return 93;
    }
    g_host->argc_seen = argc;
    if (api->app.exit != nullptr) {
        api->app.exit(55);
    }
    return 7;
}

bool span_reader_read(void* ctx, std::uint32_t offset, std::span<std::byte> out) noexcept {
    auto* reader = static_cast<SpanReaderCtx*>(ctx);
    if (reader == nullptr || reader->bytes.data() == nullptr ||
        offset > reader->bytes.size() || out.size() > (reader->bytes.size() - offset)) {
        return false;
    }
    std::memcpy(out.data(), reader->bytes.data() + offset, out.size());
    return true;
}

app_abi::AppStoreReader make_reader(SpanReaderCtx& ctx) {
    return app_abi::AppStoreReader{
        .ctx = &ctx,
        .read = span_reader_read,
    };
}

app_abi::AppLoadResult load_for_domain(void* ctx,
                                       const app_abi::AppImage& image,
                                       const app_abi::AppLoadBuffer&) noexcept {
    auto* load = static_cast<DomainLoadCtx*>(ctx);
    if (load == nullptr || load->domain == nullptr) {
        return {.code = app_abi::AppRunCode::invalid_argument};
    }
    ++load->load_count;
    load->last_domain = load->domain->kind;
    load->last_format = image.format;

    if (load->load_code != app_abi::AppRunCode::ok) {
        return {.code = load->load_code, .backend_error = load->backend_error};
    }
    if (image.format == app_abi::AppImageFormat::modulex) {
        return {.code = app_abi::AppRunCode::not_supported, .backend_error = 222};
    }
    return app_abi::AppLoadResult{
        .code = app_abi::AppRunCode::ok,
        .image = app_abi::LoadedAppImage::from_entry(image.name, image.format, load->entry),
    };
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

bool contains(const HostRuntime& host, const std::string_view needle) {
    return std::string_view{host.console.data(), host.console_used}.find(needle) != std::string_view::npos;
}

app_abi::AppRunResult run_staged(const app_abi::AppImage& image,
                                 DomainLoadCtx& load_ctx,
                                 CharmAppApi& api,
                                 const std::string_view name,
                                 const std::string_view args,
                                 HostRuntime& host) {
    app_abi::StagedAppImageSource source_ctx{
        .image = image,
        .load_ctx = &load_ctx,
        .load = load_for_domain,
    };
    auto source = app_abi::make_staged_app_image_source(source_ctx);
    app_abi::AppRuntime<> runtime{};
    g_host = &host;
    return runtime.run(app_abi::AppRunConfig{
        .source = &source,
        .api = &api,
        .name = name,
        .arg_text = args,
    });
}

std::span<const std::byte> build_store(std::array<std::byte, 256>& store,
                                       app_abi::AppStoreBuildCode& code) {
    const std::array<std::byte, 8> payload{
        std::byte{0x43},
        std::byte{0x48},
        std::byte{0x52},
        std::byte{0x4d},
        std::byte{0x01},
        std::byte{0x02},
        std::byte{0x03},
        std::byte{0x04},
    };
    const std::array<app_abi::AppStoreBuildEntry, 1> entries{
        app_abi::AppStoreBuildEntry{.name = "store_app", .payload = payload},
    };
    const auto result = app_abi::app_store_build_image(entries, store);
    code = result.code;
    return std::span<const std::byte>{store.data(), result.bytes_written};
}

bool expect_received_path(const RuntimeDomainDescriptor& domain) {
    bool ok = true;
    const std::array<std::byte, 8> payload{
        std::byte{0x52},
        std::byte{0x58},
        std::byte{0x01},
        std::byte{0x02},
        std::byte{0x03},
        std::byte{0x04},
        std::byte{0x05},
        std::byte{0x06},
    };
    std::array<std::byte, 32> cache{};
    const auto staged = app_abi::app_received_image_stage(app_abi::AppReceivedImageStageConfig{
        .name = "received_app",
        .format = app_abi::AppImageFormat::function,
        .image = payload,
        .verified = true,
        .cache = cache,
    });
    ok = expect(staged.code == app_abi::AppReceivedImageStageCode::ok &&
                    staged.image.name == "received_app" &&
                    staged.image.image_base == cache.data(),
                "received bytes stage into AppImage") && ok;

    HostRuntime host{};
    auto api = make_api();
    DomainLoadCtx load_ctx{.domain = &domain, .entry = resident_platform_app_main};
    const auto run = run_staged(staged.image, load_ctx, api, "received_app",
                                "from_received", host);
    ok = expect_result(run, app_abi::AppRunStage::exit, app_abi::AppRunCode::ok,
                       "received AppImage reaches AppRuntime exit") && ok;
    ok = expect(run.exit_code == 7 && run.exited, "received AppImage returns app exit code") && ok;
    ok = expect(host.argc_seen == 2 && host.exit_requested == 55 &&
                    host.now_calls == 1 && contains(host, "resident image platform app"),
                "received AppImage sees the same CharmAppApi capability table") && ok;
    ok = expect(load_ctx.last_domain == domain.kind && load_ctx.load_count == 1,
                "received path preserves runtime domain metadata outside App ABI") && ok;
    return ok;
}

bool expect_store_path(const RuntimeDomainDescriptor& domain) {
    bool ok = true;
    std::array<std::byte, 256> store{};
    app_abi::AppStoreBuildCode build_code{};
    const auto store_image = build_store(store, build_code);
    ok = expect(build_code == app_abi::AppStoreBuildCode::ok, "store image builds") && ok;

    SpanReaderCtx reader_ctx{.bytes = store_image};
    const auto reader = make_reader(reader_ctx);
    std::array<std::byte, 32> cache{};
    const auto staged = app_abi::app_store_stage_named_image(reader,
                                                             "store_app",
                                                             cache,
                                                             app_abi::AppImageFormat::function);
    ok = expect(staged.code == app_abi::AppStoreReadCode::ok &&
                    staged.image.name == "store_app" &&
                    staged.image.image_base == cache.data(),
                "store reader stages named AppImage") && ok;

    HostRuntime host{};
    auto api = make_api();
    DomainLoadCtx load_ctx{.domain = &domain, .entry = resident_platform_app_main};
    const auto run = run_staged(staged.image, load_ctx, api, "store_app",
                                "from_store", host);
    ok = expect_result(run, app_abi::AppRunStage::exit, app_abi::AppRunCode::ok,
                       "store AppImage reaches AppRuntime exit") && ok;
    ok = expect(run.exit_code == 7 && host.exit_requested == 55,
                "store AppImage uses the same runtime contract as received image") && ok;
    ok = expect(load_ctx.last_domain == domain.kind && load_ctx.load_count == 1,
                "store path preserves runtime domain metadata outside App ABI") && ok;

    const auto missing = app_abi::app_store_stage_named_image(reader,
                                                              "missing",
                                                              cache,
                                                              app_abi::AppImageFormat::function);
    ok = expect(missing.code == app_abi::AppStoreReadCode::image_not_found,
                "store missing image stays at store lookup boundary") && ok;
    return ok;
}

bool expect_domain_metadata_is_abi_neutral() {
    bool ok = true;
    const std::array<RuntimeDomainDescriptor, 3> domains{
        RuntimeDomainDescriptor{.kind = RuntimeDomainKind::host, .name = "host", .remote = false},
        RuntimeDomainDescriptor{.kind = RuntimeDomainKind::h747_cm7, .name = "h747_cm7", .remote = false},
        RuntimeDomainDescriptor{.kind = RuntimeDomainKind::future_remote, .name = "future_remote", .remote = true},
    };

    for (const auto& domain : domains) {
        HostRuntime host{};
        auto api = make_api();
        const auto api_size_before = api.size;
        const app_abi::AppImage image{
            .name = "domain_app",
            .format = app_abi::AppImageFormat::function,
        };
        DomainLoadCtx load_ctx{.domain = &domain, .entry = resident_platform_app_main};
        const auto run = run_staged(image, load_ctx, api, "domain_app", domain.name, host);
        ok = expect_result(run, app_abi::AppRunStage::exit, app_abi::AppRunCode::ok,
                           "domain image runs through unchanged AppRuntime") && ok;
        ok = expect(api.size == api_size_before && api.size == sizeof(CharmAppApi),
                    "runtime domain metadata does not mutate CharmAppApi") && ok;
        ok = expect(load_ctx.last_domain == domain.kind,
                    "loader can distinguish runtime domain metadata") && ok;
    }
    return ok;
}

bool expect_error_paths(const RuntimeDomainDescriptor& domain) {
    bool ok = true;
    auto api = make_api();

    {
        HostRuntime host{};
        const app_abi::AppImage image{
            .name = "present_app",
            .format = app_abi::AppImageFormat::function,
        };
        DomainLoadCtx load_ctx{.domain = &domain, .entry = resident_platform_app_main};
        const auto missing = run_staged(image, load_ctx, api, "missing_app", "", host);
        ok = expect_result(missing, app_abi::AppRunStage::lookup,
                           app_abi::AppRunCode::image_not_found,
                           "staged source name mismatch stays at lookup stage") && ok;
    }

    {
        HostRuntime host{};
        const app_abi::AppImage image{
            .name = "load_fail_app",
            .format = app_abi::AppImageFormat::function,
        };
        DomainLoadCtx load_ctx{
            .domain = &domain,
            .entry = resident_platform_app_main,
            .load_code = app_abi::AppRunCode::load_failed,
            .backend_error = 77,
        };
        const auto failed = run_staged(image, load_ctx, api, "load_fail_app", "", host);
        ok = expect_result(failed, app_abi::AppRunStage::load,
                           app_abi::AppRunCode::load_failed,
                           "loader failure stays at load stage") && ok;
        ok = expect(failed.backend_error == 77,
                    "loader failure preserves backend error") && ok;
    }

    {
        HostRuntime host{};
        const app_abi::AppImage image{
            .name = "modulex_app",
            .format = app_abi::AppImageFormat::modulex,
        };
        DomainLoadCtx load_ctx{.domain = &domain, .entry = resident_platform_app_main};
        const auto unsupported = run_staged(image, load_ctx, api, "modulex_app", "", host);
        ok = expect_result(unsupported, app_abi::AppRunStage::load,
                           app_abi::AppRunCode::not_supported,
                           "unsupported image format stays at load stage") && ok;
        ok = expect(unsupported.backend_error == 222,
                    "unsupported image format preserves backend error") && ok;
    }

    {
        HostRuntime host{};
        const app_abi::AppImage image{
            .name = "missing_entry_app",
            .format = app_abi::AppImageFormat::function,
        };
        DomainLoadCtx load_ctx{.domain = &domain, .entry = nullptr};
        const auto missing_entry = run_staged(image, load_ctx, api, "missing_entry_app", "", host);
        ok = expect_result(missing_entry, app_abi::AppRunStage::abi,
                           app_abi::AppRunCode::abi_missing,
                           "missing loader entry stays at abi stage") && ok;
    }

    return ok;
}

} // namespace

int main() {
    constexpr RuntimeDomainDescriptor kHostDomain{
        .kind = RuntimeDomainKind::host,
        .name = "host",
        .remote = false,
    };

    bool ok = true;
    ok = expect_received_path(kHostDomain) && ok;
    ok = expect_store_path(kHostDomain) && ok;
    ok = expect_domain_metadata_is_abi_neutral() && ok;
    ok = expect_error_paths(kHostDomain) && ok;

    if (!ok) {
        return 1;
    }
    std::puts("[resident-image-platform-smoke] ok");
    return 0;
}
