#include "charm_app_api.h"

static unsigned char g_zero_fill_probe[257];

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
    (void)argc;
    (void)argv;
    if (api == 0 || api->magic != CHARM_APP_API_MAGIC ||
        api->version != CHARM_APP_API_VERSION ||
        api->size < sizeof(CharmAppApi)) {
        return 101;
    }

    for (unsigned i = 0; i < sizeof(g_zero_fill_probe); ++i) {
        if (g_zero_fill_probe[i] != 0) {
            app_write(api, "bss_app: zero-fill failed\n");
            return 22;
        }
    }

    g_zero_fill_probe[0] = 7;
    g_zero_fill_probe[sizeof(g_zero_fill_probe) - 1] = 9;
    app_write(api, "bss_app: zero-fill ok\n");
    return 0;
}
