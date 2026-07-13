# App Lab Compatibility Smoke

> status: `supporting`
>
> note: the directory/target keeps its historical `mainline` name

This Host-only fixture mirrors the H747 `app_lab` embedded/QSPI baseline:

```text
embedded app lookup/run
-> memory-NOR Store install/readback
-> named/raw Store staging
-> unsupported generic path
-> status diagnostics
```

It does not execute Arm ELF or reuse H747 monitor code. The fixture rebuilds a
small Host model around `hello_app`, `player_min`, `AppStoreReader`, Store install
and the stable file-backed `not_supported` result.

The target proves compatibility with the
[`app_lab` baseline](../../project/h747-lab/apps/app_lab/README.md). It is not the
resident platform mainline and does not replace `dev_loader`, generic AppRuntime
smokes or H747 board evidence.

Configure and validate using one existing build directory owned by this smoke:

```powershell
cmake -S Examples/system/app_lab_mainline_smoke -B <cmake-build-dir> -G Ninja
cmake --build <cmake-build-dir> -- -j1
ctest --test-dir <cmake-build-dir> --output-on-failure
```

`CMakeLists.txt` and `main.cpp` own the current compiler requirements, fixtures
and assertions.
