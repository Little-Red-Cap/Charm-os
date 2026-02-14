#include <cstddef>
#include <cstdint>
#include <cstdio>

import module_core;
import module_loader;
import module_link;

int main() {
    modulex::ImageHeader img{};
    auto loaded = modulex::Loader::load(&img);
    auto linked = modulex::Linker::relocate(&img, 0);
    std::printf("[module_demo] load=%d link=%d\n", loaded.ok ? 1 : 0, linked ? 1 : 0);
    return 0;
}
