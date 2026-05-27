#pragma once

#include "charm_app_api.h"

static int charm_player_min_write(const CharmAppApi* api, const char* text) {
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

static void charm_player_min_fill_argb8888(unsigned char* frame, uint32_t tick) {
    const uint32_t width = 16u;
    const uint32_t height = 16u;
    const uint32_t stride = width * 4u;
    for (uint32_t y = 0; y < height; ++y) {
        uint32_t* row = (uint32_t*)(void*)(frame + (y * stride));
        for (uint32_t x = 0; x < width; ++x) {
            const uint8_t r = (uint8_t)((x + tick) & 0xffu);
            const uint8_t g = (uint8_t)((y + (tick >> 1)) & 0xffu);
            const uint8_t b = (uint8_t)((x ^ y ^ tick) & 0xffu);
            row[x] = 0xff000000u | ((uint32_t)r << 16u) | ((uint32_t)g << 8u) | b;
        }
    }
}

static int charm_player_min_run(const CharmAppApi* api, int argc, char** argv) {
    static unsigned char frame[16u * 16u * 4u];
    (void)argc;
    (void)argv;
    if (api == 0 || api->magic != CHARM_APP_API_MAGIC ||
        api->version != CHARM_APP_API_VERSION ||
        api->size < sizeof(CharmAppApi)) {
        return 101;
    }
    if (api->display.describe == 0 || api->display.present == 0 ||
        api->input.poll == 0 || api->time.now_ms == 0) {
        return 102;
    }

    CharmAppDisplayMode mode;
    if (api->display.describe(&mode) != CHARM_APP_STATUS_OK) {
        return 103;
    }
    if (mode.format != CHARM_APP_PIXEL_FORMAT_ARGB8888 ||
        mode.width == 0u || mode.height == 0u ||
        mode.stride_bytes < (mode.width * 4u)) {
        return 104;
    }

    CharmAppInputState input;
    if (api->input.poll(&input) != CHARM_APP_STATUS_OK) {
        return 105;
    }

    charm_player_min_fill_argb8888(frame, api->time.now_ms());
    if (api->display.present(frame, sizeof(frame)) != CHARM_APP_STATUS_OK) {
        return 106;
    }

    charm_player_min_write(api, "player_min: presented one frame\n");
    return 0;
}
