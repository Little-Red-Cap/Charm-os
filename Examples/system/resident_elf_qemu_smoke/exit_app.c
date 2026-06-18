#include "charm_app_api.h"

int charm_app_main(const CharmAppApi* api, int argc, char** argv) {
    (void)argc;
    (void)argv;
    if (api == 0 || api->app.exit == 0) {
        return 201;
    }
    api->app.exit(7);
    return 0;
}
