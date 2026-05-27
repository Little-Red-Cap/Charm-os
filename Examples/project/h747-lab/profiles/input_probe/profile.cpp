#include "profile.h"

#include "input.h"
#include "input_probe.h"
#include "power.h"

import init.node;
import util.core;
import util.error;

namespace {

constexpr init::CapId kConsoleCap = init::cap_id("h747.console");
constexpr init::CapId kPowerCap = init::cap_id("h747.power");
constexpr init::CapId kInputCap = init::cap_id("h747.input");
constexpr init::CapId kAppCap = init::cap_id("h747.app.input_probe");

constexpr init::CapId kConsoleProvides[] = {kConsoleCap};
constexpr init::CapId kPowerProvides[] = {kPowerCap};
constexpr init::CapId kInputProvides[] = {kInputCap};
constexpr init::CapId kInputRequires[] = {kPowerCap};
constexpr init::CapId kAppProvides[] = {kAppCap};
constexpr init::CapId kAppRequires[] = {kConsoleCap, kPowerCap, kInputCap};

util::Result<void> init_noop(void*) noexcept {
    return {};
}

util::Result<void> init_input(void*) noexcept {
    input_init();
    return {};
}

util::Result<void> init_power(void*) noexcept {
    power_init();
    (void)power_pmic_probe();
    return {};
}

util::Result<void> init_app(void*) noexcept {
    h747::apps::input_probe::init();
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

const init::Node kInputNode{
    "input",
    init::Phase::service,
    static_cast<util::u32>(init::Runlevel::all),
    std::span<const init::CapId>(kInputProvides, 1),
    std::span<const init::CapId>(kInputRequires, 1),
    init_input,
    nullptr,
    nullptr,
};

const init::Node kAppNode{
    "input_probe",
    init::Phase::app,
    static_cast<util::u32>(init::Runlevel::all),
    std::span<const init::CapId>(kAppProvides, 1),
    std::span<const init::CapId>(kAppRequires, 2),
    init_app,
    nullptr,
    nullptr,
};

const init::Node* const kNodes[] = {
    &kConsoleNode,
    &kPowerNode,
    &kInputNode,
    &kAppNode,
};

} // namespace

namespace h747::runtime {

const Profile& active_profile() noexcept {
    static const Profile profile{
        "input_probe",
        "h747_diy",
        std::span<const init::Node* const>(kNodes, 4),
        h747::apps::input_probe::loop_once,
    };
    return profile;
}

} // namespace h747::runtime
