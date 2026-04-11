module;

#include <array>
#include <cstddef>
#include <span>
#include <type_traits>
#include <span>

export module posix.pipe;

import init.node;
import posix.fd_table;
import service_ring_buffer;
import util.core;
import util.error;

export namespace posix {
    struct PipeEnds {
        int read_fd{-1};
        int write_fd{-1};
    };

    template <typename T, util::usize N>
    class PipePool {
    public:
        T* create() noexcept {
            for (util::usize i = 0; i < N; ++i) {
                if (!used_[i]) {
                    used_[i] = true;
                    return new (&storage_[i]) T();
                }
            }
            return nullptr;
        }

        void destroy(T* obj) noexcept {
            if (!obj) return;
            for (util::usize i = 0; i < N; ++i) {
                auto* slot = reinterpret_cast<void*>(&storage_[i]);
                if (slot == reinterpret_cast<void*>(obj)) {
                    obj->~T();
                    used_[i] = false;
                    return;
                }
            }
        }

        void reset() noexcept {
            for (util::usize i = 0; i < N; ++i) {
                used_[i] = false;
            }
        }

    private:
        using Slot = std::aligned_storage_t<sizeof(T), alignof(T)>;
        std::array<Slot, N> storage_{};
        std::array<bool, N> used_{};
    };

    template <util::usize Capacity>
    struct PipeImpl {
        struct State;

        struct Endpoint {
            State* state{nullptr};
            bool is_reader{false};
        };

        struct State {
            service::RingBuffer<util::u8, Capacity> buffer{};
            util::u32 readers{1};
            util::u32 writers{1};
            void* owner{nullptr};
            void (*destroy)(void* owner, State* state) noexcept {nullptr};
            Endpoint read_end{};
            Endpoint write_end{};
        };

        static util::Result<util::usize> read(void* ctx, MutByteView buf) noexcept {
            auto* ep = static_cast<Endpoint*>(ctx);
            if (!ep || !ep->state || !ep->is_reader) {
                return util::unexpected(util::Errc::invalid_arg);
            }
            if (buf.empty()) {
                return util::usize{0};
            }
            auto* state = ep->state;
            if (state->buffer.empty()) {
                if (state->writers == 0) {
                    return util::unexpected(util::Errc::end_of_stream);
                }
                return util::unexpected(util::Errc::would_block);
            }
            util::usize n = 0;
            util::u8 byte = 0;
            while (n < buf.size() && state->buffer.pop(byte)) {
                buf[n++] = byte;
            }
            return n;
        }

        static util::Result<util::usize> write(void* ctx, ByteView buf) noexcept {
            auto* ep = static_cast<Endpoint*>(ctx);
            if (!ep || !ep->state || ep->is_reader) {
                return util::unexpected(util::Errc::invalid_arg);
            }
            if (buf.empty()) {
                return util::usize{0};
            }
            auto* state = ep->state;
            if (state->readers == 0) {
                return util::unexpected(util::Errc::closed);
            }
            util::usize n = 0;
            while (n < buf.size() && state->buffer.push(buf[n])) {
                ++n;
            }
            if (n == 0) {
                return util::unexpected(util::Errc::would_block);
            }
            return n;
        }

        static util::Result<void> close(void* ctx) noexcept {
            auto* ep = static_cast<Endpoint*>(ctx);
            if (!ep || !ep->state) {
                return util::unexpected(util::Errc::invalid_arg);
            }
            auto* state = ep->state;
            if (ep->is_reader) {
                if (state->readers > 0) {
                    --state->readers;
                }
            } else {
                if (state->writers > 0) {
                    --state->writers;
                }
            }
            if (state->readers == 0 && state->writers == 0) {
                if (state->destroy) {
                    state->destroy(state->owner, state);
                }
            }
            return {};
        }

