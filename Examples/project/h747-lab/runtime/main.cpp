#include "foundation.h"
#include "profile.h"

#include "console.h"

import init.graph;

namespace {

void print_boot_banner(const h747::runtime::Profile& profile) {
    h747::console::write_line("========================================");
    h747::console::write_line("   H747 Lab");
    h747::console::write("   target profile=");
    h747::console::write_line(profile.name.data());
    h747::console::write("   board=");
    h747::console::write_line(profile.board.data());
    h747::console::write_line("========================================");
}

} // namespace

int main() {
    h747::runtime::foundation_init();

    const auto& profile = h747::runtime::active_profile();
    print_boot_banner(profile);

    init::Graph<12, 24> graph{};
    auto build = graph.build(profile.nodes);
    if (!build) {
        h747::runtime::foundation_fail("init.graph build failed");
    }

    auto start = graph.start();
    if (!start) {
        h747::runtime::foundation_fail("init.graph start failed");
    }

    while (true) {
        if (profile.loop_once != nullptr) {
            profile.loop_once();
        }
    }
}
