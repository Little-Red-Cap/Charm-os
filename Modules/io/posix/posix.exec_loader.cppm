module;

#include <array>
#include <string_view>

export module posix.exec_loader;

import posix.exec_source;
import posix.fd_table;
import posix.file;
import posix.proc_types;
import posix.program_image;
import posix.program_image_elf;
import util.core;
import util.error;

export namespace posix {
    template <class Service>
    util::Result<ProgramImage> load_elf_program_from_buffer(Service& service,
                                                            const util::u8* base,
                                                            util::usize size) noexcept {
        if (!base || size == 0) {
            return util::unexpected(util::Errc::invalid_arg);
        }
        ElfLoadConfig elf_cfg{};
        elf_cfg.image_base = base;
        elf_cfg.image_size = size;
        elf_cfg.load_base = service.elf_load_base_ptr();
        elf_cfg.load_size = service.elf_load_capacity();
        elf_cfg.load_align = 16;
        auto image = load_elf_image(elf_cfg);
        if (image && service.elf_hostcalls_enabled()) {
            auto host_ok = service.apply_elf_hostcalls();
            if (!host_ok) {
                return util::unexpected(host_ok.error());
            }
        }
#if defined(POSIX_TEST_BUILD) && POSIX_TEST_BUILD
        if (image && service.elf_exec_enabled() && service.elf_exec_stub_entry()) {
            image.value().entry_abi = ImageEntryAbi::main_argv_envp_v1;
            image.value().entry = service.elf_exec_stub_entry();
        }
#endif
        return image;
    }

    template <class Service>
    util::Result<ProgramImage> load_elf_program_from_file(Service& service,
                                                          std::string_view path) noexcept {
        if (path.empty()) {
            return util::unexpected(util::Errc::invalid_arg);
        }
        if (!service.has_exec_file_service()) {
            return util::unexpected(util::Errc::not_supported);
        }
        if (path.size() >= service.exec_path_capacity()) {
            return util::unexpected(util::Errc::buffer_overflow);
        }
        auto entry = service.open_exec_file(path, O_RDONLY, 0);
        if (!entry) {
            return util::unexpected(entry.error());
        }
        const auto close_guard = [&]() noexcept {
            service.close_exec_entry(entry.value());
        };
        bool size_known = false;
        util::usize file_size = 0;
        if (entry.value().ops && entry.value().ops->stat) {
            PosixStat st{};
            auto rst = entry.value().ops->stat(entry.value().ctx, st);
            if (!rst) {
                close_guard();
                return util::unexpected(rst.error());
            }
            size_known = true;
            file_size = static_cast<util::usize>(st.size);
            if (file_size > service.elf_image_capacity()) {
                close_guard();
                return util::unexpected(util::Errc::buffer_overflow);
            }
        }
        util::usize total = 0;
        auto* buffer = service.elf_image_buffer();
        const auto capacity = service.elf_image_capacity();
        while (total < capacity) {
            const auto view = MutByteView{buffer + total, capacity - total};
            auto r = entry.value().ops->read(entry.value().ctx, view);
            if (!r) {
                close_guard();
                return util::unexpected(r.error());
            }
            if (r.value() == 0) {
                break;
            }
            total += r.value();
        }
        close_guard();
        if (!size_known && total == capacity) {
            return util::unexpected(util::Errc::buffer_overflow);
        }
        if (size_known && total < file_size) {
            return util::unexpected(util::Errc::io);
        }
        return load_elf_program_from_buffer(service, buffer, total);
    }

    template <class Service, util::usize MaxPathLen>
    util::Result<ProgramImage> load_elf_candidate(Service& service,
                                                  const SpawnConfig& cfg,
                                                  std::array<char, MaxPathLen>& resolved_path) noexcept {
        if (!service.elf_exec_enabled()) {
            return util::unexpected(util::Errc::not_supported);
        }
        if (!service.has_exec_file_service()) {
            return util::unexpected(util::Errc::not_supported);
        }
        const auto path = resolve_exec_path(cfg.path, cfg.argv);
        if (path.empty()) {
            return util::unexpected(util::Errc::invalid_arg);
        }
        if (is_elf_prefix(path) || is_elf_mem_prefix(path) ||
            (path.size() >= 8 && path.substr(0, 8) == "modulex:")) {
            return util::unexpected(util::Errc::noent);
        }
        if (cfg.path_mode == PathMode::exact) {
            return load_elf_program_from_file(service, path);
        }
        const auto path_list = envp_path(cfg.envp);
        util::Result<ProgramImage> out = util::unexpected(util::Errc::noent);
        const bool found = for_each_path_candidate<MaxPathLen>(path_list, path,
            [&](std::string_view candidate) noexcept {
                auto image = load_elf_program_from_file(service, candidate);
                if (image) {
                    out = image;
                    return true;
                }
                if (image.error() != util::Errc::noent) {
                    out = util::unexpected(image.error());
                    return true;
                }
                return false;
            });
        (void)resolved_path;
        if (found) {
            return out;
        }
        return util::unexpected(util::Errc::noent);
    }
}
