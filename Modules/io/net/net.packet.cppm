module;

#include <array>
#include <cstddef>
#include <new>

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

    class OwnedPacket {
    public:
        OwnedPacket() noexcept = default;
        OwnedPacket(const OwnedPacket&) = delete;
        OwnedPacket& operator=(const OwnedPacket&) = delete;

        OwnedPacket(OwnedPacket&& other) noexcept {
            move_from(static_cast<OwnedPacket&&>(other));
        }

        OwnedPacket& operator=(OwnedPacket&& other) noexcept {
            if (this == &other) {
                return *this;
            }
            reset();
            move_from(static_cast<OwnedPacket&&>(other));
            return *this;
        }

        template <util::usize Count, util::usize Capacity>
        OwnedPacket(PacketLease<Count, Capacity>&& lease) noexcept {
            emplace(static_cast<PacketLease<Count, Capacity>&&>(lease));
        }

        ~OwnedPacket() {
            reset();
        }

        [[nodiscard]] static OwnedPacket borrowed(PacketView packet) noexcept {
            OwnedPacket owned{};
            owned.borrowed_view_ = packet;
            owned.kind_ = Kind::borrowed_const;
            return owned;
        }

        [[nodiscard]] static OwnedPacket borrowed(MutPacketView packet) noexcept {
            OwnedPacket owned{};
            owned.borrowed_mut_view_ = packet;
            owned.kind_ = Kind::borrowed_mut;
            return owned;
        }

        [[nodiscard]] bool valid() const noexcept {
            return kind_ != Kind::empty;
        }

        [[nodiscard]] bool owns_storage() const noexcept {
            return kind_ == Kind::leased;
        }

        [[nodiscard]] PacketView view() const noexcept {
            switch (kind_) {
                case Kind::borrowed_const:
                    return borrowed_view_;
                case Kind::borrowed_mut:
                    return PacketView{borrowed_mut_view_.payload, borrowed_mut_view_.headroom, borrowed_mut_view_.tailroom};
                case Kind::leased:
                    return view_fn_ ? view_fn_(storage()) : PacketView{};
                case Kind::empty:
                default:
                    return {};
            }
        }

        [[nodiscard]] Result<void> trim_front(util::usize count) noexcept {
            switch (kind_) {
                case Kind::borrowed_const:
                    if (count > borrowed_view_.size()) {
                        return util::unexpected(errc::invalid_arg);
                    }
                    borrowed_view_ = borrowed_view_.subspan(count);
                    return {};
                case Kind::borrowed_mut:
                    if (count > borrowed_mut_view_.size()) {
                        return util::unexpected(errc::invalid_arg);
                    }
                    borrowed_mut_view_ = borrowed_mut_view_.subspan(count);
                    return {};
                case Kind::leased:
                    if (trim_front_fn_ == nullptr) {
                        return util::unexpected(errc::not_supported);
                    }
                    return trim_front_fn_(storage(), count);
                case Kind::empty:
                default:
                    return util::unexpected(errc::bad_state);
            }
        }

        [[nodiscard]] Result<void> trim_back(util::usize count) noexcept {
            switch (kind_) {
                case Kind::borrowed_const:
                    if (count > borrowed_view_.size()) {
                        return util::unexpected(errc::invalid_arg);
                    }
                    borrowed_view_ = borrowed_view_.subspan(0, borrowed_view_.size() - count);
                    return {};
                case Kind::borrowed_mut:
                    if (count > borrowed_mut_view_.size()) {
                        return util::unexpected(errc::invalid_arg);
                    }
                    borrowed_mut_view_ = borrowed_mut_view_.subspan(0, borrowed_mut_view_.size() - count);
                    return {};
                case Kind::leased:
                    if (trim_back_fn_ == nullptr) {
                        return util::unexpected(errc::not_supported);
                    }
                    return trim_back_fn_(storage(), count);
                case Kind::empty:
                default:
                    return util::unexpected(errc::bad_state);
            }
        }

        [[nodiscard]] MutPacketView mut_view() noexcept {
            switch (kind_) {
                case Kind::borrowed_mut:
                    return borrowed_mut_view_;
                case Kind::leased:
                    return mut_view_fn_ ? mut_view_fn_(storage()) : MutPacketView{};
                case Kind::borrowed_const:
                case Kind::empty:
                default:
                    return {};
            }
        }

        void release() noexcept {
            reset();
        }

    private:
        enum class Kind : util::u8 {
            empty,
            borrowed_const,
            borrowed_mut,
            leased,
        };

        using ViewFn = PacketView (*)(const void*) noexcept;
        using MutViewFn = MutPacketView (*)(void*) noexcept;
        using TrimFn = Result<void> (*)(void*, util::usize) noexcept;
        using MoveFn = void (*)(void*, void*) noexcept;
        using DestroyFn = void (*)(void*) noexcept;

        static constexpr util::usize inline_storage_size = sizeof(void*) * 4;

        template <util::usize Count, util::usize Capacity>
        void emplace(PacketLease<Count, Capacity>&& lease) noexcept {
            using Lease = PacketLease<Count, Capacity>;
            static_assert(sizeof(Lease) <= inline_storage_size);
            ::new (storage()) Lease(static_cast<Lease&&>(lease));
            view_fn_ = [](const void* self) noexcept {
                return static_cast<const Lease*>(self)->buffer().view();
            };
            mut_view_fn_ = [](void* self) noexcept {
                return static_cast<Lease*>(self)->buffer().mut_view();
            };
            trim_front_fn_ = [](void* self, util::usize count) noexcept {
                return static_cast<Lease*>(self)->buffer().trim_front(count);
            };
            trim_back_fn_ = [](void* self, util::usize count) noexcept {
                return static_cast<Lease*>(self)->buffer().trim_back(count);
            };
            move_fn_ = [](void* dst, void* src) noexcept {
                auto* lease = static_cast<Lease*>(src);
                ::new (dst) Lease(static_cast<Lease&&>(*lease));
                lease->~Lease();
            };
            destroy_fn_ = [](void* self) noexcept {
                static_cast<Lease*>(self)->~Lease();
            };
            kind_ = Kind::leased;
        }

        [[nodiscard]] void* storage() noexcept {
            return lease_storage_.data();
        }

        [[nodiscard]] const void* storage() const noexcept {
            return lease_storage_.data();
        }

        void reset() noexcept {
            if (kind_ == Kind::leased && destroy_fn_ != nullptr) {
                destroy_fn_(storage());
            }
            borrowed_view_ = {};
            borrowed_mut_view_ = {};
            view_fn_ = nullptr;
            mut_view_fn_ = nullptr;
            trim_front_fn_ = nullptr;
            trim_back_fn_ = nullptr;
            move_fn_ = nullptr;
            destroy_fn_ = nullptr;
            kind_ = Kind::empty;
        }

        void move_from(OwnedPacket&& other) noexcept {
            switch (other.kind_) {
                case Kind::borrowed_const:
                    borrowed_view_ = other.borrowed_view_;
                    kind_ = Kind::borrowed_const;
                    other.borrowed_view_ = {};
                    other.kind_ = Kind::empty;
                    return;
                case Kind::borrowed_mut:
                    borrowed_mut_view_ = other.borrowed_mut_view_;
                    kind_ = Kind::borrowed_mut;
                    other.borrowed_mut_view_ = {};
                    other.kind_ = Kind::empty;
                    return;
                case Kind::leased:
                    view_fn_ = other.view_fn_;
                    mut_view_fn_ = other.mut_view_fn_;
                    trim_front_fn_ = other.trim_front_fn_;
                    trim_back_fn_ = other.trim_back_fn_;
                    move_fn_ = other.move_fn_;
                    destroy_fn_ = other.destroy_fn_;
                    kind_ = Kind::leased;
                    if (move_fn_ != nullptr) {
                        move_fn_(storage(), other.storage());
                    }
                    other.view_fn_ = nullptr;
                    other.mut_view_fn_ = nullptr;
                    other.trim_front_fn_ = nullptr;
                    other.trim_back_fn_ = nullptr;
                    other.move_fn_ = nullptr;
                    other.destroy_fn_ = nullptr;
                    other.kind_ = Kind::empty;
                    return;
                case Kind::empty:
                default:
                    return;
            }
        }

        PacketView borrowed_view_{};
        MutPacketView borrowed_mut_view_{};
        alignas(std::max_align_t) std::array<unsigned char, inline_storage_size> lease_storage_{};
        ViewFn view_fn_{nullptr};
        MutViewFn mut_view_fn_{nullptr};
        TrimFn trim_front_fn_{nullptr};
        TrimFn trim_back_fn_{nullptr};
        MoveFn move_fn_{nullptr};
        DestroyFn destroy_fn_{nullptr};
        Kind kind_{Kind::empty};
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
