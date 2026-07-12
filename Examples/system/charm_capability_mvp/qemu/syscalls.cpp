#include <cerrno>
#include <cstddef>
#include <sys/stat.h>

namespace {
    alignas(8) std::byte heap[1024]{};
    std::byte* heap_cursor = heap;
}

extern "C" int _close(int) {
    errno = EBADF;
    return -1;
}

extern "C" int _fstat(int, struct stat* value) {
    if (value == nullptr) {
        errno = EINVAL;
        return -1;
    }
    value->st_mode = S_IFCHR;
    return 0;
}

extern "C" int _getpid() {
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

extern "C" int _write(int, const char*, int length) {
    return length;
}

extern "C" void* _sbrk(int increment) {
    if (increment < 0) {
        errno = ENOMEM;
        return reinterpret_cast<void*>(-1);
    }
    const auto used = static_cast<std::size_t>(heap_cursor - heap);
    const auto next = used + static_cast<std::size_t>(increment);
    if (next > sizeof(heap)) {
        errno = ENOMEM;
        return reinterpret_cast<void*>(-1);
    }
    auto* previous = heap_cursor;
    heap_cursor = heap + next;
    return previous;
}
