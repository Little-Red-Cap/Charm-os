# App ABI Store Pack Tool

This host-only tool creates a v1 `.appstore.bin` image from named App ELF
payloads. It uses the same `app_store_build_image()` helper as the host smokes,
so generated files match the reader/staging path used by H747 `app_lab`.

```powershell
cmake -S Examples/system/app_abi_store_pack_tool -B Examples/system/app_abi_store_pack_tool/cmake-build-app-abi-store-pack-tool -G Ninja -DCMAKE_CXX_COMPILER="D:/Toolchains/w64devkit/bin/g++.exe"
cmake --build Examples/system/app_abi_store_pack_tool/cmake-build-app-abi-store-pack-tool
Examples/system/app_abi_store_pack_tool/cmake-build-app-abi-store-pack-tool/app-abi-store-pack.exe out/appstore.bin hello_app=hello_app.elf player_min=player_min.elf
```

This is not a final manifest, slot format, signing format, or filesystem. It is
the current development image shape for QSPI/eMMC/host store staging tests.
