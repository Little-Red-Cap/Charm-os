#pragma once

// Enforce a single entry contract across the repo.
// Define CHARM_LIB_BUILD for the core library target.
// Define CHARM_ENTRY_ALLOWED for the chosen executable entry.
#if !defined(CHARM_LIB_BUILD) && !defined(CHARM_ENTRY_ALLOWED)
#error "Charm entry is not allowed in this target. Define CHARM_ENTRY_ALLOWED=1 for the selected entry target."
#endif

// Entry must declare it uses init::Graph to own startup.
#if defined(CHARM_ENTRY_ALLOWED) && !defined(CHARM_ENTRY_USES_GRAPH)
#error "Charm entry must declare CHARM_ENTRY_USES_GRAPH=1 to confirm init::Graph usage."
#endif

#ifdef __cplusplus
// Forbid direct HAL usage outside designated board/runtime glue.
#if !defined(CHARM_ALLOW_HAL)
#if defined(__GNUC__)
#pragma GCC poison HAL_Init
#pragma GCC poison HAL_DeInit
#pragma GCC poison HAL_Delay
#pragma GCC poison HAL_GetTick
#pragma GCC poison HAL_PCD_Init
#pragma GCC poison HAL_SD_Init
#pragma GCC poison HAL_MMC_Init
#pragma GCC poison HAL_RCCEx_PeriphCLKConfig
#endif
#endif
#endif
