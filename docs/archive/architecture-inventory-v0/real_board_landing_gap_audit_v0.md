# 真实板级落地脱节审计与能力回收梯队 v0

> 状态：archived。本文基于早期 stm32h747-player 目录，所列 app/service 路径与 P0-P4 排期
> 已经过期。保留内容用于追溯真实板如何暴露默认路径和接缝问题。

目标：把 `stm32h747-player` 在真实 bring-up 中暴露出来的问题，收束成 Charm 主仓可以复用的架构审计，而不是停留在“某块板子现在写得有点乱”的经验层。

这份文档不承担 H747 BSP 具体实施说明；它只回答：

- Charm 在真实板级需求下暴露了哪些脱节问题
- 这些问题里哪些是能力缺失，哪些是接缝问题，哪些是默认路径问题
- 后续应该按什么顺序回收，而不是继续让每条板级线各长一套做法

## 1. 问题现象

`stm32h747-player` 当前提供了一个非常有价值的压力测试面，因为它同时具备：

- 多条并行 bring-up 主线：audio / storage / network / display / system coordination
- 真实板级 port/service/app 分层
- Charm 最小 landing：`charm_smoke`
- 共享系统壳：`system_probe`

但也正因为它足够真实，以下脱节现象被完整暴露出来：

### 1.1 控制台路径并行存在

H747 当前同时存在两条输出路径：

- 板级直写路径：`h747::console::*`
- Charm 路径：`ConsoleCaps -> io.uart1 -> io.console0 -> io::Channel`

结果是：

- `charm_smoke` / `system_probe` 这类 app 虽然已经建立了 `io.console0`，但仍然大量直接调用 `h747::console::*`
- app 作者不需要面对“默认应该走哪条路径”这个问题，因为两条路都能工作
- 架构层已经有 `out.channel` / `out.api`，工程层却没有把它们收成默认输出出口

### 1.2 app 自带 shell / 诊断风格持续分化

H747 的多个 probe app 都在自己维护：

- banner
- help 文案
- status 输出格式
- 周期 alive 日志
- shell 命令命名风格

这在 bring-up 初期是正常的，但如果没有收口机制，后果会变成：

- 同类 service 的状态面无法横向比较
- `system_probe` 无法自然消费其它线的状态
- “共享系统层”会退化成另一个 app，而不是协调整个板子的系统壳

### 1.3 board/service/bringup 已经分层，但默认接法还没闭合

H747 侧已经有清晰的分层尝试：

- `board/port`
- `board/services/*`
- `apps/*`
- `charm_landing`

Charm 主仓侧也已经具备对应拼图：

- `platform.board::{ConsoleCaps, BoardCaps}`
- `charm.system.bringup.console::BringupConsole`
- `charm.system.bringup::BringupMinimal`
- `out.channel`
- `out.api`

但当前默认工程路径没有闭合成：

`board facts -> BoardCaps/ConsoleCaps -> bringup -> io.console0 -> out`

于是作者即使知道这些组件存在，也很难确信“现在就该这样接”。

### 1.4 文档红线已经存在，但模板和脚手架还没有强制执行

Charm 当前已经写下了不少对的纪律，例如：

- 输出默认应走 `out.api` / `out.logger`
- 时间源默认应走 `charm.system.clock`
- 初始化默认应走 `init.graph`
- 通道默认应显式经由 `io.registry`

但在真实板级落地中，只有“规则文本”还不够。只要以下任一项缺失，作者就会自然退回手写：

- 现成模板
- 最短可复制样板
- 明确的默认接法
- 哪些场景允许例外的工程说明

## 2. 问题分型

这类脱节不能笼统归因于“Charm 不适合”。当前至少要分成以下 5 类：

### 2.1 能力不存在

定义：仓库里确实没有对应能力，或者现有能力不足以承接真实需求。

这一类在当前问题里不是主因。以控制台链路为例，Charm 已经存在：

- `out.api`
- `out.channel`
- `ConsoleCaps`
- `BringupConsole`
- `BringupMinimal`

因此 H747 没自然用上 `out`，不能直接归因为“Charm 没有打印能力”。

