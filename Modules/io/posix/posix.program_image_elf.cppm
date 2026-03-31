module;

#include <cstddef>

export module posix.program_image_elf;

import posix.program_image;
import util.core;
import util.error;

export namespace posix {
    struct ElfLoadConfig {
        const void* image_base{nullptr};
        util::usize image_size{0};
        void* load_base{nullptr};
    };

    inline util::Result<ProgramImage> load_elf_image(const ElfLoadConfig& cfg) noexcept {
        if (!cfg.image_base || cfg.image_size == 0) {
            return util::unexpected(util::Errc::invalid_arg);
        }
        if (!cfg.load_base) {
            return util::unexpected(util::Errc::not_supported);
        }
        return util::unexpected(util::Errc::not_supported);
    }
}
