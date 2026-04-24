# RTOS (v0) examples

This folder hosts minimal RTOS demos built on `charm.system.rtos`.
The v0 scheduler is cooperative and single-core:

- No dynamic allocation
- No exceptions
- Tasks must call `yield()` or `sleep_ms()` to give time back

See `qemu/README.md` for a QEMU-oriented run template.
