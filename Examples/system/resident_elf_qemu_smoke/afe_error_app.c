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
    if (api == 0 ||
        api->console.write == 0 ||
        api->afe.configure == 0 ||
        api->afe.read == 0) {
        return 252;
    }

    unsigned int samples[4];
    for (unsigned int i = 0; i < 4u; ++i) {
        samples[i] = 0u;
    }
    if (api->afe.configure(3u, 48000u) != CHARM_APP_STATUS_UNSUPPORTED) {
        return 253;
    }
    if (api->afe.read(samples, sizeof(samples)) != CHARM_APP_STATUS_UNSUPPORTED) {
        return 254;
    }
    for (unsigned int i = 0; i < 4u; ++i) {
        if (samples[i] != 0u) {
            return 255;
        }
    }

    write_literal(api, "afe_error_app: unsupported afe preserved buffer\n");
    return 0;
}
