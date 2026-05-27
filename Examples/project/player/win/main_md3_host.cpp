#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "main_md3_host_sdl.hpp"

#define main run_player_win_md3_impl
#include "main_md3_host.cppm"
#undef main

int run_player_win_md3(int argc, char** argv) {
    return run_player_win_md3_impl(argc, argv);
}
