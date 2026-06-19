#include "charm_app_api.h"

int charm_app_main(const CharmAppApi* api, int argc, char** argv) {
    (void)argc;
    (void)argv;
    if (api == 0 || api->time.now_ms == 0) {
        return 231;
    }

    const uint32_t first = api->time.now_ms();
    const uint32_t second = api->time.now_ms();
    const uint32_t third = api->time.now_ms();
    const uint32_t fourth = api->time.now_ms();
    if (first != 1017u || second != 1034u || third != 1051u || fourth != 1068u) {
        return 232;
    }
    return 0;
}
