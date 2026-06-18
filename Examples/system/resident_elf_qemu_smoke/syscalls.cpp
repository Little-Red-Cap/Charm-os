#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <sys/stat.h>

namespace {

alignas(8) std::byte g_heap[16 * 1024]{};
std::byte* g_heap_cursor = g_heap;

} // namespace

extern "C" int _close(int) {
    errno = EBADF;
    return -1;
}

extern "C" int _fstat(int, struct stat* st) {
    if (st == nullptr) {
        errno = EINVAL;
        return -1;
    }
    st->st_mode = S_IFCHR;
    return 0;
}

extern "C" int _getpid(void) {
    return 1;
}

extern "C" int _isatty(int) {
    return 1;
}

extern "C" int _kill(int, int) {
    errno = EINVAL;
    return -1;
}

extern "C" int _lseek(int, int, int) {
    return 0;
}

extern "C" int _read(int, char*, int) {
    errno = EAGAIN;
    return -1;
}

extern "C" int _write(int, const char*, int len) {
    return len;
}

extern "C" void* _sbrk(int incr) {
    if (incr < 0) {
        errno = ENOMEM;
        return reinterpret_cast<void*>(-1);
    }
    const auto used = static_cast<std::size_t>(g_heap_cursor - g_heap);
    const auto next = used + static_cast<std::size_t>(incr);
    if (next > sizeof(g_heap)) {
        errno = ENOMEM;
        return reinterpret_cast<void*>(-1);
    }
    auto* previous = g_heap_cursor;
    g_heap_cursor = g_heap + next;
    return previous;
}
