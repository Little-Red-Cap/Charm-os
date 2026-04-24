module;

export module player.mcu_policy;

export namespace player::mcu_policy {
    inline constexpr bool enabled =
#if defined(CHARM_PLAYER_MCU) && CHARM_PLAYER_MCU && \
    defined(CHARM_PLAYER_MCU_STRICT) && CHARM_PLAYER_MCU_STRICT
        true;
#else
        false;
#endif

    template <bool Enabled = enabled>
    consteval void guard(const char*) {
        static_assert(!Enabled, "MCU strict guard triggered");
    }
}

#if defined(CHARM_PLAYER_MCU) && CHARM_PLAYER_MCU && \
    defined(CHARM_PLAYER_MCU_STRICT) && CHARM_PLAYER_MCU_STRICT
#define CHARM_PLAYER_MCU_GUARD(MSG) ::player::mcu_policy::guard(MSG)
#else
#define CHARM_PLAYER_MCU_GUARD(MSG) ((void)0)
#endif
