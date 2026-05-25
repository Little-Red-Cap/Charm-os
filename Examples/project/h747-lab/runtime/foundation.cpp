#include "foundation.h"

#include "console.h"
#include "port.h"

namespace h747::runtime {

void foundation_init() {
    h747::port::runtime_init();
    h747::port::init_default_peripherals();
}

[[noreturn]] void foundation_fail(const char* reason) {
    h747::console::write("panic: ");
    h747::console::write_line(reason != nullptr ? reason : "unknown");
    h747::port::fail_fast();
}

} // namespace h747::runtime
