// app.data.cppm
// Example app state data (domain fields only).

module;
#include <cstdint>

export module app.data;

export namespace app {

    struct AppData {
        bool         lamp_on{false};
        std::uint8_t battery{75}; // 0..100
        std::uint8_t lamp_brightness{0};
        bool         check_a{false};
        bool         check_b{true};
        bool         switch_a{false};
        std::uint8_t progress_demo{45};
        std::uint8_t chart[8]{10, 30, 50, 70, 90, 70, 50, 30};
    };

} // namespace app
