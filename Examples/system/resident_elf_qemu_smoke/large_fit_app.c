#include "charm_app_api.h"

static unsigned char g_large_fit_bss[60 * 1024];

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

    if (g_large_fit_bss[0] != 0 ||
        g_large_fit_bss[(sizeof(g_large_fit_bss) / 2U)] != 0 ||
        g_large_fit_bss[sizeof(g_large_fit_bss) - 1U] != 0) {
        app_write(api, "large_fit_app: zero-fill failed\n");
        return 23;
    }

    g_large_fit_bss[0] = 1;
    g_large_fit_bss[(sizeof(g_large_fit_bss) / 2U)] = 2;
    g_large_fit_bss[sizeof(g_large_fit_bss) - 1U] = 3;
    app_write(api, "large_fit_app: near-limit ok\n");
    return 0;
}
