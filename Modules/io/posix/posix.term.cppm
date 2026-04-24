module;

#include <array>
#include <span>
#include <string_view>

#ifdef S_IFCHR
#undef S_IFCHR
#endif

export module posix.term;

import io.channel;
import io.registry;
import init.binding;
import posix.fd_table;
import posix.file;
import util.core;
import util.error;

export namespace posix {
    struct TermDevice {
        io::Channel* channel{nullptr};
        bool is_tty{true};

        template <util::usize MaxFds>
        util::Result<void> attach_stdio(FdTable<MaxFds>& table) noexcept {
            reset_handle(stdin_handle_);
            reset_handle(stdout_handle_);
            reset_handle(stderr_handle_);

            auto r_in = attach_stdio_handle(table, stdin_handle_, 0, FdFlags::read_only);
            if (!r_in && r_in.error() != util::Errc::exist) {
                return util::unexpected(r_in.error());
            }
            auto r_out = attach_stdio_handle(table, stdout_handle_, 1, FdFlags::write_only);
            if (!r_out && r_out.error() != util::Errc::exist) {
                return util::unexpected(r_out.error());
            }
            auto r_err = attach_stdio_handle(table, stderr_handle_, 2, FdFlags::write_only);
            if (!r_err && r_err.error() != util::Errc::exist) {
                return util::unexpected(r_err.error());
            }
            return {};
        }

        static bool is_tty_entry(const FdEntry& entry) noexcept {
            return entry.kind == FdKind::term;
        }

        static const FdOps& ops() noexcept {
            static const FdOps kOps{
                &TermDevice::read,
                &TermDevice::write,
                &TermDevice::close,
                &TermDevice::stat,
                &TermDevice::dup,
                nullptr,
                &TermDevice::get_status_flags,
                &TermDevice::set_status_flags
            };
            return kOps;
        }

    private:
        struct Handle {
            TermDevice* device{nullptr};
            util::u32 refs{0};
            bool non_block{false};
        };

        template <util::usize MaxFds>
        util::Result<int> attach_stdio_handle(FdTable<MaxFds>& table,
                                              Handle& handle,
                                              int desired_fd,
                                              FdFlags flags) noexcept {
            FdEntry entry{};
            entry.kind = FdKind::term;
            entry.flags = flags;
            entry.ops = &TermDevice::ops();
            entry.ctx = &handle;
            return table.attach(entry, desired_fd);
        }

        void reset_handle(Handle& handle) noexcept {
            handle.device = this;
            handle.refs = 1;
            handle.non_block = false;
        }

        static Handle* handle_from(void* ctx) noexcept {
            return static_cast<Handle*>(ctx);
        }

        static TermDevice* device_from(void* ctx) noexcept {
            auto* handle = handle_from(ctx);
            return handle ? handle->device : nullptr;
        }

        static util::Result<util::usize> read(void* ctx, MutByteView buf) noexcept {
            auto* self = device_from(ctx);
            if (!self || !self->channel) {
                return util::unexpected(util::Errc::invalid_arg);
            }
            return self->channel->read(buf);
        }

        static util::Result<util::usize> write(void* ctx, ByteView buf) noexcept {
            auto* self = device_from(ctx);
            if (!self || !self->channel) {
                return util::unexpected(util::Errc::invalid_arg);
            }
            return self->channel->write(buf);
        }

        static util::Result<void> close(void* ctx) noexcept {
            auto* handle = handle_from(ctx);
            if (!handle || !handle->device) {
                return util::unexpected(util::Errc::invalid_arg);
            }
            if (handle->refs > 0) {
                --handle->refs;
            }
            return {};
        }

        static util::Result<void> stat(void*, PosixStat& out) noexcept {
            out.mode = make_stat_mode(S_IFCHR, kModePermChar);
            out.size = 0;
            return {};
        }

        static util::Result<void> dup(void* ctx) noexcept {
            auto* handle = handle_from(ctx);
            if (!handle || !handle->device) {
                return util::unexpected(util::Errc::invalid_arg);
            }
            ++handle->refs;
            return {};
        }

        static util::Result<int> get_status_flags(void* ctx) noexcept {
            auto* handle = handle_from(ctx);
            if (!handle || !handle->device) {
                return util::unexpected(util::Errc::invalid_arg);
            }
            return handle->non_block ? O_NONBLOCK : 0;
        }

        static util::Result<void> set_status_flags(void* ctx, int flags) noexcept {
            auto* handle = handle_from(ctx);
            if (!handle || !handle->device) {
                return util::unexpected(util::Errc::invalid_arg);
            }
            handle->non_block = (flags & O_NONBLOCK) != 0;
            return {};
        }

        Handle stdin_handle_{};
        Handle stdout_handle_{};
        Handle stderr_handle_{};
    };

    template <typename RegistryT>
    struct TermBinding {
        RegistryT* registry{nullptr};
        TermDevice* term{nullptr};
        const char* console_cap{"io.console0"};
        const char* registry_cap_name{"io.registry"};
        std::array<init::CapId, 1> provides{};
        std::array<init::CapId, 2> requires_caps{};
        init::Node node{};

        TermBinding(RegistryT& reg,
                    TermDevice& term_dev,
                    const char* cap_name = "posix.term",
                    const char* console_cap_in = "io.console0",
                    const char* registry_cap = "io.registry",
                    init::Phase phase = init::Phase::core,
                    util::u32 runlevel_mask = static_cast<util::u32>(init::Runlevel::all)) noexcept
            : registry(&reg),
              term(&term_dev),
              console_cap(console_cap_in),
              registry_cap_name(registry_cap) {
            provides = init::capability_ids(cap_name);
            requires_caps = init::capability_ids(registry_cap, console_cap_in);
            node = init::make_binding_node(init::capability_name_view(cap_name),
                                           phase,
                                           runlevel_mask,
                                           provides,
                                           requires_caps,
                                           &TermBinding::init_trampoline,
                                           nullptr,
                                           this);
        }

        constexpr std::string_view capability_name(init::CapId id) const noexcept {
            return init::lookup_capability_name(id,
                                                provides,
                                                init::capability_names(node.name),
                                                requires_caps,
                                                init::capability_names(registry_cap_name,
                                                                       console_cap));
        }

        static util::Result<void> init_trampoline(void* ctx) noexcept {
            auto* self = static_cast<TermBinding*>(ctx);
            if (!self || !self->registry || !self->term) {
                return util::unexpected(util::Errc::invalid_arg);
            }
            auto* ch = self->registry->open_channel(self->console_cap);
            if (!ch) {
                return util::unexpected(util::Errc::noent);
            }
            self->term->channel = ch;
            return {};
        }
    };
}
