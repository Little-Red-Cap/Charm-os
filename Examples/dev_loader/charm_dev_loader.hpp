#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

namespace charm::dev_loader {

inline constexpr std::uint32_t kImageMagic = 0x444C5643U; // "CVLD", little-endian.
inline constexpr std::uint32_t kImageVersion = 1U;
inline constexpr std::uint32_t kFlagSkipCrc = 1U << 0U;

enum class Stage : std::uint8_t {
    idle,
    receiving,
    complete,
    verified,
    launch_ready,
    failed,
};

enum class Code : std::uint8_t {
    ok,
    invalid_argument,
    no_session,
    already_active,
    out_of_order,
    overflow,
    write_failed,
    read_failed,
    incomplete,
    crc_mismatch,
};

struct ImageManifest {
    std::uint32_t magic{kImageMagic};
    std::uint32_t version{kImageVersion};
    std::uint32_t load_address{0};
    std::uint32_t entry_address{0};
    std::uint32_t size_bytes{0};
    std::uint32_t crc32{0};
    std::uint32_t flags{0};
};

struct Storage {
    void* ctx{nullptr};
    std::uint32_t base_address{0};
    std::uint32_t capacity_bytes{0};
    bool (*write)(void* ctx, std::uint32_t offset, std::span<const std::byte> bytes) noexcept{nullptr};
    bool (*read)(void* ctx, std::uint32_t offset, std::span<std::byte> bytes) noexcept{nullptr};
};

struct Result {
    Stage stage{Stage::idle};
    Code code{Code::ok};
    std::uint32_t received_bytes{0};
    std::uint32_t expected_crc32{0};
    std::uint32_t actual_crc32{0};
};

[[nodiscard]] constexpr std::string_view stage_name(Stage stage) noexcept {
    using namespace std::literals::string_view_literals;
    switch (stage) {
        case Stage::idle:
            return "idle"sv;
        case Stage::receiving:
            return "receiving"sv;
        case Stage::complete:
            return "complete"sv;
        case Stage::verified:
            return "verified"sv;
        case Stage::launch_ready:
            return "launch_ready"sv;
        case Stage::failed:
            return "failed"sv;
    }
    return "unknown"sv;
}

[[nodiscard]] constexpr std::string_view code_name(Code code) noexcept {
    using namespace std::literals::string_view_literals;
    switch (code) {
        case Code::ok:
            return "ok"sv;
        case Code::invalid_argument:
            return "invalid_argument"sv;
        case Code::no_session:
            return "no_session"sv;
        case Code::already_active:
            return "already_active"sv;
        case Code::out_of_order:
            return "out_of_order"sv;
        case Code::overflow:
            return "overflow"sv;
        case Code::write_failed:
            return "write_failed"sv;
        case Code::read_failed:
            return "read_failed"sv;
        case Code::incomplete:
            return "incomplete"sv;
        case Code::crc_mismatch:
            return "crc_mismatch"sv;
    }
    return "unknown"sv;
}

[[nodiscard]] constexpr bool manifest_valid(const ImageManifest& manifest) noexcept {
    return manifest.magic == kImageMagic &&
           manifest.version == kImageVersion &&
           manifest.load_address != 0U &&
           manifest.entry_address != 0U &&
           manifest.size_bytes != 0U;
}

[[nodiscard]] inline std::uint32_t crc32_update(std::uint32_t crc,
                                                std::span<const std::byte> bytes) noexcept {
    crc = ~crc;
    for (const std::byte byte : bytes) {
        crc ^= static_cast<std::uint8_t>(byte);
        for (int bit = 0; bit < 8; ++bit) {
            const std::uint32_t mask = 0U - (crc & 1U);
            crc = (crc >> 1U) ^ (0xEDB88320U & mask);
        }
    }
    return ~crc;
}

class Session {
public:
    [[nodiscard]] Result begin(const ImageManifest& manifest, Storage storage) noexcept {
        if (active_) {
            return fail(Code::already_active);
        }
        if (!manifest_valid(manifest) || storage.write == nullptr || storage.read == nullptr ||
            storage.capacity_bytes == 0U || manifest.size_bytes > storage.capacity_bytes ||
            manifest.load_address < storage.base_address ||
            manifest.size_bytes > (storage.capacity_bytes - (manifest.load_address - storage.base_address))) {
            return fail(Code::invalid_argument);
        }
        manifest_ = manifest;
        storage_ = storage;
        active_ = true;
        received_ = 0;
        crc_ = 0;
        stage_ = Stage::receiving;
        last_code_ = Code::ok;
        return current();
    }

