# Telegram Desktop 对 Charm 的机制启发草案

## 定位

本文不是 Telegram Desktop 的源码导读，也不是要求 Charm 复制 Qt 桌面客户端的实现方式。

本文记录一个更重要的判断：

> 真实大体量产品长期承压后，能活下来的往往不是漂亮目录结构，而是少数稳定机制。

Telegram Desktop 值得借鉴的地方在于，它把用户态产品复杂性压进了若干明确归属：

- 状态变化进入 RPL
- 执行切换进入 CRL
- 视觉常量进入 style
- 协议事实进入 TL schema
- 平台差异进入 platform
- 持久化演进进入 storage law
- 协作规则进入 AGENTS.md / REVIEW.md

Charm 不应该照搬这些机制，而应该把它们翻译成嵌入式、零动态、可裁剪、可验证的版本。

## 事实校准

本文参考的是 Telegram Desktop 主仓库及其官方源码组织：

- `.gitmodules` 中，`Telegram/lib_crl`、`Telegram/lib_rpl`、`Telegram/lib_base`、`Telegram/lib_ui`、`Telegram/lib_tl`、`Telegram/lib_storage`、`Telegram/lib_webrtc`、`Telegram/lib_webview`、`Telegram/lib_translate` 等作为 submodule 引入。
- `Telegram/CMakeLists.txt` 中，主目标通过 `add_subdirectory(lib_rpl)`、`add_subdirectory(lib_crl)`、`add_subdirectory(lib_base)`、`add_subdirectory(lib_ui)` 等方式组装内部库。
- `AGENTS.md` 明确描述了 RPL、style、localization、TL API、local storage serialization、构建要求和代码风格。

这些事实说明，Telegram Desktop 是一个产品仓库，但它把可复用机制独立成较稳定的内部平台库。

## 对 Charm 的核心启发

### 1. 机制分层比目录分层更重要

Telegram Desktop 的 `lib_*` 目录重要，但更重要的是每个库背后的机制归属。

对 Charm 来说，这意味着：

- `Charm-os` 保持核心语义纯度
- `Charm-out`、`Charm-vivid`、`Charm-posix`、`Charm-usb` 等机制成熟后可以独立
- `BareWorkspace`、`Player`、客户产品仓库作为 superproject，负责 pin 住平台库、SDK、BSP 与产品代码

推荐的仓库治理原则：

> 核心仓库保持纯净，产品仓库承接现实复杂度，中间稳定机制再拆成 package 或 submodule。

这与 Charm 当前的方向相容：平台要纯，产品要活，superproject 可以更现实。

### 2. RPL 最值得学的是生命周期归属

RPL 的管道形式容易吸引注意：

```cpp
producer | map(...) | filter(...) | on_next(..., lifetime)
```

但更值得 Charm 借鉴的是订阅关系必须有明确生命周期所有者。

桌面端可以使用动态分配和容器管理订阅。Charm 更适合做一个小得多的版本：

```cpp
template<std::size_t MaxSubscriptions>
class static_lifetime {
public:
    bool attach(cancel_token token) noexcept;
    void cancel_all() noexcept;
};
```

目标不是做嵌入式 RxCpp，而是得到一组 Foundation 级小机制：

- `delegate`
- `signal`
- `deferred_signal`
- `producer/value`
- `lifetime`

约束应该从一开始写清楚：

- 无堆分配
- 固定订阅槽
- 显式 lifetime domain
- operator 可裁剪
- 订阅数量可静态分析

### 3. UI 应订阅语义源，而不是主动刷新世界

Telegram Desktop 的 localization 很有启发：传 `tr::now` 得到当前文本，不传则得到 reactive producer，语言变化时 UI 可以自动更新。

Charm 可以把这个思想推广为 UI 层状态派生语言：

- `language` 到 text producer
- `theme` 到 color/style producer
- `density/scale` 到 metric producer
- `storage state` 到 availability producer
- `audio state` 到 playback producer
- `power state` 到 low-power producer
- `capability` 到 ready/degraded/failed producer

在 Player 中，理想形态类似：

```cpp
auto cover_palette = media.cover()
                   | async_quantize()
                   | map(to_theme_tokens);

auto play_enabled = combine(audio.ready(), storage.ready(), player.has_track());

bind(title_label.text(), player.track_title(), title_label.lifetime());
bind(play_button.enabled(), play_enabled, play_button.lifetime());
bind(app_theme.tokens(), cover_palette, app_theme.lifetime());
```

