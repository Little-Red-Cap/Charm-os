//
// Created by Joho on 2025/12/30.
//

module;
#include <cstdint>
#include <optional>

export module input.encoder_decoder;

export namespace input {

    struct EncoderDelta { std::int16_t delta{0}; };

    struct EncoderCfg {
        // Typical encoder: 1 detent = 4 state steps.
        std::int8_t steps_per_detent{4};
    };

    class EncoderDecoder {
    public:
        explicit EncoderDecoder(EncoderCfg cfg = {}) noexcept : cfg_(cfg) { reset(0); }
        // Initial state (unknown may be 0xFF).
        void reset(std::uint8_t ab = 0xFF) noexcept {
            prev_ = ab;
            acc_ = 0;
        }

        // ab: bit1=A, bit0=B, range 0..3; 0xFF means unknown.
        std::optional<EncoderDelta> update_old(std::uint8_t ab) noexcept {
            if (ab == 0xFF) return std::nullopt;
            ab &= 0x03;

            if (prev_ == 0xFF) { prev_ = ab; return std::nullopt; }

            static constexpr std::int8_t t[4][4] = {
                {  0, +1, -1,  0 }, // 00->(00,01,10,11)
                { -1,  0,  0, +1 }, // 01->...
                { +1,  0,  0, -1 }, // 10->...
                {  0, -1, +1,  0 }, // 11->...
            };

            const std::int8_t step = t[prev_][ab];
            prev_ = ab;

            if (step == 0) return std::nullopt;

            // Optional: emit only after a full detent.
            // Some encoders use 2 or 4 steps per detent.
            acc_ += step;

            // Default: emit once per 4 steps (most common).
            // If your detent is 2 steps, set threshold to 2.
            const std::int8_t spd = (cfg_.steps_per_detent <= 0) ? 4 : cfg_.steps_per_detent;

            if (acc_ >= spd)  { acc_ = 0; return EncoderDelta{+1}; }
            if (acc_ <= -spd) { acc_ = 0; return EncoderDelta{-1}; }
            return std::nullopt;
        }
        std::optional<EncoderDelta> update(std::uint8_t ab) noexcept {
            if (ab == 0xFF) {
                return std::nullopt;
            }
            ab &= 0x03;

            if (prev_ == 0xFF) {
                prev_ = ab;
                return std::nullopt;
            }

            static constexpr std::int8_t t[4][4] = {
                {  0, +1, -1,  0 },
                { -1,  0,  0, +1 },
                { +1,  0,  0, -1 },
                {  0, -1, +1,  0 },
            };

            const std::int8_t step = t[prev_][ab];

            prev_ = ab;

            if (step == 0) {
                return std::nullopt;
            }

            acc_ += step;

            const std::int8_t spd = (cfg_.steps_per_detent <= 0) ? 4 : cfg_.steps_per_detent;

            if (acc_ >= spd) {
                acc_ = 0;
                return EncoderDelta{+1};
            }
            if (acc_ <= -spd) {
                acc_ = 0;
                return EncoderDelta{-1};
            }

            return std::nullopt;
        }

    private:
        EncoderCfg cfg_{};
        std::uint8_t prev_{0xFF};
        std::int8_t acc_{0};
    };

} // namespace input
