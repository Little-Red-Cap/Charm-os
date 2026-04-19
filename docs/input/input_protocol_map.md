# 输入协议映射（VSF -> Charm）

目标：把 VSF 的 input 协议事件编码迁移为 Charm 的 RawInputEvent 统一规范，避免 UI/Runtime 之间的语义漂移。

## 1) VSF 事件模型要点

VSF 的通用输入事件结构：

- `vk_input_evt_t`
  - `id`：事件类型编码（低字节为事件类别，高位可带参数）
  - `duration`：前一帧与当前帧的时间间隔（ms）
  - `pre` / `cur`：前值与当前值

VSF 的做法说明：

- 协议层（mouse/keyboard/touch 等）统一使用 `id` 的位段编码
- `cur`/`pre` 通过 union 支持多种宽度

Charm 的启示：

- RawInputEvent 需要带 `id`、`pre`、`cur`、`duration`
- `id` 必须有稳定的位段约定，协议层能无损映射

## 2) 建议的 RawInputEvent 字段（Charm）

- `id`：事件编码（协议层统一）
- `duration_ms`：事件间隔（用于长按/拖拽/速度）
- `pre` / `cur`：基础数值（按协议语义解释）
- `source`（可选）：设备/端口/采样来源

## 3) VSF Mouse 事件编码参考

VSF mouse 事件类型（来自 `vsf_input_mouse.h`）：

- `VSF_INPUT_MOUSE_EVT_MOVE`
- `VSF_INPUT_MOUSE_EVT_BUTTON`
- `VSF_INPUT_MOUSE_EVT_WHEEL`

编码规则（VSF 宏）：

- `id` 低 8bit 为事件类型
- bit 16：绝对/相对（0=absolute, 1=relative）
- button: bit 8..9 = button, bit 12 = is_down
- move/wheel: `cur.valu32 = x | (y << 16)`

Charm 建议：

- 保留 VSF 的位段编码习惯
- `RawInputEvent.id` 按 VSF 规则编码，便于协议层复用

## 4) 建议的 Charm 编码约定（草案）

### 通用约定

- `id & 0xFF`：事件类别
- `id >> 16`：模式/子类型/标志位

### Mouse

- MOVE: `id = MOUSE_MOVE | (abs_rel << 16)`
- BUTTON: `id = MOUSE_BUTTON | (button << 8) | (down << 12) | (abs_rel << 16)`
- WHEEL: `id = MOUSE_WHEEL | (1 << 16)`

`cur.valu32`：`x | (y << 16)`

### Keyboard

- 参考 VSF keyboard 协议：
  - `id` 低 8bit = key event type
  - `cur.valu32` = keycode / modifier

### Touch

- 参考 VSF touchscreen 协议：
  - `id` 低 8bit = touch event type
  - `cur.valu32` = x | (y << 16)
  - 需要时在 `id` 高位编码 finger id

## 5) 迁移建议（落地顺序）

1. RawInputEvent 固定字段与编码规则
2. Protocol 层把 mouse/keyboard/touch 转换到 RawInputEvent
3. UI 层只依赖 ProtocolEvent / Intent，不直接解析 RawInputEvent

## 6) 备注

- VSF `duration` 的定义是 pre/cur 的时间间隔，可直接用于长按/滑动速度判断
- 若未来加入 HID 报告解析，可直接复用 VSF 的 bit-field 解析思路

---

参考来源：
- VSF `source/component/input/vsf_input.h`
- VSF `source/component/input/protocol/vsf_input_mouse.h`
- VSF `source/component/input/protocol/vsf_input_keyboard.h`
- VSF `source/component/input/protocol/vsf_input_touchscreen.h`
- 本仓库历史对照入口：`docs/reference/vsf/README.md`
