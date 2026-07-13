#include "profile.h"

#include "capability_mvp.h"
#include "port.h"

import init.node;
import util.core;
import util.error;

namespace {

constexpr init::CapId kConsoleCap = init::cap_id("h747.console");
constexpr init::CapId kAppCap = init::cap_id("h747.app.capability_mvp");
constexpr std::uint32_t kProbeReleaseDelayMs = 500U;

constexpr init::CapId kConsoleProvides[] = {kConsoleCap};
constexpr init::CapId kAppProvides[] = {kAppCap};
constexpr init::CapId kAppRequires[] = {kConsoleCap};

util::Result<void> init_noop(void*) noexcept {
    return {};
}

util::Result<void> init_app(void*) noexcept {
    h747::port::delay_ms(kProbeReleaseDelayMs);
    h747::apps::capability_mvp::init();
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

const init::Node kAppNode{
    "capability_mvp",
    init::Phase::app,
    static_cast<util::u32>(init::Runlevel::all),
    std::span<const init::CapId>(kAppProvides, 1),
    std::span<const init::CapId>(kAppRequires, 1),
    init_app,
    nullptr,
    nullptr,
};

const init::Node* const kNodes[] = {
    &kConsoleNode,
    &kAppNode,
};

} // namespace

namespace h747::runtime {

const Profile& active_profile() noexcept {
    static const Profile profile{
        "capability_mvp",
        "h747_diy",
        std::span<const init::Node* const>(kNodes, 2),
        h747::apps::capability_mvp::loop_once,
    };
    return profile;
}

} // namespace h747::runtime
