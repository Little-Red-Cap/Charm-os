module;

export module player.product_policy;

export namespace player::product_policy {
    inline constexpr const char* profile =
#if defined(CHARM_PLAYER_PROFILE)
        CHARM_PLAYER_PROFILE;
#else
        "default";
#endif

    inline constexpr bool storage_default =
#if defined(CHARM_PLAYER_STORAGE_DEFAULT) && CHARM_PLAYER_STORAGE_DEFAULT
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
#if defined(CHARM_PLAYER_COVER_DECODE) && CHARM_PLAYER_COVER_DECODE
        true;
#else
        false;
#endif

    inline constexpr bool file_fonts =
#if defined(CHARM_PLAYER_FILE_FONTS) && CHARM_PLAYER_FILE_FONTS
        true;
#else
        false;
#endif

    inline constexpr bool diagnostics = fs_log || playback_log;
    inline constexpr bool fs_dump_enabled = fs_log && fs_dump;
}
