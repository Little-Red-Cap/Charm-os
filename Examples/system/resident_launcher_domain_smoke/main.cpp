#include "resident_launcher_domain.hpp"

#include <cstdio>
#include <string_view>

namespace {

namespace app_abi = charm::app_abi;
namespace launcher = h747::apps::resident_launcher;

bool expect(bool condition, const char* message) {
    if (!condition) {
        std::printf("[ERR] %s\n", message);
        return false;
    }
    return true;
}

void set_name(app_abi::FirmwareEntry& entry, std::string_view name) {
    (void)app_abi::fat_copy_text(entry.name, name);
    (void)app_abi::fat_copy_text(entry.path, "/CHARM/APPS");
    entry.format = app_abi::AppImageFormat::elf;
    entry.source = app_abi::FirmwareImageSource::emmc_fat;
    entry.size = 1024U;
    entry.first_cluster = 5U;
}

app_abi::FirmwareCatalog make_catalog(std::uint32_t count) {
    app_abi::FirmwareCatalog catalog{};
    catalog.count = count;
    for (std::uint32_t i = 0; i < count; ++i) {
        char name[24]{};
        const int written = std::snprintf(name, sizeof(name), "APP%02u.ELF", i);
        set_name(catalog.entries[i], std::string_view{name, static_cast<std::size_t>(written)});
        catalog.entries[i].size += i;
        catalog.entries[i].first_cluster += i;
    }
    return catalog;
}

bool test_catalog_view() {
    bool ok = true;
    launcher::ResidentLauncherState state{};
    const auto catalog = make_catalog(3U);
    launcher::resident_launcher_apply_catalog(state, app_abi::FirmwareCatalogCode::ok, catalog);
    const auto view = launcher::resident_launcher_view(state);
    ok = expect(view.catalog_code == app_abi::FirmwareCatalogCode::ok, "catalog code is exposed") && ok;
    ok = expect(view.count == 3U && view.row_count == 3U, "visible rows reflect catalog count") && ok;
    ok = expect(view.can_run && !view.launch_locked, "valid catalog can run before launch") && ok;
    ok = expect(view.rows[0].name == "APP00.ELF" && view.rows[0].selected,
                "first row selected by default") && ok;
    ok = expect(state.dirty, "catalog update marks state dirty") && ok;
    launcher::resident_launcher_mark_clean(state);
    ok = expect(!state.dirty, "mark clean clears dirty bit") && ok;
    return ok;
}

bool test_selection_wrap_and_scroll() {
    bool ok = true;
    launcher::ResidentLauncherState state{};
    launcher::resident_launcher_apply_catalog(state, app_abi::FirmwareCatalogCode::ok, make_catalog(12U));

    launcher::resident_launcher_move_selection(state, -1);
    ok = expect(state.selected == 11U, "negative selection wraps to last entry") && ok;
    ok = expect(state.top == 2U, "wrapped selection scrolls visible window") && ok;

    launcher::resident_launcher_move_selection(state, 1);
    ok = expect(state.selected == 0U && state.top == 0U, "positive selection wraps to first entry") && ok;

    launcher::resident_launcher_move_selection(state, 10);
    ok = expect(state.selected == 10U && state.top == 1U, "selection past visible rows updates top") && ok;

    launcher::resident_launcher_move_selection(state, -3);
    ok = expect(state.selected == 7U && state.top == 1U, "selection inside window preserves top") && ok;
    return ok;
}

bool test_confirm_request_is_one_shot() {
    bool ok = true;
    launcher::ResidentLauncherState state{};
    launcher::resident_launcher_apply_catalog(state, app_abi::FirmwareCatalogCode::ok, make_catalog(2U));
    launcher::resident_launcher_move_selection(state, 1);

    const auto request = launcher::resident_launcher_reduce(
        state,
        launcher::ResidentLauncherInput{.select_delta = 0, .confirm = true});
    ok = expect(request.requested && request.index == 1U, "confirm produces selected run request") && ok;
    ok = expect(app_abi::fat_array_view(request.entry.name) == "APP01.ELF",
                "run request carries selected entry") && ok;
    ok = expect(state.launch_locked, "confirm locks launcher until reset") && ok;

    const auto repeated = launcher::resident_launcher_reduce(
        state,
        launcher::ResidentLauncherInput{.select_delta = 1, .confirm = true});
    ok = expect(!repeated.requested, "locked launcher does not repeat run request") && ok;
    ok = expect(state.selected == 0U, "selection can still move for diagnostics after lock") && ok;
    const auto view = launcher::resident_launcher_view(state);
    ok = expect(!view.can_run && view.launch_locked, "view reports locked launch state") && ok;
    return ok;
}

bool test_error_and_run_record_views() {
    bool ok = true;
    launcher::ResidentLauncherState state{};
    launcher::resident_launcher_apply_catalog_error(state, app_abi::FirmwareCatalogCode::directory_missing);
    auto view = launcher::resident_launcher_view(state);
    ok = expect(view.catalog_code == app_abi::FirmwareCatalogCode::directory_missing,
                "error catalog code reaches view") && ok;
    ok = expect(view.row_count == 0U && !view.can_run, "error view has no runnable rows") && ok;

    app_abi::FirmwareEntry entry{};
    set_name(entry, "BROKEN.ELF");
    launcher::resident_launcher_record_stage_failure(
        state,
        entry,
        app_abi::FirmwareCatalogCode::image_too_large);
    view = launcher::resident_launcher_view(state);
    ok = expect(view.has_run_record && view.run.stage_code == app_abi::FirmwareCatalogCode::image_too_large,
                "stage failure is represented as run record") && ok;
    ok = expect(launcher::resident_launcher_record_name(view.run) == "BROKEN.ELF",
                "run record keeps firmware name") && ok;

    launcher::ResidentLauncherRunRecord record{};
    set_name(entry, "HELLO.ELF");
    launcher::resident_launcher_copy_entry_name(record, entry);
    record.format = app_abi::AppImageFormat::elf;
    record.source = app_abi::FirmwareImageSource::emmc_fat;
    record.result.stage = app_abi::AppRunStage::exit;
    record.result.code = app_abi::AppRunCode::ok;
    record.result.exit_code = 0;
    record.result.exited = true;
    record.load_base = 0x24070000U;
    record.entry_address = 0x24070021U;
    record.load_span = 512U;
    record.segment_count = 2U;
    launcher::resident_launcher_record_run(state, record);
    view = launcher::resident_launcher_view(state);
    ok = expect(view.has_run_record && view.run.result.stage == app_abi::AppRunStage::exit &&
                    view.run.result.code == app_abi::AppRunCode::ok,
                "successful AppRuntime result is represented in view") && ok;
    ok = expect(launcher::resident_launcher_format_name(view.run.format) == "elf",
                "format name is stable") && ok;
    return ok;
}

} // namespace

int main() {
    bool ok = true;
    ok = test_catalog_view() && ok;
    ok = test_selection_wrap_and_scroll() && ok;
    ok = test_confirm_request_is_one_shot() && ok;
    ok = test_error_and_run_record_views() && ok;
    if (!ok) {
        return 1;
    }
    std::puts("[resident-launcher-domain-smoke] ok");
    return 0;
}
