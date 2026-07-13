# USB MSC Trace 解释边界

## 文档状态

- `status`: `supporting`
- `scope`: `usb.class_msc` 内部 trace 的解释规则
- `source`: [`usb.msc.cppm`](../../Modules/io/usb/class/usb.msc.cppm)

`MscTraceEvent` 只服务 native fixture 与诊断，不是 USB wire format、稳定 ABI 或产品 telemetry。event
kind、字段和写入位置以源码为准，本文不复制枚举或字段清单。

## 解释规则

- event 没有 presence mask；未用于当前 kind 的字段保持默认值，不能按统一 schema 解读。
- `flag` 是复用字段：CBW、clear-stall 和 CSW 的含义不同，必须先按 kind 解释。
- command-start event 的 transfer length 来自主机 CBW；data-start event 表示设备实际准备处理的长度。
- READ CAPACITY 的 LBA 是最后可访问块，block count 是总块数，二者不能互换。
- CSW ready 表示响应已形成，CSW sent 表示 data path 已取出；ready 不等于主机已接收。
- wait-CSW 只表示 class 等待 clear-stall，不能证明 host 会完成恢复。
- sense-set 记录失败来源，不代表主机已执行 REQUEST SENSE。

## 记录限制

- trace 是固定容量；追加失败当前没有 overflow counter，因此缺少尾部事件不能自动解释为状态机未执行。
- event 没有 timestamp、全局 sequence、endpoint identity 或跨 reset correlation。
- `clear_trace()` 只清记录，不重置 BOT 状态。
- trace 适合单个 fixture 内按 kind 断言，不适合作为长期日志协议。

当前场景与断言由
[`usb_msc_boardlog_import_smoke`](../../Examples/usb/usb_msc_boardlog_import_smoke/main.cpp) 及 fixtures
维护。replay 通过不证明真实控制器时序、并发、DMA、cache 或主机兼容性。
