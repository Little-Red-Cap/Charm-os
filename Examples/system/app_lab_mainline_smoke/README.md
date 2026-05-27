# App Lab Mainline Smoke

This host-only smoke validates the current official `app_lab` mainline
narrative without touching H747 firmware or board services.

It closes one complete host-first chain:

```text
embedded app lookup/run
-> qspi store install
-> qspi named/raw run-path
-> generic file-backed stub
-> diagnostics-first status report
```

The smoke does not execute Arm ELF on the host and does not reuse
`Examples/project/h747-lab/apps/app_lab/app_lab.cpp` directly. Instead, it
rebuilds the minimum host-side model needed to mirror `app_lab` semantics:

- embedded `hello_app` and `player_min`
- memory-backed NOR install/readback
- named/raw QSPI staging through `AppStoreReader`
- stable generic file-backed `not_supported` stub
- `last_*` and store-install diagnostics state

This keeps the `app_lab` mainline separate from the more generic
`app_abi_runtime_smoke`, which still only proves reusable `AppRuntime`
semantics.

```powershell
cmake -S Examples/system/app_lab_mainline_smoke -B Examples/system/app_lab_mainline_smoke/cmake-build-app-lab-mainline-smoke -G Ninja -DCMAKE_CXX_COMPILER="D:/Toolchains/w64devkit/bin/g++.exe" -DCMAKE_C_COMPILER="D:/Toolchains/w64devkit/bin/gcc.exe"
cmake --build Examples/system/app_lab_mainline_smoke/cmake-build-app-lab-mainline-smoke
ctest --test-dir Examples/system/app_lab_mainline_smoke/cmake-build-app-lab-mainline-smoke --output-on-failure
```
