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
        return 240;
    }

    unsigned int checksum = 0;
    for (unsigned int i = 0; i < 4u; ++i) {
        CharmAppInputState state;
        if (api->input.poll(&state) != CHARM_APP_STATUS_OK) {
            return 241;
        }
        if (state.pointer_detected != 1u ||
            state.pointer_max_x != 15u ||
            state.pointer_max_y != 15u) {
            return 242;
        }
        checksum += (unsigned int)state.pointer_x +
                    (unsigned int)state.pointer_y +
                    (unsigned int)state.pointer_down +
                    (unsigned int)state.pointer_detected +
                    (unsigned int)(state.encoder1_delta + 8) +
                    (unsigned int)(state.encoder2_delta + 8);
    }

    if (checksum != 114u) {
        return 243;
    }

    write_literal(api, "input_sequence_app: polls=4 checksum=114\n");
    return 0;
}
