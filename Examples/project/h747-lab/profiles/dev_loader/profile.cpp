#include "profile.h"

#include "dev_loader.h"
#include "memory_probe.h"
#include "power.h"
#include "storage.h"

import init.node;
import util.core;
import util.error;

namespace {

constexpr init::CapId kConsoleCap = init::cap_id("h747.console");
constexpr init::CapId kPowerCap = init::cap_id("h747.power");
constexpr init::CapId kMemoryCap = init::cap_id("h747.memory");
constexpr init::CapId kStorageCap = init::cap_id("h747.storage");
constexpr init::CapId kAppCap = init::cap_id("h747.app.dev_loader");

constexpr init::CapId kConsoleProvides[] = {kConsoleCap};
constexpr init::CapId kPowerProvides[] = {kPowerCap};
constexpr init::CapId kMemoryProvides[] = {kMemoryCap};
constexpr init::CapId kMemoryRequires[] = {kPowerCap};
constexpr init::CapId kStorageProvides[] = {kStorageCap};
constexpr init::CapId kStorageRequires[] = {kPowerCap};
constexpr init::CapId kAppProvides[] = {kAppCap};
constexpr init::CapId kAppRequires[] = {kConsoleCap, kPowerCap, kMemoryCap, kStorageCap};

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

util::Result<void> init_storage(void*) noexcept {
    h747_storage_init();
    return {};
}

util::Result<void> init_app(void*) noexcept {
    h747::apps::dev_loader::init();
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

const init::Node kStorageNode{
    "storage",
    init::Phase::service,
    static_cast<util::u32>(init::Runlevel::all),
    std::span<const init::CapId>(kStorageProvides, 1),
    std::span<const init::CapId>(kStorageRequires, 1),
    init_storage,
    nullptr,
    nullptr,
};

const init::Node kAppNode{
    "dev_loader",
    init::Phase::app,
    static_cast<util::u32>(init::Runlevel::all),
    std::span<const init::CapId>(kAppProvides, 1),
    std::span<const init::CapId>(kAppRequires, 4),
    init_app,
    nullptr,
    nullptr,
};

const init::Node* const kNodes[] = {
    &kConsoleNode,
    &kPowerNode,
    &kMemoryNode,
    &kStorageNode,
    &kAppNode,
};

} // namespace

namespace h747::runtime {

const Profile& active_profile() noexcept {
    static const Profile profile{
        "dev_loader",
        "h747_diy",
        std::span<const init::Node* const>(kNodes, 5),
        h747::apps::dev_loader::loop_once,
    };
    return profile;
}

} // namespace h747::runtime
