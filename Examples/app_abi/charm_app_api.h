#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CHARM_APP_API_MAGIC 0x43484150u
#define CHARM_APP_API_VERSION 1u

typedef enum CharmAppStatus {
    CHARM_APP_STATUS_OK = 0,
    CHARM_APP_STATUS_UNSUPPORTED = 1,
    CHARM_APP_STATUS_INVALID_ARGUMENT = 2,
    CHARM_APP_STATUS_IO_ERROR = 3,
} CharmAppStatus;

typedef enum CharmAppPixelFormat {
    CHARM_APP_PIXEL_FORMAT_UNKNOWN = 0,
    CHARM_APP_PIXEL_FORMAT_ARGB8888 = 1,
} CharmAppPixelFormat;

typedef struct CharmAppDisplayMode {
    uint32_t width;
    uint32_t height;
    uint32_t stride_bytes;
    uint32_t format;
} CharmAppDisplayMode;

typedef struct CharmAppInputState {
    int16_t encoder1_delta;
    int16_t encoder2_delta;
    uint8_t encoder1_pressed;
    uint8_t encoder2_pressed;
    uint8_t pointer_detected;
    uint8_t pointer_down;
    uint16_t pointer_x;
    uint16_t pointer_y;
    uint16_t pointer_max_x;
    uint16_t pointer_max_y;
} CharmAppInputState;

typedef struct CharmAppConsoleApi {
    int (*write)(const char* text, size_t len);
} CharmAppConsoleApi;

typedef struct CharmAppTimeApi {
    uint32_t (*now_ms)(void);
} CharmAppTimeApi;

typedef struct CharmAppDisplayApi {
    int (*describe)(CharmAppDisplayMode* out_mode);
    int (*present)(const void* pixels, uint32_t bytes);
} CharmAppDisplayApi;

typedef struct CharmAppInputApi {
    int (*poll)(CharmAppInputState* out_state);
} CharmAppInputApi;

typedef struct CharmAppStorageApi {
    int (*open)(const char* path, int flags, int mode);
    int (*read)(int fd, void* buf, size_t len);
    int (*write)(int fd, const void* buf, size_t len);
    int (*close)(int fd);
} CharmAppStorageApi;

typedef struct CharmAppAfeApi {
    int (*configure)(uint32_t channel_mask, uint32_t sample_rate_hz);
    int (*read)(void* samples, size_t sample_bytes);
} CharmAppAfeApi;

typedef struct CharmAppControlApi {
    void (*exit)(int code);
} CharmAppControlApi;

typedef struct CharmAppApi {
    uint32_t magic;
    uint32_t version;
    uint32_t size;
    uint32_t flags;
    CharmAppConsoleApi console;
    CharmAppTimeApi time;
    CharmAppDisplayApi display;
    CharmAppInputApi input;
    CharmAppStorageApi storage;
    CharmAppAfeApi afe;
    CharmAppControlApi app;
} CharmAppApi;

typedef int (*CharmAppMainFn)(const CharmAppApi* api, int argc, char** argv);

int charm_app_main(const CharmAppApi* api, int argc, char** argv);

#ifdef __cplusplus
}
#endif
