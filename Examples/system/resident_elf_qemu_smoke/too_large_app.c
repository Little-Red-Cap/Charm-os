#include "charm_app_api.h"

static volatile unsigned char g_too_large_bss[80 * 1024];

int charm_app_main(const CharmAppApi* api, int argc, char** argv) {
    (void)api;
    (void)argc;
    (void)argv;
    g_too_large_bss[0] = 1;
    g_too_large_bss[sizeof(g_too_large_bss) - 1] = 2;
    return 0;
}
