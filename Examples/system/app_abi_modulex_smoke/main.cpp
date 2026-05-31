#include "charm_app_api.h"
#include "charm_app_runtime.hpp"
#include "charm_app_staged_runtime.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <span>
#include <string_view>

import module_core;
import module_link;
import module_loader;
import module_view;

#include "charm_app_modulex_loader.hpp"


namespace {

namespace app_abi = charm::app_abi;

struct HostRuntime {
    std::array<char, 512> console{};
    std::size_t console_used{0};
    std::uint32_t now_count{0};
    int requested_exit{-1};
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
    if (g_runtime != nullptr) {
        ++g_runtime->now_count;
    }
    return 9876U;
}

void app_exit(const int code) {
    if (g_runtime != nullptr) {
        g_runtime->requested_exit = code;
    }
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

int unsupported_storage_open(const char*, int, int) {
    return -1;
}

int unsupported_storage_read(int, void*, std::size_t) {
    return -1;
}

int unsupported_storage_write(int, const void*, std::size_t) {
    return -1;
}

int unsupported_storage_close(int) {
    return -1;
}

int unsupported_afe_configure(std::uint32_t, std::uint32_t) {
    return CHARM_APP_STATUS_UNSUPPORTED;
}

int unsupported_afe_read(void*, std::size_t) {
    return CHARM_APP_STATUS_UNSUPPORTED;
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
            .open = unsupported_storage_open,
            .read = unsupported_storage_read,
            .write = unsupported_storage_write,
            .close = unsupported_storage_close,
        },
        .afe = CharmAppAfeApi{
            .configure = unsupported_afe_configure,
            .read = unsupported_afe_read,
        },
        .app = CharmAppControlApi{.exit = app_exit},
    };
}

extern "C" int fake_charm_app_main(const CharmAppApi* api, int argc, char** argv) {
    if (api == nullptr || api->console.write == nullptr || api->time.now_ms == nullptr ||
        api->app.exit == nullptr) {
        return 100;
    }
    if (argc != 2 || argv == nullptr || argv[0] == nullptr || argv[1] == nullptr) {
        return 101;
    }
    if (std::string_view{argv[0]} != "modulex_app" ||
        std::string_view{argv[1]} != "demo") {
        return 102;
    }
    if (api->time.now_ms() != 9876U) {
        return 103;
    }
    static constexpr char text[] = "modulex-app-main\n";
    if (api->console.write(text, sizeof(text) - 1U) != static_cast<int>(sizeof(text) - 1U)) {
        return 104;
    }
    api->app.exit(17);
    return 17;
}

bool resolve_entry(std::string_view name, modulex::Addr& out_addr) noexcept {
    if (name == "charm_app_main") {
        out_addr = modulex::to_addr(reinterpret_cast<const void*>(&fake_charm_app_main));
        return true;
    }
    return false;
}

bool fail_resolve_entry(std::string_view, modulex::Addr&) noexcept {
    return false;
}

bool resolve_dependency(std::string_view name, std::string_view version) noexcept {
    return name == "host" && version == "v1";
}

bool fail_resolve_dependency(std::string_view, std::string_view) noexcept {
    return false;
}

struct ModuleXFixture {
    modulex::ImageHeader hdr{};
    std::array<std::byte, 4> text{};
    std::array<modulex::Symbol, 1> syms{};
    std::array<char, 24> strtab{};
    std::array<modulex::Dependency, 1> deps{};
};

struct RelocFixture {
    modulex::ImageHeader hdr{};
    std::array<std::byte, sizeof(modulex::Addr)> text{};
    std::array<modulex::Reloc, 1> relocs{};
    std::array<modulex::Symbol, 1> syms{};
    std::array<char, 32> strtab{};
};

constexpr std::uint32_t offset_of_header() noexcept {
    return 0;
}

