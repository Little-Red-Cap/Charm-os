# USB 体系规划（Charm 版本草案）

目标：在不引入过度复杂度的前提下，先实现 **Device 端最小闭环**，再扩展到 MSC/UAC。

## 0) 约束与原则

- **分层清晰**：descriptor/common 与 device/host/driver 分离
- **最小闭环优先**：先 CDC ACM（日志/控制台）
- **Device 优先**：Host 侧复杂度更高，后置
- **接口面向 Class**：Class 驱动只关心 control/data 传输，不关心 DCD 实现

## 1) 目录规划（Charm）

```
Modules/io/usb/
  common/        # 描述符、class 常量、公共类型
  device/        # device core（枚举/控制请求/配置）
  host/          # host core（枚举/pipe/urb）[后置]
  class/         # CDC/MSC/UAC/HID/DFU（device/host 共用）
  driver/        # DCD/HCD/OTG 适配
```

## 2) 设备栈核心对象（建议最小模型）

- `UsbDevice`
  - 设备状态（Default/Addressed/Configured）
  - 控制端点 EP0
  - 配置表/接口表

- `UsbClass`
  - `get_desc()`
  - `setup_request()`
  - `in/out_complete()`
  - `init()/fini()`

- `UsbEndpoint`
  - addr/type/interval/max_pkt
  - transfer buffer + callback

## 3) Phase 规划（Device Only）

### Phase 1：CDC ACM（最小可用）
- 目标：替代 UART，提供日志/控制台输入
- 验收：
  - Windows 可识别为虚拟串口
  - 发送命令触发 shell/EDA 事件

### Phase 2：MSC
- 目标：挂载 FATFS/VFS，形成 U 盘
- 验收：
  - PC 能看到卷
  - 文件写入后可被 VFS 读取

### Phase 3：UAC
- 目标：USB Audio 设备（sink/source）
- 验收：
  - PC 识别为声卡
  - 与 `audio.sink`/`audio.source` 对接

## 4) 对接点（与现有架构）

- `FS`：MSC 直接挂 `fs_vfs` → `FatFsMount`
- `Audio`：UAC → `audio.sink` 或 `audio.source`
- `Shell`：CDC → `shell_stdio` 或新 `usb_stdio`
- `Trace`：USB class driver 输出 trace 到 `trace_core`

## 5) 近期落地建议（可执行）

1. 先建 `usb/common` 的 descriptor 常量与结构体  
2. 建 `usb/device` 的最小状态机（EP0 + setup）  
3. 落地 CDC class（仅 control + bulk IN/OUT）  
4. 预留 class 注册表与 `UsbClass` 接口

## 6) VSF 参考映射

参考文档：`docs/vsf_usb_map.md`

关键借鉴：
- class driver 的 op 表设计  
- 描述符构建宏/DSL  
- 传输对象（EP + buffer + callback）
