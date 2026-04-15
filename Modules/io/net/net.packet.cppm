module;

#include <array>

export module net.packet;

export import net.common;
import util.core;
import util.error;
import util.expected;

export namespace net {
    struct PacketView {
        ByteView payload{};
        util::usize headroom{0};
        util::usize tailroom{0};

        [[nodiscard]] constexpr const util::u8* data() const noexcept {
            return payload.data();
        }

        [[nodiscard]] constexpr util::usize size() const noexcept {
            return payload.size();
        }

        [[nodiscard]] constexpr bool empty() const noexcept {
            return payload.empty();
        }

        [[nodiscard]] constexpr const util::u8& operator[](util::usize index) const noexcept {
            return payload[index];
        }

        [[nodiscard]] constexpr PacketView subspan(
            util::usize offset,
            util::usize count = static_cast<util::usize>(-1)) const noexcept {
            const auto sub = payload.subspan(offset, count);
            const auto consumed = offset > payload.size() ? payload.size() : offset;
            return PacketView{
                sub,
                headroom + consumed,
                tailroom + (payload.size() - consumed - sub.size())
            };
        }
    };

    struct MutPacketView {
        MutByteView payload{};
        util::usize headroom{0};
        util::usize tailroom{0};

        [[nodiscard]] constexpr util::u8* data() const noexcept {
            return payload.data();
        }

        [[nodiscard]] constexpr util::usize size() const noexcept {
            return payload.size();
        }

        [[nodiscard]] constexpr bool empty() const noexcept {
            return payload.empty();
        }

        [[nodiscard]] constexpr util::u8& operator[](util::usize index) const noexcept {
            return payload[index];
        }

        [[nodiscard]] constexpr MutPacketView subspan(
            util::usize offset,
            util::usize count = static_cast<util::usize>(-1)) const noexcept {
            const auto sub = payload.subspan(offset, count);
            const auto consumed = offset > payload.size() ? payload.size() : offset;
            return MutPacketView{
                sub,
                headroom + consumed,
                tailroom + (payload.size() - consumed - sub.size())
            };
        }

        [[nodiscard]] constexpr operator PacketView() const noexcept {
            return PacketView{payload, headroom, tailroom};
        }
    };

    template <util::usize Capacity>
    class PacketBuffer {
    public:
        PacketBuffer() noexcept = default;

        [[nodiscard]] constexpr util::usize capacity() const noexcept {
            return Capacity;
        }

        [[nodiscard]] constexpr util::usize size() const noexcept {
            return size_;
        }

        [[nodiscard]] constexpr bool empty() const noexcept {
            return size_ == 0;
        }

        [[nodiscard]] constexpr util::usize headroom() const noexcept {
            return offset_;
        }

        [[nodiscard]] constexpr util::usize tailroom() const noexcept {
            return Capacity - offset_ - size_;
        }

        [[nodiscard]] constexpr const util::u8* data() const noexcept {
            return storage_.data() + offset_;
        }

        [[nodiscard]] constexpr util::u8* data() noexcept {
            return storage_.data() + offset_;
        }

        [[nodiscard]] constexpr PacketView view() const noexcept {
            return PacketView{
                ByteView{data(), size_},
                headroom(),
                tailroom()
            };
        }

        [[nodiscard]] constexpr MutPacketView mut_view() noexcept {
            return MutPacketView{
                MutByteView{data(), size_},
                headroom(),
                tailroom()
            };
        }

        [[nodiscard]] constexpr Result<void> reset(util::usize headroom = 0) noexcept {
            if (headroom > Capacity) {
                return util::unexpected(errc::invalid_arg);
            }
            offset_ = headroom;
            size_ = 0;
            return {};
        }

        constexpr void clear() noexcept {
            offset_ = 0;
            size_ = 0;
        }

        [[nodiscard]] constexpr Result<void> resize(util::usize size) noexcept {
            if (size > Capacity - offset_) {
                return util::unexpected(errc::buffer_overflow);
            }
            size_ = size;
            return {};
        }

        [[nodiscard]] Result<void> append(ByteView bytes) noexcept {
            if (bytes.size() > tailroom()) {
                return util::unexpected(errc::buffer_overflow);
            }

            auto* dst = storage_.data() + offset_ + size_;
            for (util::usize i = 0; i < bytes.size(); ++i) {
                dst[i] = bytes[i];
            }
            size_ += bytes.size();
            return {};
        }

