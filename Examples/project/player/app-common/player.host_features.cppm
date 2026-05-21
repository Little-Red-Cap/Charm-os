module;

export module player.host_features;

export namespace player::host_features {
    inline constexpr bool host_ui =
#if defined(CHARM_PLAYER_HOST_UI) && CHARM_PLAYER_HOST_UI
        true;
#else
        false;
#endif

    inline constexpr bool host_storage =
#if defined(CHARM_PLAYER_HOST_STORAGE) && CHARM_PLAYER_HOST_STORAGE
        true;
#else
        false;
#endif

    inline constexpr bool fs_log =
#if defined(CHARM_PLAYER_FS_LOG) && CHARM_PLAYER_FS_LOG
        true;
#else
        false;
#endif

    inline constexpr bool fs_dump =
#if defined(CHARM_PLAYER_FS_DUMP) && CHARM_PLAYER_FS_DUMP
        true;
#else
        false;
#endif

    inline constexpr bool playback_log =
#if defined(CHARM_PLAYER_PLAYBACK_LOG) && CHARM_PLAYER_PLAYBACK_LOG
        true;
#else
        false;
#endif

    inline constexpr bool cover_decode =
#if defined(CHARM_PLAYER_HOST_COVER_DECODE) && CHARM_PLAYER_HOST_COVER_DECODE
        true;
#else
        false;
#endif

    inline constexpr bool file_fonts =
#if defined(CHARM_PLAYER_HOST_FILE_FONTS) && CHARM_PLAYER_HOST_FILE_FONTS
        true;
#else
        false;
#endif

    inline constexpr bool host_diagnostics = host_ui && (fs_log || playback_log);
    inline constexpr bool host_fs_dump = host_ui && fs_log && fs_dump;
    inline constexpr bool host_cover_decode = host_ui && cover_decode;
    inline constexpr bool host_file_fonts = host_ui && file_fonts;
}
