# App ABI ModuleX Smoke

This host-only smoke proves ModuleX can act as a second `AppImage` format for
the dynamic App ABI path:

```text
ModuleX bytes
-> AppImage(format=modulex)
-> ModuleX App loader
-> staged AppImageSource
-> AppRuntime
-> charm_app_main(api, argc, argv)
```

It does not define a second App model. The loaded entry is still interpreted as
`CharmAppMainFn`, and the runtime still validates `CharmAppApi` plus
`argc/argv` before calling it.

The fixture is intentionally host-only. Its ModuleX symbol table resolves
`charm_app_main` through the external resolver to a host fake function so the
smoke can verify App ABI semantics without requiring ModuleX text to contain
host machine code.

Build and run:

```powershell
cmake -S Examples/system/app_abi_modulex_smoke -B Examples/system/app_abi_modulex_smoke/cmake-build-app-abi-modulex-smoke -G Ninja -DCMAKE_CXX_COMPILER="D:/Toolchains/w64devkit/bin/g++.exe"
cmake --build Examples/system/app_abi_modulex_smoke/cmake-build-app-abi-modulex-smoke
ctest --test-dir Examples/system/app_abi_modulex_smoke/cmake-build-app-abi-modulex-smoke --output-on-failure
```
