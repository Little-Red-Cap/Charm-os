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
        return 300;
    }

    const int fd0 = api->storage.open("/virtual/readme.txt", 0, 0);
    const int fd1 = api->storage.open("/virtual/readme.txt", 0, 0);
    const int fd2 = api->storage.open("/virtual/alpha.txt", 0, 0);
    const int fd3 = api->storage.open("/virtual/beta.bin", 0, 0);
    if (fd0 != 3 || fd1 != 4 || fd2 != 5 || fd3 != 6) {
        return 301;
    }
    if (api->storage.open("/virtual/readme.txt", 0, 0) != CHARM_APP_STATUS_IO_ERROR) {
        return 302;
    }
    if (api->storage.close(fd1) != CHARM_APP_STATUS_OK) {
        return 303;
    }

    const int fd4 = api->storage.open("/virtual/readme.txt", 0, 0);
    if (fd4 != 4) {
        return 304;
    }
    unsigned char ch = 0;
    if (api->storage.read(fd4, &ch, 1) != 1 || ch != 'C') {
        return 305;
    }

    if (api->storage.close(fd4) != CHARM_APP_STATUS_OK ||
        api->storage.close(fd3) != CHARM_APP_STATUS_OK ||
        api->storage.close(fd2) != CHARM_APP_STATUS_OK ||
        api->storage.close(fd0) != CHARM_APP_STATUS_OK) {
        return 306;
    }

    write_literal(api, "storage_fd_exhaustion_app: fd slots exhausted and reused\n");
    return 0;
}
