module;

#include <array>
#include <cstddef>
#include <cstdint>

export module example.pc.board_io;

namespace example::pc::detail {
    struct KeyState {
        bool key0{false};
        bool wkup2{false};
        bool enc_key{false};
        int enc_steps{0};
    };

    inline KeyState& keys() noexcept {
        static KeyState state{};
        return state;
    }

    constexpr std::size_t kLedCount = 4;

    inline std::array<bool, kLedCount>& leds() noexcept {
        static std::array<bool, kLedCount> state{};
        return state;
    }
}

export namespace example::pc {
    enum class KeyId : std::uint8_t {
        key0,
        wkup2,
        enc_key,
    };

    enum class LedId : std::uint8_t {
        led0,
        led1,
        led2,
        led3,
    };

    struct KeySnapshot {
        bool key0{false};
        bool wkup2{false};
        bool enc_key{false};
        int enc_steps{0};
    };

    inline void set_key(KeyId id, bool down) noexcept {
        auto& s = detail::keys();
        switch (id) {
            case KeyId::key0: s.key0 = down; break;
            case KeyId::wkup2: s.wkup2 = down; break;
            case KeyId::enc_key: s.enc_key = down; break;
        }
    }

    inline bool get_key(KeyId id) noexcept {
        const auto& s = detail::keys();
        switch (id) {
            case KeyId::key0: return s.key0;
            case KeyId::wkup2: return s.wkup2;
            case KeyId::enc_key: return s.enc_key;
        }
        return false;
    }

    inline void add_encoder_steps(int delta) noexcept {
        detail::keys().enc_steps += delta;
    }

    inline KeySnapshot poll_keys() noexcept {
        auto& s = detail::keys();
        KeySnapshot snap{s.key0, s.wkup2, s.enc_key, s.enc_steps};
        s.enc_steps = 0;
        return snap;
    }

    inline void set_led(LedId id, bool on) noexcept {
        const auto idx = static_cast<std::size_t>(id);
        if (idx >= detail::kLedCount) return;
        detail::leds()[idx] = on;
    }

    inline bool get_led(LedId id) noexcept {
        const auto idx = static_cast<std::size_t>(id);
        if (idx >= detail::kLedCount) return false;
        return detail::leds()[idx];
    }

    inline void clear_leds() noexcept {
        for (auto& v : detail::leds()) v = false;
    }
}
