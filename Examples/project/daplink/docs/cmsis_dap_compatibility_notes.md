# DAPLink CMSIS-DAP 兼容性笔记

这份笔记记录当前工程在 `Keil / MDK / pyOCD / OpenOCD` 之外的更上层目标：
先把设备做得更像“标准 CMSIS-DAP 调试器”，而不是为某一个上位机单独写私有兼容层。

## 当前策略

- 先修 CMSIS-DAP 协议细节，再谈 IDE 兼容性。
- 先提供 `hid-only` 验证入口，确认 HID 调试链路本身没有歧义，再回到 `CDC + HID` 复合设备形态。
- `f103/`、`g431/`、`h503/` 继续只承担端口后端职责；协议、USB 前端和构建策略尽量留在公共层。

## 官方约束

- `DAP_Info(0x04)` 是 `Protocol Version`，不是产品固件版本。
- `DAP_Info(0x09)` 才是 `Product Firmware Version`。
- `DAP_Info` 的字符串返回长度需要包含结尾的 `NUL` 字节。
- USB `Product String` 或 `Interface String` 里需要能识别出 `CMSIS-DAP`。
- 如果设备是复合设备，把 `CMSIS-DAP` 放进 HID 接口字符串会更稳妥。

## 当前工程里的落地选择

- `DAP_Info(0x04)` 目前返回 `1.3.0`。
  这是一个工程判断：当前实现仍然走 HID 形态，`1.3.0` 已覆盖我们实际使用到的 `Product Firmware Version` 字段。
- `DAP_Info(0x09)` 返回产品固件版本。
- HID 接口字符串显式写成 `CMSIS-DAP v1`。
- CDC 相关接口分别给出独立字符串，避免复合设备里所有接口都显示成匿名接口。
- 根 CMake 入口支持 `DAPLINK_USB_PROFILE=hid|cdc|composite`。
- 默认仍保留 `composite`，但新增 `<port>-hid-debug` 预设用于 IDE 兼容性排查。

## 适合优先做的验证

1. 先用 `*-hid-debug` 构建并验证 Keil/MDK 是否能稳定识别 HID 调试接口。
2. 再切回默认 `composite` 预设，观察复合设备下的识别和下载表现。
3. 如果 HID 正常、复合异常，优先怀疑 USB 描述符和主机驱动绑定，而不是 SWD 事务层。

## 暂不急着做的事

- 正式 VID/PID
- Microsoft OS Descriptor / WCID
- 面向量产的唯一序列号生成
- CMSIS-DAP v2 Bulk 传输迁移

## 参考资料

- [CMSIS-DAP DAP_Info](https://arm-software.github.io/CMSIS-DAP/latest/group__DAP__Info.html)
- [CMSIS-DAP Debug Unit Information](https://arm-software.github.io/CMSIS-DAP/latest/group__DAP__Config__Debug__gr.html)
- [CMSIS-DAP USB Configuration](https://arm-software.github.io/CMSIS_5/DAP/html/group__DAP__ConfigUSB__gr.html)
- [CMSIS-DAP Firmware Notes](https://arm-software.github.io/CMSIS-DAP/latest/dap_firmware.html)
- [CMSIS-DAP Revision History](https://arm-software.github.io/CMSIS_5/DAP/html/dap_revisionHistory.html)
