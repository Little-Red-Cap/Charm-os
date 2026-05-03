module;

#include <span>
#include <string_view>

export module platform.board_facts;

import util.core;

export namespace platform::board {
    enum class BoardFactKind : util::u8 {
        board,
        profile,
        facet,
        capability,
        i2c_bus,
        i2c_device,
        i2c_controller,
        controller,
        clock_domain,
        pinmux,
        power_domain,
        memory_region,
        storage,
        evidence,
    };

    enum class BoardFactRequirement : util::u8 {
        optional,
        required,
    };

    enum class BoardFactState : util::u8 {
        unknown,
        provided,
        missing,
    };

    struct BoardFact {
        BoardFactKind kind{BoardFactKind::board};
        BoardFactRequirement requirement{BoardFactRequirement::required};
        BoardFactState state{BoardFactState::unknown};
        std::string_view name{};
        std::string_view source{};
    };

    struct BoardFactResolution {
        util::usize required_count{0};
        util::usize provided_count{0};
        util::usize missing_count{0};
        util::usize optional_unknown_count{0};

        [[nodiscard]] constexpr bool satisfied() const noexcept {
            return missing_count == 0;
        }
    };

    [[nodiscard]] constexpr std::string_view to_string(BoardFactKind kind) noexcept {
        switch (kind) {
        case BoardFactKind::board:
            return "board";
        case BoardFactKind::profile:
            return "profile";
        case BoardFactKind::facet:
            return "facet";
        case BoardFactKind::capability:
            return "capability";
        case BoardFactKind::i2c_bus:
            return "i2c.bus";
        case BoardFactKind::i2c_device:
            return "i2c.device";
        case BoardFactKind::i2c_controller:
            return "i2c.controller";
        case BoardFactKind::controller:
            return "controller";
        case BoardFactKind::clock_domain:
            return "clock.domain";
        case BoardFactKind::pinmux:
            return "pinmux";
        case BoardFactKind::power_domain:
            return "power.domain";
        case BoardFactKind::memory_region:
            return "memory.region";
        case BoardFactKind::storage:
            return "storage";
        case BoardFactKind::evidence:
            return "board.evidence";
        default:
            return "unknown";
        }
    }

    [[nodiscard]] constexpr bool is_required(BoardFact fact) noexcept {
        return fact.requirement == BoardFactRequirement::required;
    }

    [[nodiscard]] constexpr bool is_provided(BoardFact fact) noexcept {
        return fact.state == BoardFactState::provided;
    }

    [[nodiscard]] constexpr bool is_missing(BoardFact fact) noexcept {
        return fact.requirement == BoardFactRequirement::required
            && fact.state != BoardFactState::provided;
    }

    [[nodiscard]] constexpr BoardFactResolution resolve_board_facts(
        std::span<const BoardFact> facts) noexcept {
        BoardFactResolution resolution{};
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

    [[nodiscard]] constexpr BoardFact required_board_fact(
        BoardFactKind kind,
        std::string_view name,
        std::string_view source = {}) noexcept {
        return BoardFact{
            .kind = kind,
            .requirement = BoardFactRequirement::required,
            .state = BoardFactState::unknown,
            .name = name,
            .source = source,
        };
    }

    [[nodiscard]] constexpr BoardFact provided_board_fact(
        BoardFactKind kind,
        std::string_view name,
        std::string_view source = {}) noexcept {
        return BoardFact{
            .kind = kind,
            .requirement = BoardFactRequirement::required,
            .state = BoardFactState::provided,
            .name = name,
            .source = source,
        };
    }

    [[nodiscard]] constexpr BoardFact missing_board_fact(
        BoardFactKind kind,
        std::string_view name,
        std::string_view source = {}) noexcept {
        return BoardFact{
            .kind = kind,
            .requirement = BoardFactRequirement::required,
            .state = BoardFactState::missing,
            .name = name,
            .source = source,
        };
    }

    [[nodiscard]] constexpr BoardFact optional_board_fact(
        BoardFactKind kind,
        std::string_view name,
        std::string_view source = {}) noexcept {
        return BoardFact{
            .kind = kind,
            .requirement = BoardFactRequirement::optional,
            .state = BoardFactState::unknown,
            .name = name,
            .source = source,
        };
    }
}
