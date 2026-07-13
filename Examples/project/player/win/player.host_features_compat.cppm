module;

export module player.host_features;

import player.product_policy;

export namespace player::host_features {
    inline constexpr const char* profile = product_policy::profile;
    inline constexpr bool host_ui = true;
    inline constexpr bool host_storage = product_policy::storage_default;
    inline constexpr bool fs_log = product_policy::fs_log;
    inline constexpr bool fs_dump = product_policy::fs_dump;
    inline constexpr bool playback_log = product_policy::playback_log;
    inline constexpr bool cover_decode = product_policy::cover_decode;
    inline constexpr bool file_fonts = product_policy::file_fonts;
    inline constexpr bool host_diagnostics = product_policy::diagnostics;
    inline constexpr bool host_fs_dump = product_policy::fs_dump_enabled;
    inline constexpr bool host_cover_decode = product_policy::cover_decode;
    inline constexpr bool host_file_fonts = product_policy::file_fonts;
}
