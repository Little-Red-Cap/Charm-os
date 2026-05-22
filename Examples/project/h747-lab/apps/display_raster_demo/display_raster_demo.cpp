#include "display_raster_demo.h"

#include "console.h"
#include "display_raster_demo_domain.hpp"
#include "h747_world.hpp"

namespace {

h747::world::DiyBoardWorld& active_world() noexcept {
    static h747::world::DiyBoardWorld instance{};
    return instance;
}

void print_hex32(const char* label, const std::uint32_t value) {
    h747::console::write(label);
    h747::console::write_hex32(value);
}

void print_dec32(const char* label, const std::uint32_t value) {
    h747::console::write(label);
    h747::console::write_dec(value);
}

void print_raster_state(const char* prefix) {
    const auto state = active_world().display().state().raw;
    const auto mode = active_world().display().mode();

    h747::console::write(prefix);
    h747::console::write(" mode=");
    h747::console::write_dec(mode.extent.width);
    h747::console::write("x");
    h747::console::write_dec(mode.extent.height);
    h747::console::write(" fmt=argb8888");
    print_hex32(" fb=", state.framebuffer_base);
    print_hex32(" bytes=", state.framebuffer_bytes);
    print_hex32(" front=", state.front_buffer_base);
    print_hex32(" back=", state.back_buffer_base);
    h747::console::write(" init=");
    h747::console::write_dec(state.init_ok);
    h747::console::write(" sdram=");
    h747::console::write_dec(state.sdram_ready);
    h747::console::write(" smoke=");
    h747::console::write_dec(state.sdram_smoke_ok);
    print_dec32(" words=", state.sdram_tested_words);
    print_hex32(" first_err=", static_cast<std::uint32_t>(state.sdram_first_error_addr));
    h747::console::write("\n");

    h747::console::write(prefix);
    h747::console::write("_regs");
    h747::console::write(" layer=");
    h747::console::write_dec(state.ltdc_layer_ready);
    h747::console::write(" present=");
    h747::console::write_dec(state.present_count);
    h747::console::write(" clean=");
    h747::console::write_dec(state.cache_clean_count);
    print_hex32(" hal=", state.last_hal_status);
    print_hex32(" sdram_hal=", state.sdram_last_hal_status);
    print_hex32(" dsi_err=", state.dsi_error);
    print_hex32(" WCR=", state.dsi_wcr);
    print_hex32(" WISR=", state.dsi_wisr);
    print_hex32(" LTDC_ISR=", state.ltdc_isr);
    h747::console::write("\n");
}

} // namespace

namespace h747::apps::display_raster_demo {

void init() {
    active_world().init();
    print_raster_state("display_raster");
    init(active_world());
    print_raster_state("display_raster");
}

void loop_once() noexcept {
    loop_once(active_world());
    static std::uint32_t last_present = 0U;
    const auto present = active_world().display().state().raw.present_count;
    if (present != last_present) {
        last_present = present;
        print_raster_state("display_raster");
    }
}

} // namespace h747::apps::display_raster_demo