void init_fixture(ModuleXFixture& image,
                  const bool include_symbol = true,
                  const bool include_dependency = false) {
    image = ModuleXFixture{};
    image.hdr.magic = modulex::k_magic;
    image.hdr.version = modulex::k_version;
    image.hdr.entry_offset = 0;
    image.hdr.text_offset = static_cast<std::uint32_t>(offsetof(ModuleXFixture, text));
    image.hdr.text_size = static_cast<std::uint32_t>(image.text.size());
    image.hdr.sym_offset = static_cast<std::uint32_t>(offsetof(ModuleXFixture, syms));
    image.hdr.sym_size = include_symbol ? static_cast<std::uint32_t>(sizeof(image.syms)) : 0U;
    image.hdr.str_offset = static_cast<std::uint32_t>(offsetof(ModuleXFixture, strtab));
    image.hdr.str_size = static_cast<std::uint32_t>(image.strtab.size());
    image.hdr.dep_offset = static_cast<std::uint32_t>(offsetof(ModuleXFixture, deps));
    image.hdr.dep_size = include_dependency ? static_cast<std::uint32_t>(sizeof(image.deps)) : 0U;
    image.hdr.image_size = static_cast<std::uint32_t>(sizeof(ModuleXFixture));

    if (include_symbol) {
        image.syms[0].name_offset = 0;
        image.syms[0].value = 0;
        image.syms[0].size = 0;
        image.syms[0].kind = modulex::SymbolKind::external;
        image.syms[0].flags = 0;
    }

    constexpr char strings[] = "charm_app_main\0host\0v1\0";
    std::memcpy(image.strtab.data(), strings, sizeof(strings));
    if (include_dependency) {
        image.deps[0].name_offset = 15;
        image.deps[0].version_offset = 20;
    }

    (void)offset_of_header();
}

void init_reloc_fixture(RelocFixture& image) {
    image = RelocFixture{};
    image.hdr.magic = modulex::k_magic;
    image.hdr.version = modulex::k_version;
    image.hdr.entry_offset = 0;
    image.hdr.text_offset = static_cast<std::uint32_t>(offsetof(RelocFixture, text));
    image.hdr.text_size = static_cast<std::uint32_t>(image.text.size());
    image.hdr.rel_offset = static_cast<std::uint32_t>(offsetof(RelocFixture, relocs));
    image.hdr.rel_size = static_cast<std::uint32_t>(sizeof(image.relocs));
    image.hdr.sym_offset = static_cast<std::uint32_t>(offsetof(RelocFixture, syms));
    image.hdr.sym_size = static_cast<std::uint32_t>(sizeof(image.syms));
    image.hdr.str_offset = static_cast<std::uint32_t>(offsetof(RelocFixture, strtab));
    image.hdr.str_size = static_cast<std::uint32_t>(image.strtab.size());
    image.hdr.dep_offset = static_cast<std::uint32_t>(sizeof(RelocFixture));
    image.hdr.dep_size = 0;
    image.hdr.image_size = static_cast<std::uint32_t>(sizeof(RelocFixture));

    image.relocs[0].offset = image.hdr.text_offset;
    image.relocs[0].type = modulex::RelocType::abs_addr;
    image.relocs[0].sym_index = 0;
    image.relocs[0].addend = 0;

    image.syms[0].name_offset = 0;
    image.syms[0].value = 0;
    image.syms[0].size = static_cast<std::uint32_t>(image.text.size());
    image.syms[0].kind = modulex::SymbolKind::global;
    image.syms[0].flags = 0;
    constexpr char strings[] = "charm_app_main\0";
    std::memcpy(image.strtab.data(), strings, sizeof(strings));
}

app_abi::AppImage make_raw_image(void* base,
                                 const std::size_t size,
                                 const app_abi::AppImageFormat format = app_abi::AppImageFormat::modulex,
                                 std::string_view name = "modulex_app") {
    return app_abi::AppImage{
        .name = name,
        .format = format,
        .image_base = base,
        .image_size = size,
    };
}

app_abi::AppImage make_image(ModuleXFixture& fixture,
                             const app_abi::AppImageFormat format = app_abi::AppImageFormat::modulex,
                             const std::size_t size = sizeof(ModuleXFixture)) {
    return make_raw_image(&fixture.hdr, size, format);
}

struct ModuleXLoadCtx {
    app_abi::AppModuleXLoadConfig config{};
};

