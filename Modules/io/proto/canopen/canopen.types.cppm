//
// Created by Joho on 2026/03/05.
//

module;
#include <array>

export module canopen.types;

import util.core;

export namespace canopen {
    using NodeId = util::u8;
    using Index = util::u16;
    using SubIndex = util::u8;
    using CobId = util::u16;

    struct CanFrame {
        CobId id{0};
        util::u8 dlc{0};
        std::array<util::u8, 8> data{};
    };

    [[nodiscard]] constexpr CobId sdo_request_id(NodeId node) noexcept {
        return static_cast<CobId>(0x600u + static_cast<CobId>(node));
    }

    [[nodiscard]] constexpr CobId sdo_response_id(NodeId node) noexcept {
        return static_cast<CobId>(0x580u + static_cast<CobId>(node));
    }

    [[nodiscard]] constexpr CobId nmt_id() noexcept {
        return static_cast<CobId>(0x000u);
    }

    [[nodiscard]] constexpr CobId heartbeat_id(NodeId node) noexcept {
        return static_cast<CobId>(0x700u + static_cast<CobId>(node));
    }
} // namespace canopen
