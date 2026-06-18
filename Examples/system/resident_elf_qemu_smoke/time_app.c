#include "charm_app_api.h"

int charm_app_main(const CharmAppApi* api, int argc, char** argv) {
    (void)argc;
    (void)argv;
    if (api == 0 || api->time.now_ms == 0) {
        return 221;
    }
    const unsigned first = api->time.now_ms();
    const unsigned second = api->time.now_ms();
    if ((second - first) != 17u) {
        return 222;
    }
    return 0;
}