这里的关键不是炫技，而是减少复杂 UI 中散落的手动同步逻辑。

边界也必须明确：

- `producer/value` 适合 UI、配置、主题、语言、capability 投影
- `producer/value` 不应该替代 ISR、audio graph、kernel scheduling 或底层 driver 事件流
- `signal`、`deferred_signal`、`event`、`producer/value` 应保持不同语义

### 4. CRL 的启发是执行域，而不是主线程工具

CRL 给 Telegram Desktop 提供跨平台调度抽象，例如 async、queue、on_main、time 等能力。

Charm 可以把这个思想升级成 execution domain：

```text
charm.exec
  post(domain, fn)
  post_isr(domain, event)
  defer_to<ui_domain>()
  defer_to<audio_domain>()
  defer_to<kernel_domain>()
  now()
  timer_after()
```

后端可以映射到：

- PC SDL backend
- Windows thread/event backend
- bare-metal superloop backend
- RTOS task backend
- STM32 IRQ + SWI backend
- Cortex-A timer/IRQ backend

Charm 比桌面调度抽象更进一步的地方，是 execution domain 应该携带语义约束：

- 是否允许阻塞
- 是否允许分配
- 是否允许访问 UI
- 是否允许从 ISR 调用
- 是否跨托管时间边界

### 5. Style 系统最该学的是尺寸立法

Telegram Desktop 的 `.style` 系统最重要的启发不是文本格式，而是规则：

> 尺寸、边距、间距、坐标等视觉常量不应该散落在代码里。

Vivid 现在已经有 SceneBuilder、style patch、token、dirty rect、command buffer。继续走向复杂产品时，需要建立更正式的 style token / metric law。

Charm 更适合使用 constexpr 风格的 token：

```cpp
export constexpr auto player_style = style_sheet{
    token<"topbar.height"> = px<48>,
    token<"card.radius"> = px<12>,
    token<"list.gap"> = px<8>,
    token<"cover.size"> = dp<96>,
};
```

目标形态：

```text
style token -> layout token -> render token -> compiled style table
```

这会帮助 Vivid / Ink 共享设计语义，同时允许不同渲染能力降级。

### 6. TL 的启发是协议必须有唯一源事实

Telegram Desktop 的 MTProto / TL 体系说明，稳定协议不应该只散落成 C++ 类。TL schema 是协议事实，上层 C++ 类型和调用方式是它的投影。

Charm 已经在多个方向走向相似模型：

- USB spec 到 model 到 plan 到 runtime
- init node 到 plan 到 capability
- POSIX surface 到 syscall/fd/process model
- network request/service/typed facade
- system compiler vocabulary

Charm 可以把这些都纳入同一条方法论：

```text
spec -> model -> validation -> plan -> runtime
```

可能的输入形式包括：

- USB descriptor schema
- device capability schema
- init plan schema
- resource budget schema
- fault semantics schema
- IPC/message schema
- persistent storage schema

工具路线可以保持开放：

- 外部 DSL 到 codegen
- constexpr C++ spec 到 generated tables/types
- 未来 C++ reflection 接管部分生成（已有 probe 验证 `<meta>`、最小 RTE kind/role、spec 字段形状发现、reflected spec -> `ContextView` / evidence 投影、reflected spec 参与 profile resolution 编译期门禁，以及 reflected profile 贯穿 Charm Spine projection/evidence 主链）

原则只有一个：

> 协议和配置必须有唯一源事实，运行时代码只是它的投影。

### 7. Platform 目录说明差异只能隔离，不能消灭

Telegram Desktop 把 Windows、macOS、Linux 的平台差异集中在 platform 相关目录，而不是让平台 ifdef 漫游全仓。

Charm 面对的差异更硬：

- STM32 HAL / LL
- GD32 / CH32 / NXP / RP2040
- bare-metal / FreeRTOS / 自研 kernel
- PC SDL
- Cortex-A bare-metal
- DMA / cache / MPU / IRQ / linker / startup 差异

推荐原则：

> 平台抽象的目标不是统一实现，而是统一解释。

上层看 capability：

```text
capability<time_source>
capability<irq_controller>
capability<audio_device>
capability<usb_device_controller>
capability<display_surface>
```

