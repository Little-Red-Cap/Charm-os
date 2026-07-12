# Charm Bundle 概念草案

本文档定义 Charm 中的 `bundle` 概念，用于解决复杂场景下“能力装配太原子化、太费劲”的问题。

它不是对 `init.graph` 的替代，而是建立在 `init.graph` 之上的工程层抽象。

## 1. 为什么需要 Bundle

当前复杂场景开发中，一个高频痛点是：

- `capability` 很清晰
- `node` 很精确
- `init.graph` 很严格

但开发者经常需要的不是“一个 node”，而是：

- 一组总是一起出现的能力
- 一组总是一起注册的节点
- 一组总是一起启用的 contract
- 一组总是一起验证的依赖

例如：

- `usb storage`
- `audio output`
- `player storage`
- `ui ink runtime`

如果每次都从原子 capability 粒度手拼，复杂项目很快就会出现：

- 样板代码增多
- 场景切换成本上升
- 结构重复但名字不同
- 实验代码容易变成旁路装配

因此需要一个高一层概念：

> **Bundle 是“能力组合的工程表达单元”**

## 2. Bundle 的定义

`bundle` 指：

> 一组面向同一功能目标、可作为整体被装配、启用和验证的能力组合。

它应该包含的不是业务流程本身，而是业务场景所依赖的：

- capability 组合
- node 组合
- contract 组合
- 运行时依赖声明
- 可观测性挂点

## 3. Bundle 不是什么

### 3.1 不是新的全局入口

Bundle 不能绕过：

- `profile`
- `runtime`
- `init.graph`

它只是高层组合表达，不是旁路启动机制。

### 3.2 不是业务脚本

Bundle 不负责：

- 业务状态机
- UI 交互流程
- 应用层控制逻辑

这些应该仍由应用逻辑或 profile 决定。

### 3.3 不是板级事实容器

Bundle 不负责：

- HAL 句柄来源
- 中断向量
- DMA 通道分配
- memory region 物理映射

这些属于 `runtime` 与 `board` 语义层。

## 4. Bundle 在整体架构中的位置

推荐的分层关系：

```text
profile
    -> 选择 bundle
bundle
    -> 组合 capability / node / contract
runtime
    -> 提供板级宿主环境
init.graph
    -> 执行最终装配
```

即：

- `profile` 决定“这次跑什么场景”
- `bundle` 决定“这次需要哪些能力组合”
- `runtime` 决定“这些能力运行在什么宿主环境上”
- `graph` 决定“最终如何严格装配”

## 5. Bundle 应该提供什么接口

一个成熟的 bundle，至少应提供以下四类信息：

### 5.1 依赖声明

回答：

> 它依赖哪些 runtime 能力？

例如：

- 需要 `board.usb_fs`
- 需要 `block.sd0`
- 需要 `system.clock`
- 需要 `dma_safe` buffer class

### 5.2 装配内容

回答：

> 它向 graph 提交哪些节点与 capability？

也就是说，bundle 应能产出：

- node span
- capability 名称
- 默认 contract 绑定

### 5.3 配置面

回答：

> 它有哪些可控参数？

例如：

- 只读 / 可写
- endpoint 配置
- 缓冲区大小
- 调试级别
- 是否启用某个扩展路径

### 5.4 可观测性挂点

回答：

> 出问题时如何统一观察它？

例如：

- counter
- trace point
- 状态快照
- 最近错误码

## 6. Bundle 的最小模型

在概念上，一个 bundle 可以看成由以下几个部分组成：

```text
Bundle
    - name
    - runtime requirements
    - config
    - node producer
    - capability exports
    - observability hooks
```

注意：

- `bundle` 最终仍然向 `graph` 提供 node
- 它只是把“这一组 node”收束成一个正式工程概念

## 7. 第一批建议引入的 Bundle

### 7.1 `usb_storage_bundle`

目标：

- 表达 USB MSC + block device + DCD adapter + storage contract 的整体装配

适用场景：

- `usb_storage`
- `usb_self_msc`
- 后续文件传输 / 固件更新相关设备模式

### 7.2 `audio_output_bundle`

目标：

- 表达 I2S / DMA / 音频缓冲 / 输出时序相关能力组合

适用场景：

- `usb_audio`
- `player_core`
- `audio_bringup`

### 7.3 `player_storage_bundle`

目标：

- 表达存储介质、VFS、扫描、媒体访问相关能力组合

适用场景：

- `player_core`
- `storage_bringup`
- `index_scan`

### 7.4 `ui_ink_bundle`

目标：

- 表达 Ink UI 运行时相关能力组合

适用场景：

- `player_ui_ink`
- `display_bringup`

## 8. Bundle 的设计原则

### 原则 1：Bundle 必须服务于“组合稳定性”

不能为了省几行代码，把临时实验拼法包装成 bundle。

只有当一组能力在多个场景中稳定共同出现时，才值得沉淀为 bundle。

### 原则 2：Bundle 必须保持可拆解

Bundle 不是黑盒。

开发者应该仍能看见：

- 它依赖了哪些 capability
- 它提交了哪些 node
- 它暴露了哪些配置项

否则 bundle 会变成新的隐式全局入口。

### 原则 3：Bundle 必须与 Runtime 解耦

Bundle 可以要求 runtime 提供某些能力，
但不能把 board/HAL 事实直接写死在 bundle 内部。

否则 bundle 会退化成板级脚本。

### 原则 4：Bundle 必须能挂可观测性

复杂开发里，如果 bundle 不能统一挂 trace/counter/state，
它就只是“打包 node”，而不是成熟工程抽象。

### 原则 5：Bundle 必须先从具体场景长出来

Bundle 不应该先做空泛抽象，再去找使用场景。

更好的顺序是：

- 先从 Player / USB / Audio 这些真实复杂场景提炼
- 再形成可复用 bundle

## 9. 对当前仓库的直接建议

结合当前 Player 收敛进度，建议先做：

### 第一步：在文档层正式承认 Bundle 概念

目标：

- 让团队后续统一在同一套语言下讨论“能力组合”

### 第二步：从 `usb_storage_bundle` 开始

原因：

- USB MSC 已经暴露出最明显的装配痛点
- DCD / block / strings / config / ready hook 已经具备 bundle 提炼价值

### 第三步：再提炼 `audio_output_bundle`

原因：

- 它天然涉及 buffer / DMA / runtime / timing
- 很适合作为 resource class 与 observability contract 的试验田

## 10. 当前阶段什么不要做

在 bundle 概念正式落地前，避免：

- 把所有 capability 一股脑包装成巨型 bundle
- 用 bundle 替代 graph
- 在 bundle 中塞进业务状态机
- 在 bundle 中硬编码板级 HAL 细节

否则会很快把 bundle 做成新的混乱来源。

## 11. 一句话总结

如果说：

- `runtime` 解决的是“我运行在什么宿主上”
- `profile` 解决的是“我这次要跑什么场景”

那么：

> `bundle` 解决的是“我需要把哪些能力作为一个稳定整体来装配”

它将成为 Charm 从“系统抽象正确”走向“复杂开发顺手”的关键中间层。
