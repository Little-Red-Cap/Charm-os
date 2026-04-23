# DAPLink 构建入口

这个目录现在按四层职责组织：

- `app/`：公共 DAPLink 调度、DAP/CDC 桥接与业务编排
- `frontends/`：面向主机侧的接入前端，例如 USB CDC/HID；后续也可以承载无线或其它链路
- `port/`：端口契约、通用 glue、STM32 公共适配
- `f103/`、`g431/`、`h503/`：具体芯片/板级后端，以及对应的 CubeMX 生成物

从根入口构建时，必须显式选择端口；根入口不再默认绑定某一块板子。

## 分层约定

当前默认边界如下：

- `app/` 不直接依赖具体芯片宏，不直接接 HAL 回调
- `frontends/` 负责“主机如何接入这台 DAPLink 设备”，例如 USB 设备形状、描述符与前端状态机
- `port/` 通过 `daplink::port::*` 暴露统一底层能力，承接 HAL/SDK 差异
- 各端口通过 `Core/Inc/daplink_port_api.hpp` 暴露统一端口 API
- 如果某个端口需要额外的 HAL / SDK 桥接源文件，可以放在端口目录，并在根构建入口显式接入

这意味着 `f103/`、`g431/`、`h503/` 更像“后端目录”，而不是继续把芯片细节泄漏回公共层。

## 推荐构建方式

在 [CMakePresets.json](/G:/Project/Codex/Charm-os-Project/Examples/project/daplink/CMakePresets.json) 里提供了三个根级 preset：

- `f103-debug`
- `g431-debug`
- `h503-debug`

示例：

```powershell
cmake --preset h503-debug -S G:\Project\Codex\Charm-os-Project\Examples\project\daplink
cmake --build G:\Project\Codex\Charm-os-Project\Examples\project\daplink\cmake-build-daplink-h503-debug --target daplink -j 22
```

## 手动配置

如果不使用 preset，需要同时显式提供：

- `-DDAPLINK_PORT=<f103|g431|h503>`
- 与该端口匹配的 `CMAKE_TOOLCHAIN_FILE`

示例：

```powershell
cmake -S G:\Project\Codex\Charm-os-Project\Examples\project\daplink `
  -B G:\Project\Codex\Charm-os-Project\Examples\project\daplink\cmake-build-daplink-h503-debug `
  -G Ninja `
  -DDAPLINK_PORT=h503 `
  -DCMAKE_BUILD_TYPE=Debug `
  -DCMAKE_TOOLCHAIN_FILE=G:\Project\Codex\Charm-os-Project\Examples\project\daplink\cmake\toolchains\daplink-h503-gcc-arm-none-eabi.cmake
```

## 烧录

可以使用根脚本 [daplink_flash.ps1](/G:/Project/Codex/Charm-os-Project/scripts/daplink_flash.ps1)：

```powershell
.\scripts\daplink_flash.ps1 -Port h503 -Probe cmsis-dap
```

## HID Only 验证

如果需要优先排查 `Keil / MDK` 这类更依赖标准 HID CMSIS-DAP 识别的上位机，
可以先使用根级 `hid-only` 预设，把 `CDC` 暂时从 USB 复合设备里拿掉：

```powershell
cmake --preset g431-hid-debug -S G:\Project\Codex\Charm-os-Project\Examples\project\daplink
cmake --build G:\Project\Codex\Charm-os-Project\Examples\project\daplink\cmake-build-daplink-g431-hid-debug --target daplink -j 22
```

当前已提供：

- `f103-hid-debug`
- `g431-hid-debug`
- `h503-hid-debug`

兼容性约束、官方参考与当前策略见：
[docs/cmsis_dap_compatibility_notes.md](/G:/Project/Codex/Charm-os-Project/Examples/project/daplink/docs/cmsis_dap_compatibility_notes.md)
