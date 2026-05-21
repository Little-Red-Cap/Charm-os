#pragma once

namespace h747::runtime {

void foundation_init();
[[noreturn]] void foundation_fail(const char* reason);

} // namespace h747::runtime
