#include "charm_app_api.h"

static int write_literal(const CharmAppApi* api, const char* text) {
    unsigned int len = 0;
    while (text[len] != '\0') {
        ++len;
    }
    return api->console.write(text, len);
}

static void fill_frame(unsigned char* frame, unsigned int value) {
    for (unsigned int i = 0; i < (16u * 16u * 4u); ++i) {
        frame[i] = (unsigned char)value;
    }
}

int charm_app_main(const CharmAppApi* api, int argc, char** argv) {
    static unsigned char frame[16u * 16u * 4u];
    (void)argc;
    (void)argv;
    if (api == 0 ||
        api->console.write == 0 ||
        api->display.describe == 0 ||
        api->display.present == 0) {
        return 230;
    }

    CharmAppDisplayMode mode;
    if (api->display.describe(&mode) != CHARM_APP_STATUS_OK) {
        return 231;
    }
    if (mode.width != 16u ||
        mode.height != 16u ||
        mode.stride_bytes != 64u ||
        mode.format != CHARM_APP_PIXEL_FORMAT_ARGB8888) {
        return 232;
    }

    fill_frame(frame, 1u);
    if (api->display.present(frame, sizeof(frame)) != CHARM_APP_STATUS_OK) {
        return 233;
    }

    fill_frame(frame, 2u);
    if (api->display.present(frame, sizeof(frame)) != CHARM_APP_STATUS_OK) {
        return 234;
    }

    write_literal(api, "display_sequence_app: frames=2 checksum=3072\n");
    return 0;
}
