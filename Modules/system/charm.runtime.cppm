export module charm.runtime;

// Removed zombie facade.
//
// `charm.runtime` used to re-export most of System/IO/FS/HAL/Shell/Out.
// That made a migration shim look like the real Charm Runtime entry and
// encouraged permanent dependence on an unclear boundary.
//
// Do not add exports back here. New code must import the actual subsystem
// entry (`charm.system`, `charm.io`, `charm.net`) or the narrow leaf module it
// uses (`fs_vfs`, `hal_uart`, `shell_cmd`, ...).
