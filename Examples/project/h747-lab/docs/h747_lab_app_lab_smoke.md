# H747 Lab App Lab Smoke

> status: `supporting`
>
> scope: board validation for `h747_lab_app_lab`

This smoke validates the embedded-image baseline:

```text
resident app_lab -> embedded/QSPI App ELF -> AppRuntime -> charm_app_main
```

## Target

- firmware: `h747_lab_app_lab`
- console: USART1, 115200 8N1, prompt `app-lab>`
- fixtures: `hello_app`, `player_min`
- entry: `charm_app_main(const CharmAppApi*, int, char**)`

## Automated Flow

Run from `Examples/project/h747-lab` and reuse the configured H747 build tree:

```powershell
cmake --build --preset build-h747-lab-app-lab-debug -- -j1
powershell -ExecutionPolicy Bypass -File tools/flash-app-lab-pyocd.ps1
powershell -ExecutionPolicy Bypass -File tools/capture-app-lab-smoke.ps1
```

The capture script resets the target, sends `app smoke` followed by `app
status`, writes the board log and validates the required tokens. The script is
the authority for token spelling and default log/port parameters.

Validate an existing log without opening serial or touching reset:

```powershell
powershell -ExecutionPolicy Bypass -File tools/capture-app-lab-smoke.ps1 -ValidateLog <path>
```

Validate only script parsing with synthetic input:

```powershell
powershell -ExecutionPolicy Bypass -File tools/capture-app-lab-smoke.ps1 -SelfTest
```

## Manual Flow

```text
app list
app run hello_app demo
app run player_min
app store install
app store status
app store list
app run-path qspi:hello_app demo
app run-path qspi:@<offset>:<size> demo
app run-path /not-supported
app smoke
app status
```

Acceptance requires:

- embedded `hello_app` and `player_min` exit zero;
- Store install and named/raw QSPI runs succeed;
- the generic file-backed path returns stable `not_supported` without entering
  the App;
- status reports source, install, run stage/code, exit and ELF load region;
- the complete capture script token set passes.

## Retained Board Evidence

The board smoke has completed with `.bin` flashed at `0x08000000`:

- embedded `hello_app` and `player_min` exited zero;
- Store v1 installed to QSPI and named/raw QSPI ELF entered `charm_app_main`;
- QSPI JEDEC and Store header were readable;
- unsupported file-backed input remained a non-executing `not_supported` result;
- ELF loaded in the D1 region beginning at `0x24070000`.

These facts prove the app_lab baseline only. Resident download, SDRAM staging,
eMMC and ModuleX evidence belong to `dev_loader`.

## Failure Classification

- pyOCD AP discovery warnings do not prove failure; require process exit zero.
- reset failure and capture-token failure are separate outcomes in the wrapper.
- a late serial attach may miss the startup banner, so readiness is checked by
  `app status` rather than requiring the banner.
- build-only, host log validation and real-board capture are distinct evidence
  domains and must not be substituted for one another.

Implementation entry: [`app_lab README`](../apps/app_lab/README.md). Capture
logic: [`capture-app-lab-smoke.ps1`](../tools/capture-app-lab-smoke.ps1).
