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

static int read_all(const CharmAppApi* api, int fd, unsigned int* total, unsigned int* checksum) {
    unsigned char buf[5];
    for (;;) {
        const int n = api->storage.read(fd, buf, sizeof(buf));
        if (n < 0 || n > (int)sizeof(buf)) {
            return 0;
        }
        if (n == 0) {
            return 1;
        }
        for (int i = 0; i < n; ++i) {
            *checksum += buf[i];
        }
        *total += (unsigned int)n;
        if (*total > 64u) {
            return 0;
        }
    }
}

int charm_app_main(const CharmAppApi* api, int argc, char** argv) {
    (void)argc;
    (void)argv;
    if (api == 0 ||
        api->console.write == 0 ||
        api->storage.open == 0 ||
        api->storage.read == 0 ||
        api->storage.close == 0) {
        return 250;
    }

    const int alpha = api->storage.open("/virtual/alpha.txt", 0, 0);
    const int beta = api->storage.open("/virtual/beta.bin", 0, 0);
    if (alpha != 3 || beta != 4) {
        return 251;
    }

    unsigned int total = 0;
    unsigned int checksum = 0;
    if (!read_all(api, alpha, &total, &checksum)) {
        return 252;
    }
    if (!read_all(api, beta, &total, &checksum)) {
        return 253;
    }
    if (api->storage.close(beta) != CHARM_APP_STATUS_OK) {
        return 254;
    }
    if (api->storage.close(alpha) != CHARM_APP_STATUS_OK) {
        return 255;
    }
    if (total != 31u) {
        return 256;
    }
    if (checksum != 2845u) {
        return 257;
    }

    write_literal(api, "storage_catalog_app: files=2 bytes=");
    write_u32(api, total);
    write_literal(api, " checksum=");
    write_u32(api, checksum);
    write_literal(api, "\n");
    return 0;
}
