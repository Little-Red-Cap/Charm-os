#include "player_md3.h"

#include "player_md3_runtime.hpp"

namespace h747::apps::player_md3 {

void init() {
    init_runtime();
}

void loop_once() noexcept {
    loop_runtime();
}

} // namespace h747::apps::player_md3
