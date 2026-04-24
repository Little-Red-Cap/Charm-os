module;

export module block.device;

import fs_block;
import fs_errno;
import fs_stream;
import util.core;

export namespace block {
    using Device = fs::BlockDevice;
    using Status = fs::Status;
    using Errc = fs::Errc;

    enum class Caps : util::u32 {
        none = 0,
        read = 1u << 0,
        write = 1u << 1,
        erase = 1u << 2,
        flush = 1u << 3,
        cached = 1u << 4,
    };

    constexpr Caps operator|(Caps lhs, Caps rhs) noexcept {
        return static_cast<Caps>(
            static_cast<util::u32>(lhs) | static_cast<util::u32>(rhs));
    }

    constexpr Caps operator&(Caps lhs, Caps rhs) noexcept {
        return static_cast<Caps>(
            static_cast<util::u32>(lhs) & static_cast<util::u32>(rhs));
    }

    constexpr util::u32 to_bits(Caps caps) noexcept {
        return static_cast<util::u32>(caps);
    }

    inline util::u32 caps_from_ops(const Device& dev) noexcept {
        util::u32 caps = 0;
        if (dev.read) caps |= to_bits(Caps::read);
        if (dev.write) caps |= to_bits(Caps::write);
        if (dev.erase) caps |= to_bits(Caps::erase);
        if (dev.flush) caps |= to_bits(Caps::flush);
        return caps;
    }

    inline bool has_caps(const Device& dev, Caps caps) noexcept {
        return (dev.caps & to_bits(caps)) == to_bits(caps);
    }

    inline bool is_read_only(const Device& dev) noexcept {
        return (dev.caps & to_bits(Caps::write)) == 0;
    }
}
