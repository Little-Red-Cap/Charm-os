# App ABI Store Pack Tool

`app-abi-store-pack` 使用 `app_store_build_image()` 将 named ELF/ModuleX payload 打包为 Store v1
`.appstore.bin`。省略格式时按 ELF 处理。

```powershell
$build = "Examples/system/app_abi_store_pack_tool/cmake-build-app-abi-store-pack-tool"
cmake -S Examples/system/app_abi_store_pack_tool -B $build -G Ninja
cmake --build $build -- -j1
& "$build/app-abi-store-pack.exe" out/appstore.bin hello_app=hello_app.elf modulex_hello:modulex=hello.modulex
```

参数形状为 `<name[(:elf|:modulex)]>=<payload>`。Store v1 是开发期 staging image，不是 filesystem、
签名 manifest 或 update slot。
