#pragma once

// Enforce a single entry contract across the repo.
// Define CHARM_LIB_BUILD for the core library target.
// Define CHARM_ENTRY_ALLOWED for the chosen executable entry.
#if !defined(CHARM_LIB_BUILD) && !defined(CHARM_ENTRY_ALLOWED)
#error "Charm entry is not allowed in this target. Define CHARM_ENTRY_ALLOWED=1 for the selected entry target."
#endif
