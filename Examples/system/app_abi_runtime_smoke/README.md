# App ABI Runtime Smoke

This host-only smoke validates the reusable dynamic App runtime prototype under
`Examples/app_abi`.

It does not execute ARM ELF on the host. Instead, it registers function-backed
images that use the same `CharmAppApi` and `charm_app_main(api, argc, argv)`
contract as embedded App ELF samples. H747 `app_lab` uses the same runtime
surface with an ELF-backed loader callback.

The smoke proves:

- successful `lookup -> load -> abi -> argv -> start -> exit`;
- `argc/argv` construction;
- `CharmAppApi` validation;
- Player-mini display/time/input capability usage;
- stable failure stages for missing image, load failure, missing ABI entry, bad
  API, and argv overflow.

```powershell
cmake -S Examples/system/app_abi_runtime_smoke -B Examples/system/app_abi_runtime_smoke/cmake-build-app-abi-runtime-smoke -G Ninja -DCMAKE_CXX_COMPILER="D:/Toolchains/w64devkit/bin/g++.exe"
cmake --build Examples/system/app_abi_runtime_smoke/cmake-build-app-abi-runtime-smoke
ctest --test-dir Examples/system/app_abi_runtime_smoke/cmake-build-app-abi-runtime-smoke --output-on-failure
```
