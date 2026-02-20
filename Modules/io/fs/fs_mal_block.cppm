module;

#include <cstddef>
#include <cstdint>

export module fs_mal_block;

import fs_mal;
import fs_block;
import fs_stream;
import fs_errno;

export namespace fs {
    class MalBlock {
    public:
        Status bind(BlockDevice& dev) noexcept {
            mal_ = make_mal_from_block(dev, MalKind::block);
            dev_ = &dev;
            return Status{Err::ok};
        }

        void unbind() noexcept {
            dev_ = nullptr;
            mal_ = {};
        }

        [[nodiscard]] MalDevice& device() noexcept { return mal_; }
        [[nodiscard]] const MalDevice& device() const noexcept { return mal_; }
        [[nodiscard]] BlockDevice* block() const noexcept { return dev_; }

    private:
        BlockDevice* dev_{nullptr};
        MalDevice mal_{};
    };
}
