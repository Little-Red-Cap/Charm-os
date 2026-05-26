#include "profile.h"

#include "display_raster.h"
#include "memory_probe.h"
#include "player.h"
#include "power.h"

import init.node;
import util.core;
import util.error;

namespace {

constexpr init::CapId kConsoleCap = init::cap_id("h747.console");
constexpr init::CapId kPowerCap = init::cap_id("h747.power");
constexpr init::CapId kMemoryCap = init::cap_id("h747.memory");
constexpr init::CapId kDisplayCap = init::cap_id("h747.display.raster");
constexpr init::CapId kAppCap = init::cap_id("h747.app.player");

constexpr init::CapId kConsoleProvides[] = {kConsoleCap};
constexpr init::CapId kPowerProvides[] = {kPowerCap};
constexpr init::CapId kMemoryProvides[] = {kMemoryCap};
constexpr init::CapId kMemoryRequires[] = {kPowerCap};
constexpr init::CapId kDisplayProvides[] = {kDisplayCap};
constexpr init::CapId kDisplayRequires[] = {kPowerCap, kMemoryCap};
constexpr init::CapId kAppProvides[] = {kAppCap};
constexpr init::CapId kAppRequires[] = {kConsoleCap, kPowerCap, kMemoryCap, kDisplayCap};

util::Result<void> init_noop(void*) noexcept {
    return {};
}

util::Result<void> init_power(void*) noexcept {
    power_init();
    (void)power_pmic_probe();
    return {};
}

util::Result<void> init_memory(void*) noexcept {
    memory_probe_storage_init();
    return {};
}

util::Result<void> init_display(void*) noexcept {
    if (display_raster_init() == 0U) {
        return util::unexpected(util::Errc::io);
    }
    return {};
}

util::Result<void> init_app(void*) noexcept {
    h747::apps::player::init();
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

const init::Node kAppNode{
    "player",
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
    &kDisplayNode,
    &kAppNode,
};

} // namespace

namespace h747::runtime {

const Profile& active_profile() noexcept {
    static const Profile profile{
        "player",
        "h747_diy",
        std::span<const init::Node* const>(kNodes, 5),
        h747::apps::player::loop_once,
    };
    return profile;
}

} // namespace h747::runtime
