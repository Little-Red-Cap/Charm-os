#include "profile.h"

#include "diag_shell.h"

import init.node;
import util.core;
import util.error;

namespace {

constexpr init::CapId kConsoleCap = init::cap_id("h747.console");
constexpr init::CapId kPowerCap = init::cap_id("h747.power");
constexpr init::CapId kMemoryCap = init::cap_id("h747.memory");
constexpr init::CapId kAppCap = init::cap_id("h747.app.diag_shell");

constexpr init::CapId kConsoleProvides[] = {kConsoleCap};
constexpr init::CapId kPowerProvides[] = {kPowerCap};
constexpr init::CapId kMemoryProvides[] = {kMemoryCap};
constexpr init::CapId kMemoryRequires[] = {kPowerCap};
constexpr init::CapId kAppProvides[] = {kAppCap};
constexpr init::CapId kAppRequires[] = {kConsoleCap, kPowerCap, kMemoryCap};

util::Result<void> init_noop(void*) noexcept {
    return {};
}

util::Result<void> init_app(void*) noexcept {
    h747::apps::diag_shell::init();
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
    init_noop,
    nullptr,
    nullptr,
};

const init::Node kMemoryNode{
    "memory",
    init::Phase::service,
    static_cast<util::u32>(init::Runlevel::all),
    std::span<const init::CapId>(kMemoryProvides, 1),
    std::span<const init::CapId>(kMemoryRequires, 1),
    init_noop,
    nullptr,
    nullptr,
};

const init::Node kAppNode{
    "diag_shell",
    init::Phase::app,
    static_cast<util::u32>(init::Runlevel::all),
    std::span<const init::CapId>(kAppProvides, 1),
    std::span<const init::CapId>(kAppRequires, 3),
    init_app,
    nullptr,
    nullptr,
};

const init::Node* const kNodes[] = {
    &kConsoleNode,
    &kPowerNode,
    &kMemoryNode,
    &kAppNode,
};

} // namespace

namespace h747::runtime {

const Profile& active_profile() noexcept {
    static const Profile profile{
        "diag_shell",
        "h747_diy",
        std::span<const init::Node* const>(kNodes, 4),
        h747::apps::diag_shell::loop_once,
    };
    return profile;
}

} // namespace h747::runtime
