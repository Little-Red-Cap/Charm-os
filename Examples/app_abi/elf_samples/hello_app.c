#include "charm_app_api.h"

static int app_write(const CharmAppApi* api, const char* text) {
    const char* p = text;
    size_t len = 0;
    while (p != 0 && p[len] != '\0') {
        ++len;
    }
    if (api == 0 || api->console.write == 0) {
        return -1;
    }
    return api->console.write(text, len);
}

int charm_app_main(const CharmAppApi* api, int argc, char** argv) {
    if (api == 0 || api->magic != CHARM_APP_API_MAGIC ||
        api->version != CHARM_APP_API_VERSION ||
        api->size < sizeof(CharmAppApi)) {
        return 101;
    }
    app_write(api, "hello_app: charm_app_main entered\n");
    app_write(api, "hello_app: argc=");
    if (argc == 0) {
        app_write(api, "0\n");
    } else if (argc == 1) {
        app_write(api, "1\n");
    } else {
        app_write(api, "many\n");
    }
    if (argc > 1 && argv != 0 && argv[1] != 0) {
        app_write(api, "hello_app: argv1=");
        app_write(api, argv[1]);
        app_write(api, "\n");
    }
    return 0;
}
