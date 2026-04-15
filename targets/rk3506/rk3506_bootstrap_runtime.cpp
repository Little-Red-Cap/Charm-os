#include <cstddef>

extern "C" void* memset(void* destination, int value, std::size_t size) noexcept
{
    auto* out = static_cast<unsigned char*>(destination);
    const auto byte = static_cast<unsigned char>(value);

    for (std::size_t i = 0; i < size; ++i) {
        out[i] = byte;
    }

    return destination;
}

extern "C" void* memcpy(void* destination,
                        const void* source,
                        std::size_t size) noexcept
{
    auto* out = static_cast<unsigned char*>(destination);
    auto* in = static_cast<const unsigned char*>(source);

    for (std::size_t i = 0; i < size; ++i) {
        out[i] = in[i];
    }

    return destination;
}
