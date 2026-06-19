#include "charm_app_api.h"

static int write_literal(const CharmAppApi* api, const char* text) {
    unsigned int len = 0;
    while (text[len] != '\0') {
        ++len;
    }
    return api->console.write(text, len);
}

int charm_app_main(const CharmAppApi* api, int argc, char** argv) {
    static unsigned char frame[16u * 16u * 4u];
    (void)argc;
    (void)argv;
    if (api == 0 ||
        api->console.write == 0 ||
        api->display.describe == 0 ||
        api->display.present == 0) {
        return 240;
    }

    CharmAppDisplayMode mode;
    if (api->display.describe(&mode) != CHARM_APP_STATUS_OK) {
        return 241;
    }
    if (mode.width != 16u ||
        mode.height != 16u ||
        mode.stride_bytes != 64u ||
        mode.format != CHARM_APP_PIXEL_FORMAT_ARGB8888) {
        return 242;
    }

    const int code = api->display.present(frame, sizeof(frame) - 4u);
    if (code != CHARM_APP_STATUS_INVALID_ARGUMENT) {
        return 243;
    }

    write_literal(api, "display_error_app: invalid present rejected\n");
    return 0;
}
