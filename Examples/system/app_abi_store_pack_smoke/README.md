# App ABI Store Pack Smoke

This host-only smoke validates the board-free App Store writer/builder path.
It uses the same v1 `AppStoreHeader` and `AppStoreEntry` layout consumed by the
reader and H747 QSPI staging path.

The smoke proves that multiple named payloads can be packed, read back, looked
up by name, and staged into an `AppImage` without relying on QSPI, USB, or a
board.

```powershell
cmake -S Examples/system/app_abi_store_pack_smoke -B Examples/system/app_abi_store_pack_smoke/cmake-build-app-abi-store-pack-smoke -G Ninja -DCMAKE_CXX_COMPILER="D:/Toolchains/w64devkit/bin/g++.exe"
cmake --build Examples/system/app_abi_store_pack_smoke/cmake-build-app-abi-store-pack-smoke
ctest --test-dir Examples/system/app_abi_store_pack_smoke/cmake-build-app-abi-store-pack-smoke --output-on-failure
```
