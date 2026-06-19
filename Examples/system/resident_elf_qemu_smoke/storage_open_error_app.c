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
        api->storage.open == 0 ||
        api->storage.read == 0 ||
        api->storage.close == 0) {
        return 280;
    }

    if (api->storage.open(0, 0, 0) != CHARM_APP_STATUS_INVALID_ARGUMENT) {
        return 281;
    }
    if (api->storage.open("/virtual/readme.txt", 1, 0) != CHARM_APP_STATUS_UNSUPPORTED) {
        return 282;
    }
    if (api->storage.open("/virtual/readme.txt", 0, 1) != CHARM_APP_STATUS_UNSUPPORTED) {
        return 283;
    }

    const int fd = api->storage.open("/virtual/readme.txt", 0, 0);
    if (fd != 3) {
        return 284;
    }
    unsigned char ch = 0;
    if (api->storage.read(fd, &ch, 1) != 1 || ch != 'C') {
        return 285;
    }
    if (api->storage.close(fd) != CHARM_APP_STATUS_OK) {
        return 286;
    }

    write_literal(api, "storage_open_error_app: open errors preserve fd\n");
    return 0;
}
