#pragma once

#include <cstdio>

namespace examples::usb::support {
    [[nodiscard]] inline bool expect(bool cond, const char* message) noexcept {
        if (!cond) {
            std::fprintf(stderr, "[ERR] %s\n", message);
            return false;
        }
        return true;
    }

    template <typename StatusT, typename ErrT>
    [[nodiscard]] bool expect_status(const StatusT& status,
                                     ErrT want,
                                     const char* message) noexcept(noexcept(status.err == want)) {
        const auto got = status.err;
        if (got != want) {
            std::fprintf(stderr,
                         "[ERR] %s err=%d want=%d\n",
                         message,
                         static_cast<int>(got),
                         static_cast<int>(want));
            return false;
        }
        return true;
    }

    template <typename ResultT, typename ErrT>
    [[nodiscard]] bool expect_error(const ResultT& result,
                                    ErrT want,
                                    const char* message) noexcept(noexcept(result.error() == want)) {
        const auto got = result.error();
        if (got != want) {
            std::fprintf(stderr,
                         "[ERR] %s err=%d want=%d\n",
                         message,
                         static_cast<int>(got),
                         static_cast<int>(want));
            return false;
        }
        return true;
    }
}
