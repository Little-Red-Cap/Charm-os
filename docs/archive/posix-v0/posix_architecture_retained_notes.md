# POSIX 早期架构取舍保留笔记

> status: `archived`
>
> scope: POSIX v0 两份分层草案中仍可复用的设计取舍，不说明当前能力

现行源码边界与验证入口见 [`posix_support_overview.md`](../../system/posix_support_overview.md)。本文不
复制模块列表、API 完成度或 runner 命令；这些事实必须由 source、CMake 和当次运行重新确认。

## 兼容面不是 Core

POSIX 在 Charm 中是可选兼容执行面。它可以把内部行为翻译为 fd、errno、argv/envp、stdio 和进程
生命周期语义，但不能要求 kernel、runtime 或 Capability Core 按 Linux 头文件形状设计。

保留的拒绝理由：

- 不用全局 `#define open ...` 一类宏重映射作为主要兼容机制；它把 ABI、include 顺序、实现身份和
  feature gate 混在一起，并扩大命名污染与调试成本。
- 不用不断增长的 feature 宏矩阵代替模块与 target 分层；开关应选择实现，不应成为理解系统的前提。
- 不为了尚未支持的 `fork`、完整 signal、动态链接或地址空间隔离，提前污染当前 same-address-space
  模型。

薄 C header 或 wrapper 可以存在，但它们只投影兼容行为，不拥有底层领域语义。

## 三层责任

早期讨论使用三层模型区分不可消除的平台事实与程序语义。

### System Image

System Image 属于项目/平台装配，拥有 startup、vector table、linker script、memory map、board/QEMU
bring-up 和最终可启动镜像。它向 runtime 提供已装配的执行环境，不把物理内存与启动细节泄漏给
用户程序。

### Runtime

Runtime 拥有 fd、stdio、errno、argv/envp、PATH、程序装载、退出和资源回收等兼容执行语义。它可以
知道当前没有地址空间隔离或使用固定容量表，但这些实现事实不应成为 User App 的调用前提。

### User App

User App 只依赖程序入口和兼容行为，不直接依赖板级链接脚本、Reset path、向量表或物理内存布局。
User App 到 System Image 不应建立旁路；平台交互必须经过 Runtime 提供的边界。

三层名称是分析工具，不是自动获准的 Charm Core 词汇。

## ABI Spine 先于 Surface

兼容层应先稳定一条能运行真实程序的主干，再按阻塞点增加接口。早期主干关注：

- 程序入口、argv/envp 与退出状态；
- stdin/stdout/stderr 与统一 fd ownership；
- open/read/write/close、pipe/dup2/isatty；
- spawn/wait/getpid 与最小生命周期；
- PATH 和集中 errno 映射。

`select/poll`、socket、devfs、termios 或更宽 stat 矩阵只有在真实消费者出现后才进入设计。头文件符号
数量、wrapper 数量或“看起来像 Linux”都不能证明兼容行为成立。

## Image 与执行主链

registered image、memory ELF、file-backed ELF 或其它 image format 可以使用不同 loader，但不应各自
复制进程生命周期。它们应汇入同一类查找、装载、启动、等待、退出和清理骨架。

System Image 与 User App artifact 必须区分：前者知道平台启动与内存事实；后者只承载受 Runtime 约束
的程序映像。独立 App toolchain、sysroot 或 CRT 外观应在入口 ABI、装载和退出语义稳定后再冻结，避免
构建形状抢先定义运行时契约。

## 证据要求

POSIX 行为应由最小真实程序、host smoke、QEMU 或目标环境运行固定。测试需要覆盖行为和失败语义，
不能只检查 API 存在或头文件编译。QEMU 能证明对应机器模型中的执行链，不自动证明真实板、完整 Linux
兼容或产品级认证。

第三方程序保持独立 artifact 可能改善工程和许可证边界，但这不是自动成立的法律结论。

## 重新推进时的问题

- 新行为挂到现有 fd/image/process 主链，还是建立了旁路？
- 失败和 ownership 在兼容层集中翻译，还是散落到各 backend？
- 该语义来自真实程序阻塞点，还是来自接口清单？
- 是否把 platform fact 或当前 same-address-space 实现泄漏给 User App？
- 证据证明的是 host、QEMU、真实板中的哪一个环境？

旧文档中的模块路径、完成状态、阶段排期和“下一批实现”已经删除；需要追溯当时快照时使用 Git 历史。