app_abi::AppLoadResult load_modulex(void* ctx,
                                    const app_abi::AppImage& image,
                                    const app_abi::AppLoadBuffer&) noexcept {
    const auto* load_ctx = static_cast<const ModuleXLoadCtx*>(ctx);
    const auto loaded = app_abi::app_modulex_load_image(
        image,
        load_ctx != nullptr ? load_ctx->config : app_abi::AppModuleXLoadConfig{});
    return loaded.load;
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
                   const app_abi::AppRunStage stage,
                   const app_abi::AppRunCode code,
                   const int backend_error,
                   const char* message) {
    return expect(result.stage == stage, message) &&
           expect(result.code == code, message) &&
           expect(result.backend_error == backend_error, message);
}

app_abi::AppRunResult run_fixture(ModuleXFixture& fixture,
                                  ModuleXLoadCtx& load_ctx,
                                  CharmAppApi& api,
                                  app_abi::AppImageFormat format = app_abi::AppImageFormat::modulex,
                                  std::size_t image_size = sizeof(ModuleXFixture),
                                  std::string_view name = "modulex_app",
                                  std::string_view args = "demo") {
    auto image = make_image(fixture, format, image_size);
    image.name = name;
    app_abi::StagedAppImageSource staged{
        .image = image,
        .load_ctx = &load_ctx,
        .load = load_modulex,
    };
    auto source = app_abi::make_staged_app_image_source(staged);
    app_abi::AppRuntime<> runtime{};
    return runtime.run(app_abi::AppRunConfig{
        .source = &source,
        .api = &api,
        .name = name,
        .arg_text = args,
    });
}

bool expect_loader_code(ModuleXFixture& fixture,
                        const app_abi::AppModuleXLoadConfig& config,
                        const app_abi::AppModuleXLoadCode code,
                        const char* message,
                        const app_abi::AppImageFormat format = app_abi::AppImageFormat::modulex,
                        const std::size_t image_size = sizeof(ModuleXFixture)) {
    auto image = make_image(fixture, format, image_size);
    const auto loaded = app_abi::app_modulex_load_image(image, config);
    return expect(loaded.code == code, message) &&
           expect(loaded.load.backend_error == static_cast<int>(code), message);
}

bool expect_raw_loader_code(void* base,
                            const std::size_t size,
                            const app_abi::AppModuleXLoadConfig& config,
                            const app_abi::AppModuleXLoadCode code,
                            const char* message,
                            const modulex::ImageError validate_error = modulex::ImageError::ok) {
    auto image = make_raw_image(base, size);
    const auto loaded = app_abi::app_modulex_load_image(image, config);
    return expect(loaded.code == code, message) &&
           expect(loaded.load.backend_error == static_cast<int>(code), message) &&
           expect(loaded.validate_error == validate_error, message);
}

bool test_success_path() {
    ModuleXFixture fixture{};
    init_fixture(fixture);
    ModuleXLoadCtx load_ctx{
        .config = app_abi::AppModuleXLoadConfig{
            .resolve_external = resolve_entry,
        },
    };
    HostRuntime host{};
    g_runtime = &host;
    CharmAppApi api = make_api();

    const auto result = run_fixture(fixture, load_ctx, api);
    bool ok = true;
    ok = expect(result.exited && result.exit_code == 17, "ModuleX App exits through AppRuntime") && ok;
    ok = expect_result(result,
                       app_abi::AppRunStage::exit,
                       app_abi::AppRunCode::ok,
                       0,
                       "ModuleX App reaches exit stage") && ok;
    ok = expect(contains(host, "modulex-app-main"),
                "ModuleX App receives CharmAppApi console capability") && ok;
    ok = expect(host.now_count == 1U, "ModuleX App receives time capability") && ok;
    ok = expect(host.requested_exit == 17, "ModuleX App receives app.exit capability") && ok;

    const auto loaded = app_abi::app_modulex_load_image(
        make_image(fixture),
        load_ctx.config);
    ok = expect(loaded.code == app_abi::AppModuleXLoadCode::ok,
                "ModuleX loader reports ok on success") && ok;
    ok = expect(loaded.validate_error == modulex::ImageError::ok &&
                    loaded.dependency_error == modulex::DepError::ok &&
                    loaded.entry_offset == 0xFFFFFFFFU &&
                    loaded.image_span == sizeof(ModuleXFixture) &&
                    !loaded.relocated,
                "ModuleX loader records success diagnostics") && ok;
    return ok;
}

