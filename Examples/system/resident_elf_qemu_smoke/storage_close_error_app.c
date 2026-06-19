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
        return 270;
    }

    const int fd = api->storage.open("/virtual/readme.txt", 0, 0);
    if (fd != 3) {
        return 271;
    }
    if (api->storage.close(fd) != CHARM_APP_STATUS_OK) {
        return 272;
    }
    if (api->storage.close(fd) != CHARM_APP_STATUS_UNSUPPORTED) {
        return 273;
    }

    const int fd2 = api->storage.open("/virtual/readme.txt", 0, 0);
    if (fd2 != 3) {
        return 274;
    }
    unsigned char ch = 0;
    if (api->storage.read(fd2, &ch, 1) != 1 || ch != 'C') {
        return 275;
    }
    if (api->storage.close(fd2) != CHARM_APP_STATUS_OK) {
        return 276;
    }

    write_literal(api, "storage_close_error_app: close reuse ok\n");
    return 0;
}
