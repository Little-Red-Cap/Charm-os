#include "charm_app_api.h"

static int write_literal(const CharmAppApi* api, const char* text) {
    unsigned int len = 0;
    while (text[len] != '\0') {
        ++len;
    }
    return api->console.write(text, len);
}

static int write_u32(const CharmAppApi* api, unsigned int value) {
    char buf[11];
    unsigned int cursor = sizeof(buf);
    buf[--cursor] = '\0';
    do {
        buf[--cursor] = (char)('0' + (value % 10u));
        value /= 10u;
    } while (value != 0u);
    return api->console.write(&buf[cursor], sizeof(buf) - cursor - 1u);
}

int charm_app_main(const CharmAppApi* api, int argc, char** argv) {
    (void)argc;
    (void)argv;
    if (api == 0 ||
        api->console.write == 0 ||
        api->storage.open == 0 ||
        api->storage.read == 0 ||
        api->storage.close == 0) {
        return 220;
    }

    const int fd = api->storage.open("/virtual/readme.txt", 0, 0);
    if (fd != 3) {
        return 221;
    }

    unsigned char buf[8];
    unsigned int total = 0;
    unsigned int checksum = 0;
    for (;;) {
        const int n = api->storage.read(fd, buf, sizeof(buf));
        if (n < 0 || n > (int)sizeof(buf)) {
            return 222;
        }
        if (n == 0) {
            break;
        }
        for (int i = 0; i < n; ++i) {
            checksum += buf[i];
        }
        total += (unsigned int)n;
        if (total > 64u) {
            return 223;
        }
    }

    if (api->storage.close(fd) != CHARM_APP_STATUS_OK) {
        return 224;
    }
    if (total != 27u) {
        return 225;
    }
    if (checksum != 2441u) {
        return 226;
    }

    write_literal(api, "storage_app: bytes=");
    write_u32(api, total);
    write_literal(api, " checksum=");
    write_u32(api, checksum);
    write_literal(api, "\n");
    return 0;
}
