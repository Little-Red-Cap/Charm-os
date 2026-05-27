export module player.win.md3_host;

#define main run_player_win_md3_impl
#include "main_md3_host.cppm"
#undef main

export int run_player_win_md3(int argc, char** argv) {
    return run_player_win_md3_impl(argc, argv);
}