        [[nodiscard]] Result<void> prepend(ByteView bytes) noexcept {
            if (bytes.size() > headroom()) {
                return util::unexpected(errc::buffer_overflow);
            }

            offset_ -= bytes.size();
            auto* dst = storage_.data() + offset_;
            for (util::usize i = 0; i < bytes.size(); ++i) {
                dst[i] = bytes[i];
            }
            size_ += bytes.size();
            return {};
        }

        [[nodiscard]] constexpr Result<void> trim_front(util::usize count) noexcept {
            if (count > size_) {
                return util::unexpected(errc::invalid_arg);
            }
            offset_ += count;
            size_ -= count;
            return {};
        }

        [[nodiscard]] constexpr Result<void> trim_back(util::usize count) noexcept {
            if (count > size_) {
                return util::unexpected(errc::invalid_arg);
            }
            size_ -= count;
            return {};
        }

    private:
        std::array<util::u8, Capacity> storage_{};
        util::usize offset_{0};
        util::usize size_{0};
    };

    template <util::usize Count, util::usize Capacity>
    class PacketPool;

    template <util::usize Count, util::usize Capacity>
    class PacketLease {
    public:
        PacketLease() noexcept = default;
        PacketLease(const PacketLease&) = delete;
        PacketLease& operator=(const PacketLease&) = delete;

        PacketLease(PacketLease&& other) noexcept
            : pool_(other.pool_),
              index_(other.index_) {
            other.pool_ = nullptr;
            other.index_ = invalid_index();
        }

        PacketLease& operator=(PacketLease&& other) noexcept {
            if (this == &other) {
                return *this;
            }
            release();
            pool_ = other.pool_;
            index_ = other.index_;
            other.pool_ = nullptr;
            other.index_ = invalid_index();
            return *this;
        }

        ~PacketLease() {
            release();
        }

        [[nodiscard]] bool valid() const noexcept {
            return pool_ != nullptr && index_ != invalid_index();
        }

        [[nodiscard]] PacketBuffer<Capacity>& buffer() noexcept {
            return pool_->buffers_[index_];
        }

        [[nodiscard]] const PacketBuffer<Capacity>& buffer() const noexcept {
            return pool_->buffers_[index_];
        }

        [[nodiscard]] PacketBuffer<Capacity>* operator->() noexcept {
            return &buffer();
        }

        [[nodiscard]] const PacketBuffer<Capacity>* operator->() const noexcept {
            return &buffer();
        }

        void release() noexcept {
            if (!valid()) {
                return;
            }
            pool_->release_index(index_);
            pool_ = nullptr;
            index_ = invalid_index();
        }

    private:
        friend class PacketPool<Count, Capacity>;

        static constexpr util::usize invalid_index() noexcept {
            return static_cast<util::usize>(-1);
        }

        PacketLease(PacketPool<Count, Capacity>* pool, util::usize index) noexcept
            : pool_(pool),
              index_(index) {}

        PacketPool<Count, Capacity>* pool_{nullptr};
        util::usize index_{invalid_index()};
    };

    template <util::usize Count, util::usize Capacity>
    class PacketPool {
    public:
        using Lease = PacketLease<Count, Capacity>;

        [[nodiscard]] Result<Lease> acquire(util::usize headroom = 0) noexcept {
            for (util::usize i = 0; i < Count; ++i) {
                if (used_[i]) {
                    continue;
                }

                auto reset = buffers_[i].reset(headroom);
                if (!reset) {
                    return util::unexpected(reset.error());
                }

                used_[i] = true;
                return Lease{this, i};
            }
            return util::unexpected(errc::busy);
        }

        [[nodiscard]] constexpr util::usize slot_count() const noexcept {
            return Count;
        }

        [[nodiscard]] util::usize in_use_count() const noexcept {
            util::usize count = 0;
            for (bool used : used_) {
                if (used) {
                    ++count;
                }
            }
            return count;
        }

        [[nodiscard]] util::usize free_count() const noexcept {
            return Count - in_use_count();
        }

    private:
        friend class PacketLease<Count, Capacity>;

        void release_index(util::usize index) noexcept {
            if (index >= Count) {
                return;
            }
            used_[index] = false;
            buffers_[index].clear();
        }

        std::array<PacketBuffer<Capacity>, Count> buffers_{};
        std::array<bool, Count> used_{};
    };
}
