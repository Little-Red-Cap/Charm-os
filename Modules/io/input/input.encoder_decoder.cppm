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
        // 常见编码器：1 detent = 4 个“状态步进”
        std::int8_t steps_per_detent{4};
    };

    class EncoderDecoder {
    public:
        explicit EncoderDecoder(EncoderCfg cfg = {}) noexcept : cfg_(cfg) { reset(0); }
        // 初始状态（unknown 可设为 0xFF）
        void reset(std::uint8_t ab = 0xFF) noexcept {
            prev_ = ab;
            acc_ = 0;
        }

        // ab: bit1=A, bit0=B, 取值 0..3；0xFF 表示“无输入/未提供”
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

            // 可选：累计到每“一个 detent”再输出
            // 不同编码器 detent 可能是 2 或 4 个step
            acc_ += step;

            // 默认：每 4 步输出一次（最常见）
            // 如果你编码器每 detent = 2 步，把阈值改成 2
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
