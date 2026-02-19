# VSF USB 体系映射（Device/Host/Driver/Class）

目标：抽取 VSF USB 组件的分层结构与接口模式，为 Charm 的 USB 规划提供可迁移骨架。

## 1) 目录分层（VSF）

- `component/usb/common`
  - `usb_common.h` / `usb_desc.h`
  - class 描述头（CDC/DFU/HID/MSC/UAC/UVC/HUB/XB1 等）

- `component/usb/device`
  - `vsf_usbd.h/.c` 设备栈
  - class drivers（CDC/MSC/UAC/UVC/HID/DFU）
  - extensions（BOS/WebUSB/WinUSB/MS）

- `component/usb/host`
  - `vsf_usbh.h/.c` 主机栈
  - class drivers（CDC/HID/MSC/UAC/UVC/HUB/DFU 等）

- `component/usb/driver`
  - DCD（device controller driver）
  - HCD（host controller driver）
  - OTG（DWCOTG/MUSB 等）

- `component/usb/utils`
  - usbmitm（USB 中间层/代理）

## 2) VSF Device 栈核心结构

关键点（`vsf_usbd.h`）：

- 设备对象 `vk_usbd_dev_t`
  - 持有：配置表、描述符表、速度、DCD driver、控制请求处理器
- 设备配置 `vk_usbd_cfg_t`
  - 含接口列表与 init/fini
- 接口对象 `vk_usbd_ifs_t`
  - 绑定 class_op + param
- class driver 操作表 `vk_usbd_class_op_t`
  - get_desc / request_prepare / request_process / init / fini
- 传输对象 `vk_usbd_trans_t`
  - EP、buffer、ZLP、回调/EDA

特性：
- 描述符构建大量使用宏（usbd_common_desc / usbd_str_desc 等）
- 支持 raw mode（暴露驱动接口）
- EDA 事件机制可选（USBD_CFG_USE_EDA）

## 3) VSF Host 栈核心结构

关键点（`vsf_usbh.h`）：

- HCD 驱动接口 `vk_usbh_hcd_drv_t`
  - init/fini/alloc_urb/submit_urb/reset_dev
- URB 对象 `vk_usbh_hcd_urb_t`
  - pipe/transfer buffer/status/complete 回调
- Host 设备对象 `vk_usbh_dev_t`
  - ep0、接口列表、层级关系
- class driver `vk_usbh_class_drv_t`
  - probe / disconnect
- pipe 描述 `vk_usbh_pipe_t` + flags

特性：
- 设备枚举与解析状态机（probe_state）
- 支持 root hub / hub class
- 支持 libusb/winusb/webusb host 驱动

## 4) Charm 映射建议（骨架层次）

建议结构（对齐 Charm 运行期分层）：

- `io/usb/common`
  - 描述符、class 常量与公共类型
- `io/usb/device`
  - device core（描述符表、接口表、class op）
  - class drivers（CDC/MSC/UAC/HID/DFU）
  - extension（BOS/WebUSB/WinUSB）
- `io/usb/host`
  - host core（枚举/pipe/urb）
  - class drivers（MSC/UAC/HID/CDC/HUB）
- `io/usb/driver`
  - DCD/HCD/OTG 适配

## 4.1 Charm 当前落地状态（对照 VSF）

- `io/usb/common`
  - 已完成：描述符结构、DescriptorBuilder、UTF-16/ASCII/LangID 工具
- `io/usb/device`
  - 已完成：EP0 状态机、标准请求、vendor/class 分派、ZLP/长度裁剪
- `io/usb/driver`
  - 已完成：最小 DCD 接口契约（`usb.driver` / `usb.ep0_driver`）
- `io/usb/class`
  - 已完成：CDC/UAC/MSC 类草案 + CDC 收发钩子
- 示例
  - 已完成：`Examples/usb/usb_cdc_minimal`

## 4.2 仅参考 VSF 的“接口形状”

只参考这些内容：
- class op 的职责划分（setup/data/init/fini）
- 描述符构建风格（结构/表/DSL）
- 传输对象模型（EP/缓冲/回调）

不参考这些内容：
- VSF 的宏配置体系
- 平台驱动细节与工程裁剪机制

## 5) 迁移优先级建议

1) Device Core + CDC ACM
   - 最快形成可回归闭环（串口日志/控制）
2) MSC（结合现有 FS/VFS）
3) UAC（对接 audio.sink / audio.source）

Host 侧可后置（复杂度更高）。

## 6) 对 Charm 的直接启示

- 描述符构建可抽象为“描述符 DSL/宏层”
- class driver 统一接口模型（get_desc/request/init/fini）
- 传输对象 = endpoint + buffer + callback/EDA

---

参考来源：
- `Draft/vsf/source/component/usb/vsf_usb.h`
- `Draft/vsf/source/component/usb/device/vsf_usbd.h`
- `Draft/vsf/source/component/usb/host/vsf_usbh.h`
- `Draft/vsf/source/component/usb/common/*`
