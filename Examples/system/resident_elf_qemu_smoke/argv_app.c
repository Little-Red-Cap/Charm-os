#include "charm_app_api.h"

static int text_eq(const char* lhs, const char* rhs) {
    if (lhs == 0 || rhs == 0) {
        return 0;
    }
    while (*lhs != '\0' && *rhs != '\0') {
        if (*lhs++ != *rhs++) {
            return 0;
        }
    }
    return *lhs == '\0' && *rhs == '\0';
}

static unsigned int text_checksum(const char* text) {
    unsigned int checksum = 0;
    if (text == 0) {
        return checksum;
    }
    while (*text != '\0') {
        checksum += (unsigned char)*text++;
    }
    return checksum;
}

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
    if (api == 0 || api->console.write == 0) {
        return 230;
    }
    if (argc != 4 || argv == 0) {
        return 231;
    }
    if (!text_eq(argv[0], "argv_app") ||
        !text_eq(argv[1], "one") ||
        !text_eq(argv[2], "two") ||
        !text_eq(argv[3], "three") ||
        argv[4] != 0) {
        return 232;
    }

    const unsigned int checksum =
        text_checksum(argv[0]) +
        text_checksum(argv[1]) +
        text_checksum(argv[2]) +
        text_checksum(argv[3]);
    if (checksum != 2052u) {
        return 233;
    }

    write_literal(api, "argv_app: argc=");
    write_u32(api, (unsigned int)argc);
    write_literal(api, " checksum=");
    write_u32(api, checksum);
    write_literal(api, "\n");
    return 0;
}