        static util::Result<void> stat(void* ctx, PosixStat& out) noexcept {
            auto* ep = static_cast<Endpoint*>(ctx);
            out.mode = make_stat_mode(S_IFIFO, kModePermPipe);
            out.size = (ep && ep->state) ? static_cast<util::u64>(ep->state->buffer.size()) : 0;
            return {};
        }

        static util::Result<void> dup(void* ctx) noexcept {
            auto* ep = static_cast<Endpoint*>(ctx);
            if (!ep || !ep->state) {
                return util::unexpected(util::Errc::invalid_arg);
            }
            auto* state = ep->state;
            if (ep->is_reader) {
                ++state->readers;
            } else {
                ++state->writers;
            }
            return {};
        }

        static const FdOps& ops() noexcept {
            static const FdOps kOps{
                &PipeImpl::read,
                &PipeImpl::write,
                &PipeImpl::close,
                &PipeImpl::stat,
                &PipeImpl::dup
            };
            return kOps;
        }
    };

    template <util::usize MaxPipes, util::usize Capacity>
    class PipeService {
    public:
        using Impl = PipeImpl<Capacity>;
        using State = typename Impl::State;

        void init() noexcept { pool_.reset(); }

        template <util::usize MaxFds>
        util::Result<PipeEnds> create(FdTable<MaxFds>& table) noexcept {
            State* state = pool_.create();
            if (!state) {
                return util::unexpected(util::Errc::buffer_overflow);
            }
            state->buffer.clear();
            state->readers = 1;
            state->writers = 1;
            state->owner = this;
            state->destroy = &PipeService::destroy_state;
            state->read_end = typename Impl::Endpoint{state, true};
            state->write_end = typename Impl::Endpoint{state, false};

            FdEntry read_entry{};
            read_entry.kind = FdKind::pipe;
            read_entry.flags = FdFlags::read_only;
            read_entry.ops = &Impl::ops();
            read_entry.ctx = &state->read_end;

            FdEntry write_entry{};
            write_entry.kind = FdKind::pipe;
            write_entry.flags = FdFlags::write_only;
            write_entry.ops = &Impl::ops();
            write_entry.ctx = &state->write_end;

            auto rfd = table.attach(read_entry);
            if (!rfd) {
                pool_.destroy(state);
                return util::unexpected(rfd.error());
            }

            auto wfd = table.attach(write_entry);
            if (!wfd) {
                (void)table.close(rfd.value());
                pool_.destroy(state);
                return util::unexpected(wfd.error());
            }

            return PipeEnds{rfd.value(), wfd.value()};
        }

    private:
        static void destroy_state(void* owner, State* state) noexcept {
            auto* self = static_cast<PipeService*>(owner);
            if (!self || !state) return;
            self->pool_.destroy(state);
        }

        PipePool<State, MaxPipes> pool_{};
    };

    template <util::usize MaxPipes, util::usize Capacity>
    struct PipeServiceBinding {
        PipeService<MaxPipes, Capacity>* service{nullptr};
        std::array<init::CapId, 1> provides{};
        init::Node node{};

        explicit PipeServiceBinding(PipeService<MaxPipes, Capacity>& pipe_service,
                                    const char* cap_name = "posix.pipe",
                                    init::Phase phase = init::Phase::core,
                                    util::u32 runlevel_mask = static_cast<util::u32>(init::Runlevel::all)) noexcept
            : service(&pipe_service) {
            provides[0] = init::cap_id(cap_name);
            node = init::Node{
                cap_name,
                phase,
                runlevel_mask,
                std::span<const init::CapId>(provides.data(), provides.size()),
                {},
                &PipeServiceBinding::init_trampoline,
                nullptr,
                this
            };
        }

        static util::Result<void> init_trampoline(void* ctx) noexcept {
            auto* self = static_cast<PipeServiceBinding*>(ctx);
            if (!self || !self->service) {
                return util::unexpected(util::Errc::invalid_arg);
            }
            self->service->init();
            return {};
        }
    };
}
