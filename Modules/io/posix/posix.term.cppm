module;

#include <array>
#include <span>

export module posix.term;

import io.channel;
import io.registry;
import init.node;
import posix.fd_table;
import util.core;
import util.error;

export namespace posix {
    struct TermDevice {
        io::Channel* channel{nullptr};
        bool is_tty{true};

        template <util::usize MaxFds>
        util::Result<void> attach_stdio(FdTable<MaxFds>& table) noexcept {
            FdEntry in{};
            in.kind = FdKind::term;
            in.flags = FdFlags::read_only;
            in.ops = &TermDevice::ops();
            in.ctx = this;

            FdEntry out{};
            out.kind = FdKind::term;
            out.flags = FdFlags::write_only;
            out.ops = &TermDevice::ops();
            out.ctx = this;

            FdEntry err = out;

            auto r_in = table.attach(in, 0);
            if (!r_in && r_in.error() != util::Errc::exist) {
                return util::unexpected(r_in.error());
            }
            auto r_out = table.attach(out, 1);
            if (!r_out && r_out.error() != util::Errc::exist) {
                return util::unexpected(r_out.error());
            }
            auto r_err = table.attach(err, 2);
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
                nullptr
            };
            return kOps;
        }

    private:
        static util::Result<util::usize> read(void* ctx, MutByteView buf) noexcept {
            auto* self = static_cast<TermDevice*>(ctx);
            if (!self || !self->channel) {
                return util::unexpected(util::Errc::invalid_arg);
            }
            return self->channel->read(buf);
        }

        static util::Result<util::usize> write(void* ctx, ByteView buf) noexcept {
            auto* self = static_cast<TermDevice*>(ctx);
            if (!self || !self->channel) {
                return util::unexpected(util::Errc::invalid_arg);
            }
            return self->channel->write(buf);
        }

        static util::Result<void> close(void*) noexcept { return {}; }

        static util::Result<void> stat(void*, PosixStat& out) noexcept {
            out.mode = make_stat_mode(S_IFCHR, kModePermChar);
            out.size = 0;
            return {};
        }

        static util::Result<void> dup(void*) noexcept { return {}; }
    };

    template <typename RegistryT>
    struct TermBinding {
        RegistryT* registry{nullptr};
        TermDevice* term{nullptr};
        const char* console_cap{"io.console0"};
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
            : registry(&reg), term(&term_dev), console_cap(console_cap_in) {
            provides[0] = init::cap_id(cap_name);
            requires_caps[0] = init::cap_id(registry_cap);
            requires_caps[1] = init::cap_id(console_cap);
            node = init::Node{
                cap_name,
                phase,
                runlevel_mask,
                std::span<const init::CapId>(provides.data(), provides.size()),
                std::span<const init::CapId>(requires_caps.data(), requires_caps.size()),
                &TermBinding::init_trampoline,
                nullptr,
                this
            };
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
