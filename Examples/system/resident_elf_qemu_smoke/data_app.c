#include "charm_app_api.h"

static unsigned char g_data_probe[5] = {3, 5, 8, 13, 21};
static unsigned char g_mutable_counter = 7;
static unsigned char g_zero_probe[11];

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

    const unsigned char expected[5] = {3, 5, 8, 13, 21};
    unsigned checksum = 0;
    for (unsigned i = 0; i < sizeof(g_data_probe); ++i) {
        if (g_data_probe[i] != expected[i]) {
            app_write(api, "data_app: data-init failed\n");
            return 23;
        }
        checksum += g_data_probe[i];
    }
    if (g_mutable_counter != 7) {
        app_write(api, "data_app: mutable data failed\n");
        return 24;
    }
    for (unsigned i = 0; i < sizeof(g_zero_probe); ++i) {
        if (g_zero_probe[i] != 0) {
            app_write(api, "data_app: bss failed\n");
            return 25;
        }
    }

    g_data_probe[0] = 34;
    g_mutable_counter = 55;
    g_zero_probe[sizeof(g_zero_probe) - 1] = 89;
    (void)checksum;
    app_write(api, "data_app: data-init ok checksum=50\n");
    return 0;
}