底层保留真实差异，并把差异集中在被允许的位置。

### 8. AGENTS.md 应成为架构执法入口

Telegram Desktop 的 `AGENTS.md` 把构建路径、平台要求、文本格式、style、localization、RPL、TL API、local storage serialization 和注释规则写给维护者与代码代理。

Charm 的根 `AGENTS.md` 已经承担第一跳入口职责。下一步可以把它扩展成更明确的架构宪法，并允许子目录拥有局部规则：

- `Modules/ui/vivid/AGENTS.md`
- `Modules/system/AGENTS.md`
- `Examples/project/player/AGENTS.md`

建议逐步写入的红线：

- Foundation 禁止依赖 Runtime / Domain
- Runtime 禁止反向依赖 Domain
- Sink 禁止知道 format/logger/ansi
- ISR 中禁止运行 Graph
- audio graph 禁止阻塞、分配、加锁
- UI style 禁止硬编码尺寸
- POSIX v0 closed 后禁止随意扩 surface
- system compiler 的 Node / Plan / Profile / Binding 词义
- 默认 no exception / no RTTI / no heap

这不是文档洁癖，而是 AI、脚手架和长期协作需要的操作边界。

### 9. Storage law 应尽早建立

Telegram Desktop 的 local storage 规则很产品化，但对嵌入式非常重要：顺序二进制序列化只能 append，新字段读取要检查流尾并提供默认值，简单 flags/value 更适合 KV prefs。

Charm 未来会面对：

- NVM 配置
- 校准数据
- 用户设置
- 设备身份
- 配网信息
- 音频参数
- UI theme cache
- boot state
- fault log
- 升级状态

建议形成 `charm.storage.law`：

- persistent object 必须带 schema version
- 字段只能 append，不允许重排
- 可选字段必须有默认值
- 二进制区块必须有 magic/version/length/crc
- 复杂结构优先 TLV / KV
- 升级路径必须可测试

### 10. 注释规则应解释原因，不解释下一行

Telegram Desktop 倾向不写解释下一行的注释，只允许复杂算法长注释。Charm 不应机械照抄。

嵌入式中有些东西必须解释：

- 寄存器时序
- cache / DMA coherency
- memory barrier
- 启动阶段限制
- 链接脚本意图
- ISR / task 边界
- 硬件 errata
- 协议异常情况

Charm 更适合的规则是：

- 普通业务逻辑靠命名与类型自解释
- 架构边界写清禁止事项
- 硬件相关写清手册依据、errata 或时序原因
- 复杂算法写清不变量与失败条件
- 临时妥协必须写 TODO 和退出条件

## 优先级建议

### P0：先立法

- 扩展根 `AGENTS.md` 的架构红线
- 为 Vivid / system / Player 增加局部 `AGENTS.md`
- 写入 style、ISR、audio、storage、dependency 的硬约束

### P1：补最小机制

- zero-alloc `lifetime`
- `producer/value`
- `derived state`
- `execution domain`
- `style token / metric law`

这一步只做最小可用版，不追求完整 reactive 框架。

### P2：先在 Player 验证

- theme/language/density/capability 先成为 Player 可消费的语义源
- Home / Library / Now Playing 先吃同一套 style token 和 runtime roles
- 跑顺后再上提到 Vivid

### P3：推进 schema 与 superproject

- storage law
- init/capability/resource budget schema
- BareWorkspace / product superproject pin versions

这些收益大，但需要前面的语义边界先稳定。

## 结论

Telegram Desktop 表面上值得学的是 RPL、CRL、style、TL、platform、storage 和 AGENTS.md。

更深层值得学的是：

> 一个长期真实产品，必须把变化压进少数稳定机制。

Charm 的翻译版本应该是：

```text
RPL      -> zero-alloc producer / lifetime / derived state
CRL      -> execution domain / hosted-time boundary
.style   -> constexpr style token / resource-aware UI law
TL       -> system compiler schema / typed plan
platform -> capability binding / platform truth quarantine
storage  -> NVM schema evolution law
AGENTS   -> AI-facing architecture constitution
```

这份草案的后续价值，不在于证明 Telegram Desktop 多优秀，而在于帮助 Charm 判断：

> 当系统被产品、平台、历史、状态和协议长期压迫时，哪些机制值得提前成为法律。
