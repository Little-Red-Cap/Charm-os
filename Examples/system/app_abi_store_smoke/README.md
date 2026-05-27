# App ABI Store Smoke

This host-only smoke validates the prototype program-image store layout defined
in `Examples/app_abi/charm_app_store.hpp`.

The layout is used by H747 `app_lab` for the first read-only QSPI Nor App image
source. This smoke does not model QSPI hardware, ELF loading, erase/write, or a
filesystem; it only freezes the header/entry semantics used to map app names to
ELF byte ranges.

```powershell
cmake -S Examples/system/app_abi_store_smoke -B Examples/system/app_abi_store_smoke/cmake-build-app-abi-store-smoke -G Ninja -DCMAKE_CXX_COMPILER="D:/Toolchains/w64devkit/bin/g++.exe"
cmake --build Examples/system/app_abi_store_smoke/cmake-build-app-abi-store-smoke
ctest --test-dir Examples/system/app_abi_store_smoke/cmake-build-app-abi-store-smoke --output-on-failure
```
