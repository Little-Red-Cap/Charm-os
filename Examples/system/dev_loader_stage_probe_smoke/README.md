# Dev Loader Stage Probe Smoke

Host-only proof for the H747 `dev app stage/probe` command semantics.

It simulates:

```text
launch_ready payload -> received_image_read -> app_received_image_stage -> app_elf_probe_load
```

It does not call `charm_app_main`, does not jump, and does not model a USB or
UART transport. Transport smokes own the download path; this smoke owns the
post-download App ELF preparation boundary.
