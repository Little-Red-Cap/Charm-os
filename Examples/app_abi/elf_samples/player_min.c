#include "player_min_core.h"

int charm_app_main(const CharmAppApi* api, int argc, char** argv) {
    return charm_player_min_run(api, argc, argv);
}
