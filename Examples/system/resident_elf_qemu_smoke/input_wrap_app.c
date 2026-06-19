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
        return 244;
    }

    static const int expected_encoder1[6] = {1, 0, -1, 0, 1, 0};
    static const unsigned int expected_x[6] = {3u, 4u, 5u, 6u, 3u, 4u};
    static const unsigned int expected_y[6] = {5u, 6u, 7u, 8u, 5u, 6u};
    static const unsigned int expected_down[6] = {0u, 1u, 1u, 0u, 0u, 1u};

    unsigned int checksum = 0;
    for (unsigned int i = 0; i < 6u; ++i) {
        CharmAppInputState state;
        if (api->input.poll(&state) != CHARM_APP_STATUS_OK) {
            return 245;
        }
        if (state.encoder1_delta != expected_encoder1[i] ||
            state.pointer_x != expected_x[i] ||
            state.pointer_y != expected_y[i] ||
            state.pointer_down != expected_down[i] ||
            state.pointer_detected != 1u ||
            state.pointer_max_x != 15u ||
            state.pointer_max_y != 15u) {
            return 246;
        }
        checksum += (unsigned int)state.pointer_x +
                    (unsigned int)state.pointer_y +
                    (unsigned int)state.pointer_down +
                    (unsigned int)state.pointer_detected +
                    (unsigned int)(state.encoder1_delta + 8) +
                    (unsigned int)(state.encoder2_delta + 8);
    }

    if (checksum != 169u) {
        return 247;
    }

    write_literal(api, "input_wrap_app: polls=6 checksum=169\n");
    return 0;
}
