module;

#include <array>
#include <type_traits>

export module service.signal;

import util.core;
import util.delegate;
import util.error;

export namespace service {
    template <class Signature, util::usize MaxSlots>
    class signal;

    template <class... Args, util::usize MaxSlots>
    class signal<void(Args...), MaxSlots> {
    public:
        static_assert(MaxSlots >= 1);
        static_assert(MaxSlots <= static_cast<util::usize>(0x10000u));
        static_assert((!std::is_rvalue_reference_v<Args> && ...),
                      "service::signal does not support rvalue-reference payloads");

        using slot_type = util::delegate<Args...>;

        struct connection {
            util::u16 slot{};
            util::u16 generation{};

            constexpr explicit operator bool() const noexcept {
                return generation != 0;
            }
        };

        [[nodiscard]] constexpr util::Result<connection> connect(slot_type slot) noexcept {
            if (!slot) {
                return util::unexpected(util::Errc::invalid_arg);
            }
            for (util::usize i = 0; i < MaxSlots; ++i) {
                auto& rec = slots_[i];
                if (rec.occupied) {
                    continue;
                }
                rec.occupied = true;
                rec.slot = slot;
                rec.generation = next_generation(rec.generation);
                ++size_;
                return connection{
                    static_cast<util::u16>(i),
                    rec.generation,
                };
            }
            return util::unexpected(util::Errc::buffer_overflow);
        }

        [[nodiscard]] constexpr bool disconnect(connection c) noexcept {
            if (!c) {
                return false;
            }
            const auto index = static_cast<util::usize>(c.slot);
            if (index >= MaxSlots) {
                return false;
            }
            auto& rec = slots_[index];
            if (!rec.occupied || rec.generation != c.generation) {
                return false;
            }
            rec.slot = {};
            rec.occupied = false;
            if (size_ > 0) {
                --size_;
            }
            return true;
        }

        [[nodiscard]] constexpr util::usize emit(Args... args) const noexcept {
            util::usize dispatched = 0;
            for (const auto& rec : slots_) {
                if (!rec.occupied || !rec.slot) {
                    continue;
                }
                rec.slot(args...);
                ++dispatched;
            }
            return dispatched;
        }

        constexpr void clear() noexcept {
            for (auto& rec : slots_) {
                rec.slot = {};
                rec.occupied = false;
            }
            size_ = 0;
        }

        [[nodiscard]] constexpr util::usize size() const noexcept {
            return size_;
        }

        [[nodiscard]] static consteval util::usize capacity() noexcept {
            return MaxSlots;
        }

        [[nodiscard]] constexpr bool empty() const noexcept {
            return size_ == 0;
        }

    private:
        struct slot_record {
            slot_type slot{};
            util::u16 generation{};
            bool occupied{false};
        };

        [[nodiscard]] static constexpr util::u16 next_generation(util::u16 generation) noexcept {
            ++generation;
            if (generation == 0) {
                ++generation;
            }
            return generation;
        }

        std::array<slot_record, MaxSlots> slots_{};
        util::usize size_{0};
    };
}