    [[nodiscard]] Result write_chunk(std::uint32_t offset,
                                     std::span<const std::byte> bytes) noexcept {
        if (!active_) {
            return fail(Code::no_session);
        }
        if (stage_ != Stage::receiving || bytes.empty()) {
            return fail(Code::invalid_argument);
        }
        if (offset != received_) {
            return fail(Code::out_of_order);
        }
        if (offset > manifest_.size_bytes || bytes.size() > (manifest_.size_bytes - offset)) {
            return fail(Code::overflow);
        }
        const std::uint32_t storage_offset = (manifest_.load_address - storage_.base_address) + offset;
        if (!storage_.write(storage_.ctx, storage_offset, bytes)) {
            return fail(Code::write_failed);
        }
        crc_ = crc32_update(crc_, bytes);
        received_ += static_cast<std::uint32_t>(bytes.size());
        if (received_ == manifest_.size_bytes) {
            stage_ = Stage::complete;
        }
        last_code_ = Code::ok;
        return current();
    }

    [[nodiscard]] Result verify() noexcept {
        if (!active_) {
            return fail(Code::no_session);
        }
        if (received_ != manifest_.size_bytes) {
            return fail(Code::incomplete);
        }
        if ((manifest_.flags & kFlagSkipCrc) == 0U && crc_ != manifest_.crc32) {
            return fail(Code::crc_mismatch);
        }
        stage_ = Stage::verified;
        last_code_ = Code::ok;
        return current();
    }

    [[nodiscard]] Result mark_launch_ready() noexcept {
        if (!active_) {
            return fail(Code::no_session);
        }
        if (stage_ != Stage::verified) {
            return fail(Code::incomplete);
        }
        stage_ = Stage::launch_ready;
        last_code_ = Code::ok;
        return current();
    }

    void abort() noexcept {
        active_ = false;
        manifest_ = {};
        storage_ = {};
        received_ = 0;
        crc_ = 0;
        stage_ = Stage::idle;
        last_code_ = Code::ok;
    }

    [[nodiscard]] Result current() const noexcept {
        return Result{
            .stage = stage_,
            .code = last_code_,
            .received_bytes = received_,
            .expected_crc32 = manifest_.crc32,
            .actual_crc32 = crc_,
        };
    }

    [[nodiscard]] const ImageManifest& manifest() const noexcept {
        return manifest_;
    }

    [[nodiscard]] bool active() const noexcept {
        return active_;
    }

private:
    [[nodiscard]] Result fail(Code code) noexcept {
        last_code_ = code;
        if (code != Code::no_session && code != Code::already_active) {
            stage_ = Stage::failed;
        }
        return current();
    }

    ImageManifest manifest_{};
    Storage storage_{};
    bool active_{false};
    std::uint32_t received_{0};
    std::uint32_t crc_{0};
    Stage stage_{Stage::idle};
    Code last_code_{Code::ok};
};

class BinaryReceiveRuntime {
public:
    explicit constexpr BinaryReceiveRuntime(Storage storage) noexcept : storage_(storage) {}

    [[nodiscard]] Result begin(std::uint32_t size_bytes,
                               std::uint32_t crc32 = 0,
                               bool check_crc = false) noexcept {
        if (size_bytes == 0U || size_bytes > storage_.capacity_bytes || storage_.base_address == 0U) {
            return session_.begin({}, storage_);
        }
        session_.abort();
        cursor_ = 0;
        const ImageManifest manifest{
            .magic = kImageMagic,
            .version = kImageVersion,
            .load_address = storage_.base_address,
            .entry_address = storage_.base_address | 1U,
            .size_bytes = size_bytes,
            .crc32 = crc32,
            .flags = check_crc ? 0U : kFlagSkipCrc,
        };
        return session_.begin(manifest, storage_);
    }

    [[nodiscard]] Result write(std::span<const std::byte> bytes) noexcept {
        const auto result = session_.write_chunk(cursor_, bytes);
        if (result.code == Code::ok) {
            cursor_ += static_cast<std::uint32_t>(bytes.size());
        }
        return result;
    }

    [[nodiscard]] Result write_at(std::uint32_t offset, std::span<const std::byte> bytes) noexcept {
        const auto result = session_.write_chunk(offset, bytes);
        if (result.code == Code::ok && offset == cursor_) {
            cursor_ += static_cast<std::uint32_t>(bytes.size());
        }
        return result;
    }

    [[nodiscard]] Result verify() noexcept {
        return session_.verify();
    }

    [[nodiscard]] Result mark_launch_ready() noexcept {
        return session_.mark_launch_ready();
    }

    void abort() noexcept {
        session_.abort();
        cursor_ = 0;
    }

    [[nodiscard]] Result current() const noexcept {
        return session_.current();
    }

    [[nodiscard]] const ImageManifest& manifest() const noexcept {
        return session_.manifest();
    }

    [[nodiscard]] bool active() const noexcept {
        return session_.active();
    }

    [[nodiscard]] std::uint32_t cursor() const noexcept {
        return cursor_;
    }

private:
    Storage storage_{};
    Session session_{};
    std::uint32_t cursor_{0};
};

} // namespace charm::dev_loader
