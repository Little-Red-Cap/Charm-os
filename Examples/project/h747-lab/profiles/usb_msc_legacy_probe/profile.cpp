#include "profile.h"

#include "power.h"
#include "storage.h"
#include "usb_msc_legacy_probe.h"
#include "usb_msc_legacy_service.hpp"

import init.node;
import util.core;
import util.error;

namespace {

constexpr init::CapId kConsoleCap = init::cap_id("h747.console");
constexpr init::CapId kPowerCap = init::cap_id("h747.power");
constexpr init::CapId kStorageCap = init::cap_id("h747.storage.emmc");
constexpr init::CapId kUsbMscCap = init::cap_id("h747.usb.msc.legacy");
constexpr init::CapId kAppCap = init::cap_id("h747.app.usb_msc_legacy_probe");

constexpr init::CapId kConsoleProvides[] = {kConsoleCap};
constexpr init::CapId kPowerProvides[] = {kPowerCap};
constexpr init::CapId kStorageProvides[] = {kStorageCap};
constexpr init::CapId kStorageRequires[] = {kPowerCap};
constexpr init::CapId kUsbMscProvides[] = {kUsbMscCap};
constexpr init::CapId kUsbMscRequires[] = {kPowerCap, kStorageCap};
constexpr init::CapId kAppProvides[] = {kAppCap};
constexpr init::CapId kAppRequires[] = {kConsoleCap, kPowerCap, kStorageCap, kUsbMscCap};

util::Result<void> init_noop(void*) noexcept {
    return {};
}

util::Result<void> init_power(void*) noexcept {
    power_init();
    (void)power_pmic_probe();
    return {};
}

util::Result<void> init_storage(void*) noexcept {
    h747_storage_init();
    return {};
}

util::Result<void> init_usb_msc(void*) noexcept {
    h747::usb_msc_legacy::init();
    return {};
}

util::Result<void> init_app(void*) noexcept {
    h747::apps::usb_msc_legacy_probe::init();
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

const init::Node kUsbMscNode{
    "usb_msc_legacy",
    init::Phase::service,
    static_cast<util::u32>(init::Runlevel::all),
    std::span<const init::CapId>(kUsbMscProvides, 1),
    std::span<const init::CapId>(kUsbMscRequires, 2),
    init_usb_msc,
    nullptr,
    nullptr,
};

const init::Node kAppNode{
    "usb_msc_legacy_probe",
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
    &kStorageNode,
    &kUsbMscNode,
    &kAppNode,
};

} // namespace

namespace h747::runtime {

const Profile& active_profile() noexcept {
    static const Profile profile{
        "usb_msc_legacy_probe",
        "h747_diy",
        std::span<const init::Node* const>(kNodes, 5),
        h747::apps::usb_msc_legacy_probe::loop_once,
    };
    return profile;
}

} // namespace h747::runtime
