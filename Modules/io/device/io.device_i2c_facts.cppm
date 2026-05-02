module;

#include <span>
#include <string_view>

export module io.device_i2c_facts;

import io.device_i2c;
import util.core;

export namespace io::device {
    enum class I2cFactKind : util::u8 {
        bus,
        controller,
        device,
        clock_domain,
        pinmux,
        power_domain,
        backend,
        evidence,
    };

    enum class I2cFactRequirement : util::u8 {
        optional,
        required,
    };

    enum class I2cFactState : util::u8 {
        unknown,
        provided,
        missing,
    };

    struct I2cFact {
        I2cFactKind kind{I2cFactKind::bus};
        I2cFactRequirement requirement{I2cFactRequirement::required};
        I2cFactState state{I2cFactState::unknown};
        I2cAddress address{0};
        std::string_view name{};
        std::string_view source{};
    };

    struct I2cFactResolution {
        util::usize required_count{0};
        util::usize provided_count{0};
        util::usize missing_count{0};
        util::usize optional_unknown_count{0};

        [[nodiscard]] constexpr bool satisfied() const noexcept {
            return missing_count == 0;
        }
    };

    [[nodiscard]] constexpr std::string_view to_string(I2cFactKind kind) noexcept {
        switch (kind) {
        case I2cFactKind::bus:
            return "i2c.bus";
        case I2cFactKind::controller:
            return "i2c.controller";
        case I2cFactKind::device:
            return "i2c.device";
        case I2cFactKind::clock_domain:
            return "clock.domain";
        case I2cFactKind::pinmux:
            return "pinmux";
        case I2cFactKind::power_domain:
            return "power.domain";
        case I2cFactKind::backend:
            return "i2c.backend";
        case I2cFactKind::evidence:
            return "i2c.evidence";
        default:
            return "unknown";
        }
    }

    [[nodiscard]] constexpr bool is_required(I2cFact fact) noexcept {
        return fact.requirement == I2cFactRequirement::required;
    }

    [[nodiscard]] constexpr bool is_provided(I2cFact fact) noexcept {
        return fact.state == I2cFactState::provided;
    }

    [[nodiscard]] constexpr bool is_missing(I2cFact fact) noexcept {
        return fact.requirement == I2cFactRequirement::required
            && fact.state != I2cFactState::provided;
    }

    [[nodiscard]] constexpr I2cFactResolution resolve_i2c_facts(
        std::span<const I2cFact> facts) noexcept {
        I2cFactResolution resolution{};
        for (auto fact : facts) {
            if (is_required(fact)) {
                ++resolution.required_count;
                if (is_provided(fact)) {
                    ++resolution.provided_count;
                } else {
                    ++resolution.missing_count;
                }
            } else if (is_provided(fact)) {
                ++resolution.provided_count;
            } else {
                ++resolution.optional_unknown_count;
            }
        }
        return resolution;
    }

    [[nodiscard]] constexpr I2cFact required_i2c_bus(std::string_view name,
                                                     std::string_view source = {}) noexcept {
        return I2cFact{
            .kind = I2cFactKind::bus,
            .requirement = I2cFactRequirement::required,
            .state = I2cFactState::unknown,
            .name = name,
            .source = source,
        };
    }

    [[nodiscard]] constexpr I2cFact provided_i2c_bus(std::string_view name,
                                                     std::string_view source = {}) noexcept {
        return I2cFact{
            .kind = I2cFactKind::bus,
            .requirement = I2cFactRequirement::required,
            .state = I2cFactState::provided,
            .name = name,
            .source = source,
        };
    }

    [[nodiscard]] constexpr I2cFact required_i2c_device(std::string_view bus_name,
                                                        I2cAddress address,
                                                        std::string_view source = {}) noexcept {
        return I2cFact{
            .kind = I2cFactKind::device,
            .requirement = I2cFactRequirement::required,
            .state = I2cFactState::unknown,
            .address = address,
            .name = bus_name,
            .source = source,
        };
    }

    [[nodiscard]] constexpr I2cFact provided_i2c_device(std::string_view bus_name,
                                                        I2cAddress address,
                                                        std::string_view source = {}) noexcept {
        return I2cFact{
            .kind = I2cFactKind::device,
            .requirement = I2cFactRequirement::required,
            .state = I2cFactState::provided,
            .address = address,
            .name = bus_name,
            .source = source,
        };
    }

    [[nodiscard]] constexpr I2cFact required_i2c_support_fact(
        I2cFactKind kind,
        std::string_view name,
        std::string_view source = {}) noexcept {
        return I2cFact{
            .kind = kind,
            .requirement = I2cFactRequirement::required,
            .state = I2cFactState::unknown,
            .name = name,
            .source = source,
        };
    }

    [[nodiscard]] constexpr I2cFact provided_i2c_support_fact(
        I2cFactKind kind,
        std::string_view name,
        std::string_view source = {}) noexcept {
        return I2cFact{
            .kind = kind,
            .requirement = I2cFactRequirement::required,
            .state = I2cFactState::provided,
            .name = name,
            .source = source,
        };
    }

    [[nodiscard]] constexpr I2cFact optional_i2c_fact(I2cFactKind kind,
                                                      std::string_view name,
                                                      std::string_view source = {}) noexcept {
        return I2cFact{
            .kind = kind,
            .requirement = I2cFactRequirement::optional,
            .state = I2cFactState::unknown,
            .name = name,
            .source = source,
        };
    }
}
