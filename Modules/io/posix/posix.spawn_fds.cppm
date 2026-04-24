module;

#include <array>
#include <string_view>

export module posix.spawn_fds;

import posix.exec_source;
import posix.fd_table;
import posix.file;
import posix.proc_types;
import util.core;
import util.error;

export namespace posix {
    namespace detail {
        template <util::usize MaxFds>
        util::Result<void> apply_spawn_stdio(FdTable<MaxFds>& child, const SpawnConfig& cfg) noexcept {
            if (cfg.stdio_in >= 0) {
                auto r = child.dup2(cfg.stdio_in, 0);
                if (!r) return util::unexpected(r.error());
            }
            if (cfg.stdio_out >= 0) {
                auto r = child.dup2(cfg.stdio_out, 1);
                if (!r) return util::unexpected(r.error());
            }
            if (cfg.stdio_err >= 0) {
                auto r = child.dup2(cfg.stdio_err, 2);
                if (!r) return util::unexpected(r.error());
            }
            return {};
        }

        template <util::usize MaxFds, util::usize MaxFiles, util::usize MaxPathLen>
        util::Result<void> apply_spawn_file_actions(FdTable<MaxFds>& child,
                                                    FileService<MaxFiles>* file_service,
                                                    const SpawnConfig& cfg) noexcept {
            if (!cfg.file_actions || cfg.file_actions->count == 0) {
                return {};
            }

            for (util::usize i = 0; i < cfg.file_actions->count; ++i) {
                const auto& act = cfg.file_actions->actions[i];
                switch (act.kind) {
                    case FileAction::Kind::open: {
                        if (!act.path || act.fd < 0) {
                            return util::unexpected(util::Errc::invalid_arg);
                        }
                        if (!file_service) {
                            return util::unexpected(util::Errc::not_supported);
                        }
                        std::array<char, MaxPathLen> resolved_path{};
                        auto resolved = resolve_path_from_cwd<MaxPathLen>(cfg.cwd ? std::string_view{cfg.cwd}
                                                                                  : std::string_view{"/"},
                                                                          std::string_view{act.path},
                                                                          resolved_path);
                        if (!resolved) {
                            return util::unexpected(resolved.error());
                        }
                        auto entry = file_service->open(resolved.value(), act.flags, act.mode);
                        if (!entry) {
                            return util::unexpected(entry.error());
                        }
                        if (child.get(act.fd)) {
                            auto r = child.close(act.fd);
                            if (!r) {
                                if (entry.value().ops && entry.value().ops->close) {
                                    (void)entry.value().ops->close(entry.value().ctx);
                                }
                                return util::unexpected(r.error());
                            }
                        }
                        auto r = child.attach(entry.value(), act.fd);
                        if (!r) {
                            if (entry.value().ops && entry.value().ops->close) {
                                (void)entry.value().ops->close(entry.value().ctx);
                            }
                            return util::unexpected(r.error());
                        }
                        break;
                    }
                    case FileAction::Kind::close: {
                        auto r = child.close(act.fd);
                        if (!r) return util::unexpected(r.error());
                        break;
                    }
                    case FileAction::Kind::dup2: {
                        auto r = child.dup2(act.fd, act.newfd);
                        if (!r) return util::unexpected(r.error());
                        break;
                    }
                }
            }
            return {};
        }
    }

    template <util::usize MaxFds, util::usize MaxFiles, util::usize MaxPathLen = 256>
    util::Result<FdTable<MaxFds>> build_spawn_fd_table(FdTable<MaxFds>* parent,
                                                       FileService<MaxFiles>* file_service,
                                                       const SpawnConfig& cfg) noexcept {
        if (!parent) {
            return util::unexpected(util::Errc::not_supported);
        }

        FdTable<MaxFds> child{};
        child.init();
        auto rc = parent->clone_all_to(child);
        if (!rc) {
            return util::unexpected(rc.error());
        }

        auto stdio = detail::apply_spawn_stdio(child, cfg);
        if (!stdio) {
            child.close_all();
            return util::unexpected(stdio.error());
        }

        auto actions = detail::apply_spawn_file_actions<MaxFds, MaxFiles, MaxPathLen>(child, file_service, cfg);
        if (!actions) {
            child.close_all();
            return util::unexpected(actions.error());
        }

        auto prune = child.close_non_inheritable();
        if (!prune) {
            child.close_all();
            return util::unexpected(prune.error());
        }

        return child;
    }
}
