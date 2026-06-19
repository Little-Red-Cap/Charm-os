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
        return 260;
    }

    const int fd = api->storage.open("/virtual/readme.txt", 0, 0);
    if (fd != 3) {
        return 261;
    }

    const int rejected = api->storage.read(fd, 0, 4);
    if (rejected != CHARM_APP_STATUS_INVALID_ARGUMENT) {
        return 262;
    }

    unsigned char buf[4];
    const int n = api->storage.read(fd, buf, sizeof(buf));
    if (n != (int)sizeof(buf)) {
        return 263;
    }
    if (buf[0] != 'C' || buf[1] != 'h' || buf[2] != 'a' || buf[3] != 'r') {
        return 264;
    }

    if (api->storage.close(fd) != CHARM_APP_STATUS_OK) {
        return 265;
    }

    write_literal(api, "storage_error_app: invalid read preserved cursor\n");
    return 0;
}