bool test_relocation_paths() {
    bool ok = true;

    RelocFixture reloc{};
    init_reloc_fixture(reloc);
    const auto loaded = app_abi::app_modulex_load_image(make_raw_image(&reloc.hdr, sizeof(reloc)));
    ok = expect(loaded.code == app_abi::AppModuleXLoadCode::ok,
                "abs_addr relocation fixture loads") && ok;
    ok = expect(loaded.relocated, "abs_addr relocation is recorded") && ok;
    ok = expect(loaded.entry_offset == 0U, "local global entry offset is recorded") && ok;
    ok = expect(loaded.image_span == sizeof(RelocFixture), "ModuleX image span is recorded") && ok;
    modulex::Addr patched{};
    std::memcpy(&patched, reloc.text.data(), sizeof(patched));
    const auto expected = modulex::to_addr(&reloc.hdr) + modulex::layout_text(reloc.hdr);
    ok = expect(patched == expected, "abs_addr relocation patches text slot") && ok;

    init_reloc_fixture(reloc);
    reloc.relocs[0].sym_index = 99;
    ok = expect_raw_loader_code(&reloc.hdr,
                                sizeof(reloc),
                                {},
                                app_abi::AppModuleXLoadCode::load_failed,
                                "bad relocation symbol is rejected during validation",
                                modulex::ImageError::bad_sym) && ok;

    init_reloc_fixture(reloc);
    reloc.relocs[0].type = modulex::RelocType::rel32;
    reloc.syms[0].value = static_cast<modulex::Addr>(std::numeric_limits<std::uint32_t>::max()) - 16U;
    const auto rel32 = app_abi::app_modulex_load_image(make_raw_image(&reloc.hdr, sizeof(reloc)));
    ok = expect(rel32.code == app_abi::AppModuleXLoadCode::entry_missing ||
                    rel32.code == app_abi::AppModuleXLoadCode::relocate_failed,
                "rel32 overflow or invalid entry fails before AppRuntime start") && ok;
    return ok;
}

