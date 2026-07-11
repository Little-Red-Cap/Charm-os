#pragma once

#include "charm_app_fat_catalog.hpp"

#include <array>
#include <cstdint>
#include <cstring>
#include <string_view>

namespace h747::apps::resident_launcher {

inline constexpr std::uint32_t kResidentLauncherVisibleRows = 10U;

struct ResidentLauncherInput {
    int select_delta{0};
    bool confirm{false};
};

struct ResidentLauncherRunRequest {
    bool requested{false};
    std::uint32_t index{0};
    charm::app_abi::FirmwareEntry entry{};
};

struct ResidentLauncherRunRecord {
    bool valid{false};
    std::array<char, charm::app_abi::kFirmwareCatalogMaxName> name{};
    charm::app_abi::AppImageFormat format{charm::app_abi::AppImageFormat::elf};
    charm::app_abi::FirmwareImageSource source{charm::app_abi::FirmwareImageSource::emmc_fat};
    charm::app_abi::FirmwareCatalogCode stage_code{charm::app_abi::FirmwareCatalogCode::ok};
    charm::app_abi::AppRunResult result{};
    std::uintptr_t load_base{0};
    std::uintptr_t entry_address{0};
    std::uint32_t load_span{0};
    std::uint32_t segment_count{0};
    std::uint32_t console_bytes{0};
    std::uint32_t present_count{0};
    std::uint32_t input_count{0};
};

struct ResidentLauncherState {
    charm::app_abi::FirmwareCatalog catalog{};
    charm::app_abi::FirmwareCatalogCode catalog_code{
        charm::app_abi::FirmwareCatalogCode::storage_unavailable};
    std::uint32_t selected{0};
    std::uint32_t top{0};
    bool launch_locked{false};
    bool dirty{true};
    ResidentLauncherRunRecord last_run{};
};

struct ResidentLauncherVisibleRow {
    std::uint32_t index{0};
    std::string_view name{};
    std::uint32_t size{0};
    charm::app_abi::AppImageFormat format{charm::app_abi::AppImageFormat::elf};
    bool selected{false};
};

struct ResidentLauncherViewModel {
    charm::app_abi::FirmwareCatalogCode catalog_code{
        charm::app_abi::FirmwareCatalogCode::storage_unavailable};
    std::uint32_t count{0};
    std::uint32_t selected{0};
    std::uint32_t top{0};
    bool can_run{false};
    bool launch_locked{false};
    bool has_run_record{false};
    ResidentLauncherRunRecord run{};
    std::array<ResidentLauncherVisibleRow, kResidentLauncherVisibleRows> rows{};
    std::uint32_t row_count{0};
};

[[nodiscard]] constexpr std::string_view resident_launcher_format_name(
    charm::app_abi::AppImageFormat format) noexcept {
    using namespace std::literals::string_view_literals;
    switch (format) {
        case charm::app_abi::AppImageFormat::function:
            return "function"sv;
        case charm::app_abi::AppImageFormat::elf:
            return "elf"sv;
        case charm::app_abi::AppImageFormat::modulex:
            return "modulex"sv;
    }
    return "unknown"sv;
}

[[nodiscard]] inline std::string_view resident_launcher_record_name(
    const ResidentLauncherRunRecord& record) noexcept {
    return charm::app_abi::fat_array_view(record.name);
}

inline void resident_launcher_copy_entry_name(
    ResidentLauncherRunRecord& record,
    const charm::app_abi::FirmwareEntry& entry) noexcept {
    record.name.fill('\0');
    const auto name = charm::app_abi::fat_array_view(entry.name);
    const auto copy = name.size() < record.name.size() ? name.size() : (record.name.size() - 1U);
    if (copy != 0U) {
        std::memcpy(record.name.data(), name.data(), copy);
    }
}

inline void resident_launcher_apply_catalog(
    ResidentLauncherState& state,
    charm::app_abi::FirmwareCatalogCode code,
    const charm::app_abi::FirmwareCatalog& catalog) noexcept {
    state.catalog = catalog;
    state.catalog_code = code;
    if (code != charm::app_abi::FirmwareCatalogCode::ok) {
        state.catalog.count = 0;
    }
    state.selected = 0;
    state.top = 0;
    state.launch_locked = false;
    state.last_run = {};
    state.dirty = true;
}

inline void resident_launcher_apply_catalog_error(
    ResidentLauncherState& state,
    charm::app_abi::FirmwareCatalogCode code) noexcept {
    charm::app_abi::FirmwareCatalog empty{};
    resident_launcher_apply_catalog(state, code, empty);
}

inline void resident_launcher_mark_clean(ResidentLauncherState& state) noexcept {
    state.dirty = false;
}

inline void resident_launcher_move_selection(ResidentLauncherState& state, int delta) noexcept {
    if (state.catalog_code != charm::app_abi::FirmwareCatalogCode::ok ||
        state.catalog.count == 0U || delta == 0) {
        return;
    }

    const auto count = static_cast<int>(state.catalog.count);
    auto next = static_cast<int>(state.selected) + delta;
    while (next < 0) {
        next += count;
    }
    while (next >= count) {
        next -= count;
    }

    state.selected = static_cast<std::uint32_t>(next);
    if (state.selected < state.top) {
        state.top = state.selected;
    } else if (state.selected >= state.top + kResidentLauncherVisibleRows) {
        state.top = state.selected - kResidentLauncherVisibleRows + 1U;
    }
    state.dirty = true;
}

[[nodiscard]] inline ResidentLauncherRunRequest resident_launcher_reduce(
    ResidentLauncherState& state,
    ResidentLauncherInput input) noexcept {
    resident_launcher_move_selection(state, input.select_delta);

    ResidentLauncherRunRequest request{};
    if (!input.confirm || state.launch_locked ||
        state.catalog_code != charm::app_abi::FirmwareCatalogCode::ok ||
        state.selected >= state.catalog.count) {
        return request;
    }

    state.launch_locked = true;
    state.dirty = true;
    request.requested = true;
    request.index = state.selected;
    request.entry = state.catalog.entries[state.selected];
    return request;
}

inline void resident_launcher_record_stage_failure(
    ResidentLauncherState& state,
    const charm::app_abi::FirmwareEntry& entry,
    charm::app_abi::FirmwareCatalogCode code) noexcept {
    ResidentLauncherRunRecord record{};
    record.valid = true;
    record.format = entry.format;
    record.source = entry.source;
    record.stage_code = code;
    record.result.stage = charm::app_abi::AppRunStage::load;
    record.result.code = charm::app_abi::AppRunCode::load_failed;
    resident_launcher_copy_entry_name(record, entry);
    state.last_run = record;
    state.last_run.result.name = resident_launcher_record_name(state.last_run);
    state.launch_locked = true;
    state.dirty = true;
}

inline void resident_launcher_record_run(
    ResidentLauncherState& state,
    ResidentLauncherRunRecord record) noexcept {
    record.valid = true;
    state.last_run = record;
    state.last_run.result.name = resident_launcher_record_name(state.last_run);
    state.launch_locked = true;
    state.dirty = true;
}

[[nodiscard]] inline ResidentLauncherViewModel resident_launcher_view(
    const ResidentLauncherState& state) noexcept {
    ResidentLauncherViewModel view{};
    view.catalog_code = state.catalog_code;
    view.count = state.catalog.count;
    view.selected = state.selected;
    view.top = state.top;
    view.can_run = state.catalog_code == charm::app_abi::FirmwareCatalogCode::ok &&
                   state.catalog.count != 0U && !state.launch_locked;
    view.launch_locked = state.launch_locked;
    view.has_run_record = state.last_run.valid;
    view.run = state.last_run;

    for (std::uint32_t i = 0; i < kResidentLauncherVisibleRows; ++i) {
        const auto index = state.top + i;
        if (index >= state.catalog.count) {
            break;
        }
        const auto& entry = state.catalog.entries[index];
        view.rows[view.row_count++] = ResidentLauncherVisibleRow{
            .index = index,
            .name = charm::app_abi::fat_array_view(entry.name),
            .size = entry.size,
            .format = entry.format,
            .selected = index == state.selected,
        };
    }
    return view;
}

} // namespace h747::apps::resident_launcher
