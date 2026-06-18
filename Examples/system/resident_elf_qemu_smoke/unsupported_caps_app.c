#include "charm_app_api.h"

int charm_app_main(const CharmAppApi* api, int argc, char** argv) {
    (void)argc;
    (void)argv;
    if (api == 0 ||
        api->storage.open == 0 ||
        api->storage.read == 0 ||
        api->storage.write == 0 ||
        api->storage.close == 0 ||
        api->afe.configure == 0 ||
        api->afe.read == 0) {
        return 211;
    }
    if (api->storage.open("/missing", 0, 0) != CHARM_APP_STATUS_UNSUPPORTED) {
        return 212;
    }
    if (api->storage.read(-1, 0, 0) != CHARM_APP_STATUS_UNSUPPORTED) {
        return 213;
    }
    if (api->storage.write(-1, 0, 0) != CHARM_APP_STATUS_UNSUPPORTED) {
        return 214;
    }
    if (api->storage.close(-1) != CHARM_APP_STATUS_UNSUPPORTED) {
        return 215;
    }
    if (api->afe.configure(1u, 48000u) != CHARM_APP_STATUS_UNSUPPORTED) {
        return 216;
    }
    if (api->afe.read(0, 0) != CHARM_APP_STATUS_UNSUPPORTED) {
        return 217;
    }
    return 0;
}
