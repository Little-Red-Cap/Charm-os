#include "profile.h"

#include "display_raster.h"
#include "input.h"
#include "memory_probe.h"
#include "power.h"
#include "resident_launcher.h"
#include "storage.h"

import init.node;
import util.core;
import util.error;

namespace {

constexpr init::CapId kConsoleCap = init::cap_id("h747.console");
constexpr init::CapId kPowerCap = init::cap_id("h747.power");
constexpr init::CapId kMemoryCap = init::cap_id("h747.memory");
constexpr init::CapId kDisplayCap = init::cap_id("h747.display.raster");
constexpr init::CapId kStorageCap = init::cap_id("h747.storage.emmc");
constexpr init::CapId kInputCap = init::cap_id("h747.input");
constexpr init::CapId kAppCap = init::cap_id("h747.app.resident_launcher");

constexpr init::CapId kConsoleProvides[] = {kConsoleCap};
constexpr init::CapId kPowerProvides[] = {kPowerCap};
constexpr init::CapId kMemoryProvides[] = {kMemoryCap};
constexpr init::CapId kMemoryRequires[] = {kPowerCap};
constexpr init::CapId kDisplayProvides[] = {kDisplayCap};
constexpr init::CapId kDisplayRequires[] = {kPowerCap, kMemoryCap};
constexpr init::CapId kStorageProvides[] = {kStorageCap};
constexpr init::CapId kStorageRequires[] = {kPowerCap};
constexpr init::CapId kInputProvides[] = {kInputCap};
constexpr init::CapId kAppProvides[] = {kAppCap};
constexpr init::CapId kAppRequires[] = {
    kConsoleCap,
    kPowerCap,
    kMemoryCap,
    kDisplayCap,
    kStorageCap,
    kInputCap,
};

util::Result<void> init_noop(void*) noexcept {
    return {};
}

util::Result<void> init_power(void*) noexcept {
    power_init();
    (void)power_apply_profile(POWER_PROFILE_STORAGE_STAGE_A);
    return {};
}

util::Result<void> init_memory(void*) noexcept {
    memory_probe_storage_init();
    (void)memory_probe_configure_sdram_mpu_normal();
    (void)memory_probe_sdram2_smoke();
    return {};
}

util::Result<void> init_display(void*) noexcept {
    if (display_raster_init() == 0U) {
        return util::unexpected(util::Errc::io);
    }
    return {};
}

util::Result<void> init_storage(void*) noexcept {
    h747_storage_init();
    return {};
}

util::Result<void> init_input(void*) noexcept {
    input_init();
    return {};
}

util::Result<void> init_app(void*) noexcept {
    h747::apps::resident_launcher::init();
    return {};
}

const init::Node kConsoleNode{
    "console",
    init::Phase::core,
    static_cast<util::u32>(init::Runlevel::all),
    std::span<const init::CapId>(kConsoleProvides, 1),
    {},
    init_noop,
    nullptr,
    nullptr,
};

const init::Node kPowerNode{
    "power",
    init::Phase::service,
    static_cast<util::u32>(init::Runlevel::all),
    std::span<const init::CapId>(kPowerProvides, 1),
    {},
    init_power,
    nullptr,
    nullptr,
};

const init::Node kMemoryNode{
    "memory",
    init::Phase::service,
    static_cast<util::u32>(init::Runlevel::all),
    std::span<const init::CapId>(kMemoryProvides, 1),
    std::span<const init::CapId>(kMemoryRequires, 1),
    init_memory,
    nullptr,
    nullptr,
};

const init::Node kDisplayNode{
    "display_raster",
    init::Phase::service,
    static_cast<util::u32>(init::Runlevel::all),
    std::span<const init::CapId>(kDisplayProvides, 1),
    std::span<const init::CapId>(kDisplayRequires, 2),
    init_display,
    nullptr,
    nullptr,
};

const init::Node kStorageNode{
    "storage_emmc",
    init::Phase::service,
    static_cast<util::u32>(init::Runlevel::all),
    std::span<const init::CapId>(kStorageProvides, 1),
    std::span<const init::CapId>(kStorageRequires, 1),
    init_storage,
    nullptr,
    nullptr,
};

const init::Node kInputNode{
    "input",
    init::Phase::service,
    static_cast<util::u32>(init::Runlevel::all),
    std::span<const init::CapId>(kInputProvides, 1),
    {},
    init_input,
    nullptr,
    nullptr,
};

const init::Node kAppNode{
    "resident_launcher",
    init::Phase::app,
    static_cast<util::u32>(init::Runlevel::all),
    std::span<const init::CapId>(kAppProvides, 1),
    std::span<const init::CapId>(kAppRequires, 6),
    init_app,
    nullptr,
    nullptr,
};

const init::Node* const kNodes[] = {
    &kConsoleNode,
    &kPowerNode,
    &kMemoryNode,
    &kDisplayNode,
    &kStorageNode,
    &kInputNode,
    &kAppNode,
};

} // namespace

namespace h747::runtime {

const Profile& active_profile() noexcept {
    static const Profile profile{
        "resident_launcher",
        "h747_diy",
        std::span<const init::Node* const>(kNodes, 7),
        h747::apps::resident_launcher::loop_once,
    };
    return profile;
}

} // namespace h747::runtime
