module;

#include <cstddef>
#include <string_view>

export module posix.program_image;

import util.core;
import util.error;

export namespace posix {
    enum class ImageKind : util::u8 {
        registered,
        flat,
        elf,
        modulex
    };

    enum class ImageEntryAbi : util::u8 {
        none,
        main_argv_v0,
        main_argv_envp_v1,
    };

    using ImageEntry = int (*)(int argc, char** argv, char** envp);
    using ImageEntryV0 = int (*)(int argc, char** argv);

    struct ProgramImage {
        ImageKind kind{ImageKind::registered};
        std::string_view name{};
        ImageEntryAbi entry_abi{ImageEntryAbi::none};
        ImageEntry entry{nullptr};
        ImageEntryV0 entry_v0{nullptr};
        util::u32 stack_size{0};
        util::u32 flags{0};
    };

    inline ProgramImage make_registered_image(std::string_view name, ImageEntry entry) noexcept {
        ProgramImage image{};
        image.kind = ImageKind::registered;
        image.name = name;
        image.entry_abi = ImageEntryAbi::main_argv_envp_v1;
        image.entry = entry;
        return image;
    }

    inline ProgramImage make_registered_image(std::string_view name, ImageEntryV0 entry) noexcept {
        ProgramImage image{};
        image.kind = ImageKind::registered;
        image.name = name;
        image.entry_abi = ImageEntryAbi::main_argv_v0;
        image.entry_v0 = entry;
        return image;
    }

    inline bool has_runnable_entry(const ProgramImage& image) noexcept {
        switch (image.entry_abi) {
            case ImageEntryAbi::main_argv_envp_v1:
                return image.entry != nullptr;
            case ImageEntryAbi::main_argv_v0:
                return image.entry_v0 != nullptr;
            case ImageEntryAbi::none:
                return false;
        }
        return false;
    }

    inline util::Result<void> validate_program_image(const ProgramImage& image) noexcept {
        switch (image.entry_abi) {
            case ImageEntryAbi::main_argv_envp_v1:
                if (!image.entry || image.entry_v0) {
                    return util::unexpected(util::Errc::invalid_arg);
                }
                return {};
            case ImageEntryAbi::main_argv_v0:
                if (!image.entry_v0 || image.entry) {
                    return util::unexpected(util::Errc::invalid_arg);
                }
                return {};
            case ImageEntryAbi::none:
                return util::unexpected(util::Errc::invalid_arg);
        }
        return util::unexpected(util::Errc::invalid_arg);
    }

    inline util::Result<int> invoke_program_main(const ProgramImage& image,
                                                 int argc,
                                                 char** argv,
                                                 char** envp) noexcept {
        switch (image.entry_abi) {
            case ImageEntryAbi::main_argv_envp_v1:
                if (!image.entry) {
                    return util::unexpected(util::Errc::invalid_arg);
                }
                return image.entry(argc, argv, envp);
            case ImageEntryAbi::main_argv_v0:
                if (!image.entry_v0) {
                    return util::unexpected(util::Errc::invalid_arg);
                }
                return image.entry_v0(argc, argv);
            case ImageEntryAbi::none:
                return util::unexpected(util::Errc::invalid_arg);
        }
        return util::unexpected(util::Errc::invalid_arg);
    }
}
