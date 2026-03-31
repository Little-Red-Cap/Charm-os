module;

#include <cstddef>
#include <string_view>

export module posix.program_image;

import util.core;

export namespace posix {
    enum class ImageKind : util::u8 {
        registered,
        flat,
        elf
    };

    using ImageEntry = int (*)(int argc, char** argv, char** envp);
    using ImageEntryV0 = int (*)(int argc, char** argv);

    struct ProgramImage {
        ImageKind kind{ImageKind::registered};
        std::string_view name{};
        ImageEntry entry{nullptr};
        ImageEntryV0 entry_v0{nullptr};
        util::u32 stack_size{0};
        util::u32 flags{0};
    };

    inline ProgramImage make_registered_image(std::string_view name, ImageEntryV0 entry) noexcept {
        ProgramImage image{};
        image.kind = ImageKind::registered;
        image.name = name;
        image.entry_v0 = entry;
        return image;
    }
}