### 2.2 能力存在但不可发现

定义：能力已经存在，但普通落地作者很难知道它是当前推荐入口。

典型表现：

- 组件存在于 `Modules/*`，但没有被路由文档或 board landing 文档明确指出
- 能力的“仓库存在”与“工程默认入口”之间缺少中间层说明
- 作者只能通过翻 examples 或偶然搜索才能知道它

`out.*` 在当前真实板级压力下，就明显暴露了这一类问题。

### 2.3 能力可发现但接缝不顺手

定义：作者知道组件存在，但从真实 board port/service 进入这套能力的接线成本仍然偏高。

典型表现：

- 需要手动桥接多个对象才能形成最小输出路径
- pre-graph / post-graph 的职责边界不清
- 一旦只想先“让板子说话”，手写 UART 比走现成能力更省力

H747 当前的 `h747::console` 就是这种“接缝阻力”的现实补偿物。

### 2.4 已有接线但不是默认路径

定义：仓库里已经有可行接线，但它没有被收束成新 board/app 的自然默认选择。

典型表现：

- `charm_landing` 已把 `io.console0` 建起来，但 app 仍默认直写 `h747::console`
- `BringupMinimal` 已经能承接 `BoardCaps`，但 board 工程仍偏向手写局部 bring-up
- `out.channel` 已存在，但没有成为 board console 的固定下游

这类问题比“能力不存在”更危险，因为它会制造“第二套永远更方便”的长期印象。

### 2.5 文档有纪律，但模板没有强制

定义：规则是对的，但没有进入脚手架、契约、app 模板、service 准入门槛。

典型表现：

- 文档说“推荐”，但 sample / app / board template 没用
- 文档说“默认”，但例外路径没有被命名
- 文档说“共享 service”，但缺少统一的只读状态准入契约

这说明当前问题不是“谁没看文档”，而是“文档尚未变成系统默认行为”。

## 3. 证据索引与归因结论

## 3.1 H747 证据索引

H747 当前提供了以下关键证据面：

- `board/services/console/console.cpp`
  证明板级直写输出路径真实存在，且已被广泛使用
- `board/services/charm_landing/h747_charm_landing.cppm`
  证明 `ConsoleCaps/BoardCaps` 与 `io.uart1/io.console0` 已被桥接出来
- `apps/charm_smoke/main.cpp`
  证明 Charm 已可站上真实 H747，但 early output 与运行期 console 仍并行
- `apps/system_probe/main.cpp`
  证明共享系统壳已出现，但 shell/status 仍主要靠 app 私有格式维护
- 各 probe app
  证明 app 层仍在自带 banner/help/status/alive pattern，而不是自然落到共享系统模板

## 3.2 Charm 主仓证据索引

Charm 主仓已经具备这些关键能力：

- `Modules/io/out/out.api.cppm`
- `Modules/io/out/out.channel.cppm`
- `Modules/system/bringup/system_bringup_console.cppm`
- `Modules/system/bringup/system_bringup.cppm`
- `Modules/platform/platform.board.cppm`

这些能力足以支撑：

- board console 能力描述
- `io.console0` 的建立
- `io::Channel` 到 `out` 的桥接
- 基于 `BoardCaps` 的系统 bringup

## 3.3 归因结论

这轮审计必须把三种结论严格分开：

### A. “Charm 不适合”

只在以下条件成立时才允许使用：

- 现有抽象会系统性破坏实时性/可验证性/板级可控性
- 或者它要求的前提在 MCU/bring-up 环境下长期不成立

当前控制台与 bringup 问题，不满足这一类结论。

### B. “Charm 缺组件”

只在以下条件成立时才允许使用：

- 仓库里确实没有对应能力
- 或现有能力明显无法承接真实场景

当前 H747 暴露的问题里，控制台链路和 bringup 落点也不属于这一类主因。

### C. “Charm 有组件，但没被装配成默认路径”

这是当前主结论。

H747 这条真实板级线证明了：

- 组件已经有了
- 一部分桥也已经有了
- 但它们还没有被收束成新 board/app 作者几乎无需思考就会选中的默认工程路径

