#pragma once

#include <cstddef>
#include <cstdint>

namespace charm::cap {

enum class StatusCode : std::int8_t {
    ok = 0,
    busy,
    timeout,
    unsupported,
    invalid_argument,
    io_error,
    hardware_error,
};

[[nodiscard]] constexpr const char* status_code_name(const StatusCode code) noexcept {
    switch (code) {
    case StatusCode::ok: return "ok";
    case StatusCode::busy: return "busy";
    case StatusCode::timeout: return "timeout";
    case StatusCode::unsupported: return "unsupported";
    case StatusCode::invalid_argument: return "invalid_argument";
    case StatusCode::io_error: return "io_error";
    case StatusCode::hardware_error: return "hardware_error";
    }
    return "unknown";
}

struct Status {
    StatusCode code{StatusCode::ok};

    [[nodiscard]] constexpr bool is_ok() const noexcept {
        return code == StatusCode::ok;
    }

    [[nodiscard]] constexpr explicit operator bool() const noexcept {
        return is_ok();
    }

    [[nodiscard]] static consteval Status ok() noexcept {
        return {};
    }

    [[nodiscard]] static constexpr Status from(const StatusCode value) noexcept {
        return {value};
    }

    [[nodiscard]] constexpr const char* name() const noexcept {
        return status_code_name(code);
    }
};

struct Transfer {
    Status status{};
    std::size_t bytes{0U};

    [[nodiscard]] constexpr bool is_ok() const noexcept {
        return status.is_ok();
    }

    [[nodiscard]] constexpr explicit operator bool() const noexcept {
        return is_ok();
    }
};

[[nodiscard]] constexpr Transfer transferred(const std::size_t bytes) noexcept {
    return Transfer{Status::ok(), bytes};
}

[[nodiscard]] constexpr Transfer transfer_error(const StatusCode code) noexcept {
    return Transfer{Status::from(code), 0U};
}

} // namespace charm::cap
