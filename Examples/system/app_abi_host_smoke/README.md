# App ABI Host Smoke

This host-only smoke validates the first-generation dynamic App ABI without
loading ELF:

```c
int charm_app_main(const CharmAppApi* api, int argc, char** argv);
```

It directly links the same C sample sources used for embedded App ELF generation
and calls them through a mock `CharmAppApi`.

## What It Proves

- `hello_app` can use console capability and receive `argc/argv`.
- `player_min` can query display mode, poll input, query time, present a bounded
  raster payload, and return an exit code.
- Storage and AFE capabilities can remain present but unsupported without
  blocking Player-oriented apps.

## Build

```powershell
$env:PATH = "D:/Toolchains/w64devkit/bin;$env:PATH"
cmake -S Examples/system/app_abi_host_smoke -B Examples/system/app_abi_host_smoke/cmake-build-app-abi-host-smoke -G Ninja -DCMAKE_CXX_COMPILER="D:/Toolchains/w64devkit/bin/g++.exe" -DCMAKE_C_COMPILER="D:/Toolchains/w64devkit/bin/gcc.exe"
cmake --build Examples/system/app_abi_host_smoke/cmake-build-app-abi-host-smoke
ctest --test-dir Examples/system/app_abi_host_smoke/cmake-build-app-abi-host-smoke --output-on-failure
```
