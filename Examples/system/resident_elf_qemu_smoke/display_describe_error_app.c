#include "charm_app_api.h"

static int write_literal(const CharmAppApi* api, const char* text) {
    unsigned int len = 0;
    while (text[len] != '\0') {
        ++len;
    }
    return api->console.write(text, len);
}

int charm_app_main(const CharmAppApi* api, int argc, char** argv) {
    (void)argc;
    (void)argv;
    if (api == 0 || api->console.write == 0 || api->display.describe == 0) {
        return 238;
    }

    const int code = api->display.describe(0);
    if (code != CHARM_APP_STATUS_INVALID_ARGUMENT) {
        return 239;
    }

    write_literal(api, "display_describe_error_app: null describe rejected\n");
    return 0;
}
