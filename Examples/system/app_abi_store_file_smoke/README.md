# App ABI Store File Smoke

This host-only smoke writes a real `.appstore.bin` file, reads it back through
`AppStoreReader`, and stages a named payload into an `AppImage`.

It proves the file-backed side of the current v1 store shape without requiring
QSPI, USB, eMMC, or a board.

```powershell
cmake -S Examples/system/app_abi_store_file_smoke -B Examples/system/app_abi_store_file_smoke/cmake-build-app-abi-store-file-smoke -G Ninja -DCMAKE_CXX_COMPILER="D:/Toolchains/w64devkit/bin/g++.exe"
cmake --build Examples/system/app_abi_store_file_smoke/cmake-build-app-abi-store-file-smoke
ctest --test-dir Examples/system/app_abi_store_file_smoke/cmake-build-app-abi-store-file-smoke --output-on-failure
```
