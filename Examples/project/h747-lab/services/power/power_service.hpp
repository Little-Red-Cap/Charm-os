#pragma once

#include "power.h"

#include <cstdint>
#include <optional>
#include <string_view>

namespace h747::power {

using namespace std::literals::string_view_literals;

enum class Rail : std::uint8_t {
    dcdc1,
    dcdc2,
    dcdc3,
    ldo1,
    ldo2,
    ldo3,
    ldo4,
};

enum class Profile : std::uint8_t {
    unknown,
    alive_minimal,
    system_console,
    audio_stage_a,
    display_stage_a,
    network_stage_a,
    storage_stage_a,
};

enum class Transport : std::uint8_t {
    none,
    i2c1_hw,
    i2c1_gpio_swapped,
};

constexpr power_pmic_rail_t to_c(const Rail rail) noexcept {
    switch (rail) {
    case Rail::dcdc1: return POWER_PMIC_RAIL_DCDC1;
    case Rail::dcdc2: return POWER_PMIC_RAIL_DCDC2;
    case Rail::dcdc3: return POWER_PMIC_RAIL_DCDC3;
    case Rail::ldo1: return POWER_PMIC_RAIL_LDO1;
    case Rail::ldo2: return POWER_PMIC_RAIL_LDO2;
    case Rail::ldo3: return POWER_PMIC_RAIL_LDO3;
    case Rail::ldo4: return POWER_PMIC_RAIL_LDO4;
    }
    return POWER_PMIC_RAIL_DCDC1;
}

constexpr power_profile_t to_c(const Profile profile) noexcept {
    switch (profile) {
    case Profile::alive_minimal: return POWER_PROFILE_ALIVE_MINIMAL;
    case Profile::system_console: return POWER_PROFILE_SYSTEM_CONSOLE;
    case Profile::audio_stage_a: return POWER_PROFILE_AUDIO_STAGE_A;
    case Profile::display_stage_a: return POWER_PROFILE_DISPLAY_STAGE_A;
    case Profile::network_stage_a: return POWER_PROFILE_NETWORK_STAGE_A;
    case Profile::storage_stage_a: return POWER_PROFILE_STORAGE_STAGE_A;
    case Profile::unknown: return POWER_PROFILE_UNKNOWN;
    }
    return POWER_PROFILE_UNKNOWN;
}

constexpr Transport transport_from_c(const power_pmic_transport_t transport) noexcept {
    switch (transport) {
    case POWER_PMIC_TRANSPORT_I2C1_HW: return Transport::i2c1_hw;
    case POWER_PMIC_TRANSPORT_I2C1_GPIO_BITBANG_SWAPPED: return Transport::i2c1_gpio_swapped;
    case POWER_PMIC_TRANSPORT_NONE: return Transport::none;
    }
    return Transport::none;
}

constexpr std::string_view rail_name(const Rail rail) noexcept {
    switch (rail) {
    case Rail::dcdc1: return "dcdc1"sv;
    case Rail::dcdc2: return "dcdc2"sv;
    case Rail::dcdc3: return "dcdc3"sv;
    case Rail::ldo1: return "ldo1"sv;
    case Rail::ldo2: return "ldo2"sv;
    case Rail::ldo3: return "ldo3"sv;
    case Rail::ldo4: return "ldo4"sv;
    }
    return "unknown"sv;
}

constexpr std::string_view transport_name(const Transport transport) noexcept {
    switch (transport) {
    case Transport::i2c1_hw: return "i2c1_hw"sv;
    case Transport::i2c1_gpio_swapped: return "i2c1_gpio_swapped"sv;
    case Transport::none: return "none"sv;
    }
    return "none"sv;
}

inline std::optional<Rail> parse_rail(const std::string_view text) noexcept {
    if (text == "dcdc1"sv) return Rail::dcdc1;
    if (text == "dcdc2"sv) return Rail::dcdc2;
    if (text == "dcdc3"sv) return Rail::dcdc3;
    if (text == "ldo1"sv) return Rail::ldo1;
    if (text == "ldo2"sv) return Rail::ldo2;
    if ((text == "ldo3"sv) || (text == "ls1"sv)) return Rail::ldo3;
    if ((text == "ldo4"sv) || (text == "ls2"sv)) return Rail::ldo4;
    return std::nullopt;
}

struct PmicSnapshot {
    power_pmic_snapshot_t raw{};

    [[nodiscard]] bool ready() const noexcept {
        return raw.ready != 0U;
    }

    [[nodiscard]] Transport transport() const noexcept {
        return transport_from_c(static_cast<power_pmic_transport_t>(raw.transport));
    }

    [[nodiscard]] std::string_view transport_text() const noexcept {
        return transport_name(transport());
    }
};

class Pmic {
public:
    void init() const noexcept {
        power_init();
    }

    bool apply(const Profile profile) const noexcept {
        return power_apply_profile(to_c(profile)) != 0U;
    }

    bool probe() const noexcept {
        return power_pmic_probe() != 0U;
    }

    [[nodiscard]] PmicSnapshot snapshot() const noexcept {
        return PmicSnapshot{power_pmic_snapshot()};
    }

    [[nodiscard]] std::string_view current_profile_name() const noexcept {
        return power_profile_name(power_current_profile());
    }

    bool set_enabled(const Rail rail, const bool enabled) const noexcept {
        return power_pmic_set_rail_enabled(to_c(rail), enabled ? 1U : 0U) != 0U;
    }

    bool set_voltage_mv(const Rail rail, const std::uint16_t millivolts) const noexcept {
        return power_pmic_set_rail_voltage_mv(to_c(rail), millivolts) != 0U;
    }
};

} // namespace h747::power
