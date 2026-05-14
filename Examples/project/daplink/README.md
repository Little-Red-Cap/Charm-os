# DAPLink for STM32

`Examples/project/daplink` 当前承载的是一条面向 `STM32` 家族的 `CMSIS-DAP / CDC bridge` 固件线。

它已经不只是一个“协议层样例”，而是一套相对完整的 STM32 DAPLink 实现，包含：

- `CMSIS-DAP HID`
- 可选 `CDC` 或 `CDC + HID` 复合 USB
- 自研 USB 设备模型与最小 USB 状态机
- 面向 `f103`、`g431`、`h503` 的端口化后端

## 当前定位

这份目录已经可以按“独立优先、Charm 集成可选”的方式继续整理。

## 目录分层

- `app/`
  - `CMSIS-DAP` 协议、传输调度、CDC 桥接策略
- `frontends/usb/`
  - USB 设备模型、描述符、最小控制面 / 数据面状态机
- `port/`
  - 公共契约、能力拼装、平台无关 glue
- `platform/stm32/`
  - STM32 家族共用 backend glue
- `f103/`、`g431/`、`h503/`
  - 具体端口目录，只保留端口事实与 CubeMX 后端

## 与 Charm 的关系

- 默认构建路径已经独立，不再要求 `add_subdirectory(Charm-os)`
- 仍保留可选集成开关：`-DDAPLINK_ENABLE_CHARM_INTEGRATION=ON`

## 构建入口

唯一主入口仍然是本目录根 `CMakeLists.txt`。

- 源目录：`G:\Project\Codex\Charm-os-Project\Examples\project\daplink`
- 端口选择参数：`DAPLINK_PORT_DIR`

根 preset 当前只做“选择端口目录 + USB profile”这两件事：

- `f103-debug`
- `g431-debug`
- `h503-debug`
- `f103-hid-debug`
- `g431-hid-debug`
- `h503-hid-debug`

### 第三方依赖

当前三条 STM32 端口都依赖对应系列的 `STM32Cube` 固件包根目录。

- `f103` 需要 `DAPLINK_STM32CUBE_F1_ROOT`
- `g431` 需要 `DAPLINK_STM32CUBE_G4_ROOT`
- `h503` 需要 `DAPLINK_STM32CUBE_H5_ROOT`

这些变量可以通过 `-D...` 传入，也可以直接设置成同名环境变量。
它们都应指向各自 Cube 包根目录，也就是包含 `Drivers/` 的那一级目录。

### 使用 preset 构建

```powershell
cmake --preset g431-debug -S G:\Project\Codex\Charm-os-Project\Examples\project\daplink `
  -DDAPLINK_STM32CUBE_G4_ROOT=G:\third_party\STM32Cube_FW_G4
cmake --build --preset g431-debug
```

### 手动指定端口目录

```powershell
cmake -S G:\Project\Codex\Charm-os-Project\Examples\project\daplink `
  -B G:\Project\Codex\Charm-os-Project\Examples\project\daplink\cmake-build-daplink-g431-debug `
  -G Ninja `
  -DCMAKE_BUILD_TYPE=Debug `
  -DDAPLINK_STM32CUBE_G4_ROOT=G:\third_party\STM32Cube_FW_G4 `
  -DDAPLINK_PORT_DIR=G:\Project\Codex\Charm-os-Project\Examples\project\daplink\g431
```

## 构建约定

- `DAPLINK_PORT_DIR` 必须指向一个包含 `daplink.port.cmake` 的具体端口目录
- 根入口不再维护芯片注册表，也不猜测芯片
- `DAPLINK_PORT_DIR` 优先级最高；若未提供，则会优先复用已有 build tree 的 stamp / cache，最后才回退到默认端口
- `cmake-build-*` 这类 IDE 管理的构建目录仍然可用
- 当前活动端口会写入 `daplink.port.stamp`，并同步到：
  - `DAPLINK_ACTIVE_PORT_NAME`
  - `DAPLINK_ACTIVE_PORT_DIR`
  - `DAPLINK_ACTIVE_PORT_MANIFEST`
- 端口 manifest 负责选择匹配的 ARM toolchain；IDE 不需要额外手填一份独立 toolchain 文件

## 相关文档

- `docs/daplink_port_layering_contract.md`
  - 端口分层边界与新增端口规则
- `docs/daplink_board_capability_contract.md`
  - 板级能力拆分规则
- `docs/daplink_open_source_boundary.md`
  - 当前开源边界与依赖归属
- `docs/daplink_detach_from_charm_gap_checklist.md`
  - 脱 `Charm` 的剩余缺口清单与建议顺序
- `docs/daplink_non_stm32_gap_assessment.md`
  - 面向 `ESP32 / RP2040` 一类非 STM32 端口的缺口评估
- `docs/daplink_platform_contract_blueprint.md`
  - 平台契约拆分与未来 `platform/<vendor>/` 接入蓝图
- `docs/daplink_stm32_platform_template.md`
  - 当前 `platform/stm32/` 样板结构与未来平台目录参考模板
