//
// Created by Joho on 2026/03/05.
//

module;
#include <cstddef>
#include <span>

export module canopen.pdo;

import util.core;
import canopen.types;

export namespace canopen {
    struct PdoConfig {
        CobId cob_id{0};
        util::u8 length{0};
    };

    class PdoProducer {
    public:
        PdoProducer() = default;
        PdoProducer(PdoConfig cfg, std::span<const std::byte> data) noexcept {
            reset(cfg, data);
        }

        void reset(PdoConfig cfg, std::span<const std::byte> data) noexcept {
            cfg_ = cfg;
            data_ = data;
        }

        [[nodiscard]] bool build(CanFrame& tx) const noexcept {
            if (cfg_.cob_id == 0) return false;
            if (cfg_.length == 0 || cfg_.length > 8) return false;
            if (data_.size() < cfg_.length) return false;

            tx.id = cfg_.cob_id;
            tx.dlc = cfg_.length;
            tx.data.fill(0);
            for (util::u8 i = 0; i < cfg_.length; ++i) {
                tx.data[i] = static_cast<util::u8>(data_[i]);
            }
            return true;
        }

    private:
        PdoConfig cfg_{};
        std::span<const std::byte> data_{};
    };

    class PdoConsumer {
    public:
        PdoConsumer() = default;
        PdoConsumer(PdoConfig cfg, std::span<std::byte> data) noexcept {
            reset(cfg, data);
        }

        void reset(PdoConfig cfg, std::span<std::byte> data) noexcept {
            cfg_ = cfg;
            data_ = data;
            last_len_ = 0;
        }

        [[nodiscard]] bool handle(const CanFrame& rx) noexcept {
            if (cfg_.cob_id == 0) return false;
            if (rx.id != cfg_.cob_id) return false;
            if (cfg_.length == 0 || cfg_.length > 8) return false;
            if (rx.dlc < cfg_.length) return false;
            if (data_.size() < cfg_.length) return false;

            for (util::u8 i = 0; i < cfg_.length; ++i) {
                data_[i] = std::byte{rx.data[i]};
            }
            last_len_ = cfg_.length;
            return true;
        }

        [[nodiscard]] util::u8 last_len() const noexcept { return last_len_; }

    private:
        PdoConfig cfg_{};
        std::span<std::byte> data_{};
        util::u8 last_len_{0};
    };
} // namespace canopen