bool test_failure_paths() {
    bool ok = true;
    HostRuntime host{};
    g_runtime = &host;
    CharmAppApi api = make_api();

    const app_abi::AppModuleXLoadConfig good_config{
        .resolve_external = resolve_entry,
    };
    ModuleXLoadCtx good_ctx{.config = good_config};

    ModuleXFixture fixture{};
    init_fixture(fixture);
    ok = expect_loader_code(fixture,
                            good_config,
                            app_abi::AppModuleXLoadCode::format_mismatch,
                            "format mismatch rejected by ModuleX loader",
                            app_abi::AppImageFormat::elf) && ok;
    const auto format_result = run_fixture(fixture, good_ctx, api, app_abi::AppImageFormat::elf);
    ok = expect_result(format_result,
                       app_abi::AppRunStage::load,
                       app_abi::AppRunCode::not_supported,
                       static_cast<int>(app_abi::AppModuleXLoadCode::format_mismatch),
                       "format mismatch propagates through AppRuntime load stage") && ok;

    init_fixture(fixture);
    fixture.hdr.magic = 0;
    ok = expect_loader_code(fixture,
                            good_config,
                            app_abi::AppModuleXLoadCode::load_failed,
                            "bad magic rejected") && ok;

    init_fixture(fixture);
    fixture.hdr.version = static_cast<std::uint16_t>(modulex::k_version + 1U);
    ok = expect_loader_code(fixture,
                            good_config,
                            app_abi::AppModuleXLoadCode::load_failed,
                            "bad version rejected") && ok;

    init_fixture(fixture);
    fixture.hdr.text_size = 0;
    ok = expect_loader_code(fixture,
                            good_config,
                            app_abi::AppModuleXLoadCode::load_failed,
                            "missing text rejected") && ok;

    init_fixture(fixture);
    fixture.hdr.entry_offset = fixture.hdr.text_size;
    ok = expect_loader_code(fixture,
                            good_config,
                            app_abi::AppModuleXLoadCode::load_failed,
                            "bad entry offset rejected") && ok;

    init_fixture(fixture, false);
    ok = expect_loader_code(fixture,
                            good_config,
                            app_abi::AppModuleXLoadCode::entry_missing,
                            "missing charm_app_main symbol rejected") && ok;

    init_fixture(fixture);
    ModuleXLoadCtx fail_external_ctx{
        .config = app_abi::AppModuleXLoadConfig{
            .resolve_external = fail_resolve_entry,
        },
    };
    const auto external_result = run_fixture(fixture, fail_external_ctx, api);
    ok = expect_result(external_result,
                       app_abi::AppRunStage::load,
                       app_abi::AppRunCode::load_failed,
                       static_cast<int>(app_abi::AppModuleXLoadCode::external_failed),
                       "external resolver failure stays at AppRuntime load stage") && ok;

    init_fixture(fixture, true, true);
    ModuleXLoadCtx dep_ctx{
        .config = app_abi::AppModuleXLoadConfig{
            .resolve_external = resolve_entry,
            .resolve_dependency = fail_resolve_dependency,
        },
    };
    const auto dep_result = run_fixture(fixture, dep_ctx, api);
    ok = expect_result(dep_result,
                       app_abi::AppRunStage::load,
                       app_abi::AppRunCode::load_failed,
                       static_cast<int>(app_abi::AppModuleXLoadCode::dependency_failed),
                       "dependency resolver failure stays at AppRuntime load stage") && ok;

    init_fixture(fixture, true, true);
    ModuleXLoadCtx dep_success_ctx{
        .config = app_abi::AppModuleXLoadConfig{
            .resolve_external = resolve_entry,
            .resolve_dependency = resolve_dependency,
        },
    };
    const auto dep_success = run_fixture(fixture, dep_success_ctx, api);
    ok = expect(dep_success.exited && dep_success.exit_code == 17,
                "dependency resolver success still runs ModuleX App") && ok;

    init_fixture(fixture);
    ok = expect_loader_code(fixture,
                            good_config,
                            app_abi::AppModuleXLoadCode::load_failed,
                            "truncated image rejected",
                            app_abi::AppImageFormat::modulex,
                            sizeof(modulex::ImageHeader)) && ok;

    init_fixture(fixture);
    fixture.syms[0].kind = modulex::SymbolKind::global;
    fixture.syms[0].value = fixture.hdr.text_size;
    ok = expect_loader_code(fixture,
                            app_abi::AppModuleXLoadConfig{},
                            app_abi::AppModuleXLoadCode::entry_missing,
                            "global symbol outside text rejected as missing App ABI entry") && ok;

    init_fixture(fixture);
    fixture.hdr.bss_size = 4;
    ok = expect_loader_code(fixture,
                            good_config,
                            app_abi::AppModuleXLoadCode::unsupported_bss,
                            "ModuleX App v1 rejects BSS") && ok;

    init_fixture(fixture);
    fixture.hdr.flags = static_cast<std::uint16_t>(modulex::ImageFlags::xip_text);
    ok = expect_loader_code(fixture,
                            good_config,
                            app_abi::AppModuleXLoadCode::unsupported_xip,
                            "ModuleX App v1 rejects XIP flags") && ok;

    init_fixture(fixture);
    fixture.hdr.sym_size = sizeof(modulex::Symbol) - 1U;
    const auto bad_layout = app_abi::app_modulex_load_image(make_image(fixture), good_config);
    ok = expect(bad_layout.code == app_abi::AppModuleXLoadCode::load_failed &&
                    bad_layout.validate_error == modulex::ImageError::bad_sym,
                "bad symbol layout keeps validate diagnostic") && ok;

    init_fixture(fixture, true, true);
    const app_abi::AppModuleXLoadConfig dep_ctx_config{
        .resolve_external = resolve_entry,
        .resolve_dependency_ctx =
            [](void*, std::string_view, std::string_view) noexcept -> bool { return false; },
    };
    const auto dep_ctx_loaded = app_abi::app_modulex_load_image(make_image(fixture), dep_ctx_config);
    ok = expect(dep_ctx_loaded.code == app_abi::AppModuleXLoadCode::dependency_failed &&
                    dep_ctx_loaded.dependency_error == modulex::DepError::resolve_failed &&
                    dep_ctx_loaded.dependency_index == 0U,
                "dependency ctx failure keeps dependency diagnostic") && ok;

    return ok;
}

} // namespace

int main() {
    bool ok = true;
    ok = test_success_path() && ok;
    ok = test_relocation_paths() && ok;
    ok = test_failure_paths() && ok;

    if (!ok) {
        return 1;
    }
    std::puts("[app-abi-modulex-smoke] ok");
    return 0;
}
