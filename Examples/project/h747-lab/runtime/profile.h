#pragma once

#include <span>
#include <string_view>

import init.node;

namespace h747::runtime {

struct Profile {
    std::string_view name;
    std::string_view board;
    std::span<const init::Node* const> nodes;
    void (*loop_once)() noexcept;
};

const Profile& active_profile() noexcept;

} // namespace h747::runtime
