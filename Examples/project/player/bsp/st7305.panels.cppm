module;

#include <cstdint>
#include <span>
#include <string_view>

export module bsp.st7305.panels;

import bsp.st7305;

export namespace bsp::st7305 {
    struct PanelPreset {
        const char* name;
        Geometry geom;
        InitOptions init;
        bool supports_hw_swap_xy;
    };

    export constexpr PanelPreset kPresetYdp290h001V3{
        "ydp290h001_v3",
        Geometry{168, 384, 0, 0x17, 0x00},
        InitOptions{},
        true
    };

    export constexpr PanelPreset kPresetYdp213h001V3{
        "ydp213h001_v3",
        Geometry{122, 250, 10, 0x19, 0x00},
        InitOptions{},
        true
    };

    export constexpr PanelPreset kPresetYdp154h008V3{
        "ydp154h008_v3",
        Geometry{200, 200, 4, 0x16, 0x00},
        InitOptions{},
        true
    };

    export constexpr PanelPreset kPresetW420hc018Mono12z{
        "w420hc018mono_12z",
        Geometry{300, 400, 144, 0x05, 0x00},
        InitOptions{},
        true
    };

    export inline std::span<const PanelPreset> panel_presets() noexcept {
        static constexpr PanelPreset kAll[] = {
            kPresetYdp290h001V3,
            kPresetYdp213h001V3,
            kPresetYdp154h008V3,
            kPresetW420hc018Mono12z
        };
        return std::span<const PanelPreset>{kAll};
    }

    export inline const PanelPreset* find_panel_preset(std::string_view name) noexcept {
        for (const auto& preset : panel_presets()) {
            if (preset.name && name == preset.name) return &preset;
        }
        return nullptr;
    }
}
