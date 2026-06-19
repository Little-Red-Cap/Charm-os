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
    if (api == 0 || api->console.write == 0 || api->input.poll == 0) {
        return 250;
    }

    const int code = api->input.poll(0);
    if (code != CHARM_APP_STATUS_INVALID_ARGUMENT) {
        return 251;
    }

    write_literal(api, "input_error_app: null poll rejected\n");
    return 0;
}
