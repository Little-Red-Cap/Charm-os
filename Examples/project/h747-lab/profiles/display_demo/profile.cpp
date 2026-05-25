#include "profile.h"

#include "display_demo.h"

import init.node;
import util.core;
import util.error;

namespace {

constexpr init::CapId kConsoleCap = init::cap_id("h747.console");
constexpr init::CapId kPowerCap = init::cap_id("h747.power");
constexpr init::CapId kDisplayCap = init::cap_id("h747.display");
constexpr init::CapId kAppCap = init::cap_id("h747.app.display_demo");

constexpr init::CapId kConsoleProvides[] = {kConsoleCap};
constexpr init::CapId kPowerProvides[] = {kPowerCap};
constexpr init::CapId kDisplayProvides[] = {kDisplayCap};
constexpr init::CapId kDisplayRequires[] = {kPowerCap};
constexpr init::CapId kAppProvides[] = {kAppCap};
constexpr init::CapId kAppRequires[] = {kConsoleCap, kPowerCap, kDisplayCap};

util::Result<void> init_noop(void*) noexcept {
    return {};
}

util::Result<void> init_app(void*) noexcept {
    h747::apps::display_demo::init();
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

const init::Node kDisplayNode{
    "display",
    init::Phase::service,
    static_cast<util::u32>(init::Runlevel::all),
    std::span<const init::CapId>(kDisplayProvides, 1),
    std::span<const init::CapId>(kDisplayRequires, 1),
    init_noop,
    nullptr,
    nullptr,
};

const init::Node kAppNode{
    "display_demo",
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
    &kDisplayNode,
    &kAppNode,
};

} // namespace

namespace h747::runtime {

const Profile& active_profile() noexcept {
    static const Profile profile{
        "display_demo",
        "h747_diy",
        std::span<const init::Node* const>(kNodes, 4),
        h747::apps::display_demo::loop_once,
    };
    return profile;
}

} // namespace h747::runtime
