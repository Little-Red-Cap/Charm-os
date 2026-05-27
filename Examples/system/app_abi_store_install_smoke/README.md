# App ABI Store Install Smoke

This host-only smoke validates the board-free App Store install semantics.

It installs a generated v1 store image into a memory-backed NOR simulator,
readbacks the installed image through `AppStoreReader`, and stages a named App
payload. The simulator starts erased at `0xff`, requires erase before write, and
only permits NOR-style `1 -> 0` bit transitions.

```powershell
cmake -S Examples/system/app_abi_store_install_smoke -B Examples/system/app_abi_store_install_smoke/cmake-build-app-abi-store-install-smoke -G Ninja -DCMAKE_CXX_COMPILER="D:/Toolchains/w64devkit/bin/g++.exe"
cmake --build Examples/system/app_abi_store_install_smoke/cmake-build-app-abi-store-install-smoke
ctest --test-dir Examples/system/app_abi_store_install_smoke/cmake-build-app-abi-store-install-smoke --output-on-failure
```
