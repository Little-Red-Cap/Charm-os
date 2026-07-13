# POSIX 早期架构取舍保留笔记

> `status`: `archived`

现行源码与验证入口见 [`posix_support_overview.md`](../../system/posix_support_overview.md)。本文只保留
POSIX 兼容面、执行分层和 image 生命周期取舍，不说明当前 API 完成度。

## Compatibility Boundary

POSIX 是可选兼容执行面，把内部行为投影为 fd、errno、argv/envp、stdio 和 process lifecycle；它不要求
kernel/runtime 按 Linux header 形状设计。避免：

- 用全局宏重映射 `open` 等 API，混合 ABI、include order、implementation identity 与 feature gate；
- 用不断增长的 feature 宏矩阵替代 module/target 分层；
- 为未支持的 `fork`、完整 signal、dynamic linking 或 isolation 提前污染 same-address-space 模型。

薄 C header/wrapper 可以投影兼容行为，但不拥有底层领域语义。

## 三层责任

| 层 | 责任 |
|---|---|
| System Image | startup、vector、linker、memory map、board/QEMU bring-up 与 bootable artifact |
| Runtime | fd/stdio/errno、argv/envp、PATH、program load、exit 与 resource reclamation |
| User App | 只依赖 entry ABI 和兼容行为，不直接依赖物理内存、reset path 或 board linker |

App 到 System Image 不建立旁路；平台交互经过 Runtime。三层只是分析边界，不是 Core 词汇。

## ABI Spine 与 Image

先稳定真实程序所需主干：entry/argv/envp/exit、stdio 与 fd ownership、open/read/write/close、pipe/dup2、
spawn/wait/getpid、PATH 和集中 errno mapping。`select/poll`、socket、devfs、termios 或更宽 stat 只由真实
consumer 阻塞点驱动；符号和 wrapper 数量不证明兼容性。

Registered image、memory/file ELF 或其它格式可以有不同 loader，但必须汇入同一 resolve、load、start、
wait、exit 和 cleanup 生命周期。System Image 知道平台事实，User App artifact 只承载 Runtime 约束的程序；
toolchain/sysroot/CRT 外观不能先于 entry、load 和 exit 语义冻结。

## Evidence

行为由最小真实程序、Host smoke、QEMU 或目标环境运行固定，并覆盖失败语义。QEMU 不证明真实板、完整
Linux 兼容或产品认证。新增行为需要回答：是否进入现有 fd/image/process 主链、ownership/error 是否集中、
是否泄漏 platform fact，以及证据属于 Host、QEMU 还是真实板。
