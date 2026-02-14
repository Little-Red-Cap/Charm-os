module;

#include <cstddef>
#include <cstdint>
#include <string_view>

export module shell_stdio;

import shell_core;

export namespace shell {
    inline Result write(Console& c, std::string_view sv) noexcept {
        if (!c.write) return err(Errno::nosys);
        Buffer buf{sv.data(), sv.size()};
        (void)c.write(c.ctx, buf);
        return ok();
    }
}