因此当前最重要的工作，不是再写第三套组件，而是把现有能力回收到默认落点。

## 4. 公共术语与准入契约

为了后续整改不再反复解释，这里固定 4 个公共术语。

### 4.1 `DefaultConsolePath`

定义：

`BoardCaps/ConsoleCaps -> io.console0 -> out.channel_sink -> out.api/out.logger`

含义：

- 这是 Charm 认可的 board/app 默认控制台路径
- 新 board/app 如无特殊理由，默认应沿这条路径落下
- “能打印出来”不等于“已进入默认路径”

### 4.2 `EarlyConsole`

定义：

允许直接绑定 port/UART 的早期例外路径，仅用于：

- pre-graph
- fault / fail-fast
- 极早期生存证据

含义：

- `EarlyConsole` 是合法例外，不是坏味道本身
- 问题在于例外路径不能无限外溢到运行期 app 逻辑

### 4.3 `ServiceSnapshotContract`

定义：

任何 board service 若要进入共享协调层，必须先提供：

- 无副作用
- 可重复读取
- 可串口打印
- 不要求进入 runtime 重服务

的只读 `snapshot/status` 接口。

含义：

- `system_probe` 这类协调 app 只能消费状态，不应接管各主线 runtime
- 没有 snapshot 契约的 service，不得直接进入共享系统层

### 4.4 `EvidenceRig`

定义：

像 H747 这样用于暴露 Charm 真实落地问题的板级实验台。

含义：

- 它提供高价值证据
- 但它的当前实现，不自动等于 Charm 的通约架构契约
- 架构结论应从 `EvidenceRig` 抽取，不应原样复制所有局部实现

## 5. 能力回收梯队（P0-P4）

## P0 术语与判定法统一

先统一：

- `DefaultConsolePath`
- `EarlyConsole`
- `ServiceSnapshotContract`
- `EvidenceRig`

没有这一层，后续每条子线都会再次争论“现在这个直写 UART 到底算不算合理”。

## P1 输出链回收

首刀固定落在输出链，而不是 USB Audio。

目标：

- 把 board console 的默认落点明确收成
  `io.console0 -> out.channel_sink -> out.api/out.logger`
- 把直写 `h747::console` 降级为 `EarlyConsole`

原因：

- 输出链是所有主线都会碰到的最低公共面
- 它最容易暴露“能力存在但不是默认路径”的问题
- 它一旦收好，后续 shell/status/trace/board service 才有统一基础

## P2 bringup / shell / status 回收

目标：

- 把 `system_probe`、`board_smoke`、`charm_smoke` 这类 app 的输出模式收束成共享 pattern
- 统一 banner/help/status/alive 风格
- 让共享 shell 消费 `ServiceSnapshotContract`，而不是每个 app 私带一套状态面

这一层仍然不要求把所有 app 合并成一个大 app；它只要求协调层有可复制的统一样板。

## P3 board landing 回收

目标：

- 让 `BoardCaps / ConsoleCaps / BringupConsole / BringupMinimal`
  成为 board service 进入系统的标准入口
- 把“板级事实如何进入 Charm”收成一条固定接法

这一层完成后，新 board 的作者不应该再从 `main.cpp + UART printf + 手写 status` 起步，而应先从板级能力缝起步。

## P4 子系统 adoption queue

目标：

- Audio / Storage / Net / Display 后续统一按同一准入模板接入
- 不再允许每条主线重新发明自己的 app/service/console/status 组合

顺序上，`storage/block` 仍然比 `audio runtime` 更适合作为 first-class capability 进入系统图，因为它噪音更低、边界更清晰。

## 6. 当前阶段结论

当前最重要的结论不是“Charm 不行”，而是：

**Charm 已经具备相当多正确的能力，但它们还没有被组织成真实板级落地时的默认工程路径。**

H747 的价值正在于：

- 它不是一个失败样例
- 它是一个把缺失的默认接法、能力发现性、board landing 接缝、共享系统层准入问题完整暴露出来的 `EvidenceRig`

因此第一阶段最值得做的，不是继续堆新组件，而是把现有能力收回到默认落点。
