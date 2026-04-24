# Charm 三层执行模型草案（System Image / Runtime / User App）

这份文档把 Charm 的 POSIX 方向进一步具体化：

- 哪些东西属于系统镜像层
- 哪些东西属于运行时层
- 哪些东西属于用户程序层
- 三层之间应该暴露什么契约，不应该暴露什么细节

它服务的核心目标只有一个：

> 让应用开发者越来越接近“只写 `main()` 和程序语义”，
> 而把裸机平台必须承担的链接、启动、内存布局知识压回平台层。

## 1. 这份文档与现有文档的关系

这份文档不是用来替代已有 POSIX 文档，而是把它们串成一个统一分层模型。

- 总体原则：`docs/system/posix_subsystem_principles.md`
- POSIX 总览：`docs/system/posix_support_overview.md`
- `spawn` 设计：`docs/system/posix_spawn_minimal_design.md`
- `ProgramImage` 设计：`docs/system/posix_program_image_minimal_design.md`
- ELF 加载设计：`docs/system/posix_program_image_elf_minimal_design.md`
- Linux 兼容任务清单：`docs/system/posix_linux_compat_tasklist.md`

如果说 `posix_subsystem_principles` 回答的是“为什么这样设计”，
那么这份文档回答的是“这三层分别负责什么”。

## 2. 设计目标

Charm 的长期目标不是把 MCU 裸机工程伪装成“没有平台事实”的世界，
而是把平台事实稳定地收口到系统镜像层，使用户程序层尽量只面对 hosted-like 语义。

这意味着：

- 链接脚本不会消失，但它不应该由应用工程直接维护
- 启动文件不会消失，但它不应该成为用户程序的入口知识
- 内存布局不会消失，但它应该是平台包的职责，而不是应用接口的一部分
- 用户程序应该逐步走向“独立构建、独立装载、独立运行单元”

## 3. 三层模型总览

### 3.1 System Image

System Image 是“真正知道板级世界事实”的那一层。

它负责：

- 启动文件与 Reset 路径
- 中断向量表
- 链接脚本与内存布局
- Flash / SRAM / TCM / 外部存储区域规划
- 最终板级 ELF / bin / hex / uf2 等镜像产物
- 板级 bringup、时钟、console、QEMU 启动入口
- Runtime 的装配与初始化顺序

它不负责：

- 为用户程序暴露 POSIX 头文件
- 直接承载用户程序的业务逻辑接口
- 把平台细节泄漏给 `main()`

### 3.2 Runtime

Runtime 是“把裸机平台包装成可执行环境”的那一层。

它负责：

- 文件描述符模型
- 文件 / 管道 / 终端 / 设备的 POSIX 投影
- 进程/程序执行骨架
- ProgramImage / ELF / ModuleX 等程序映像装载与启动
- PATH / argv / envp / errno / wait 等用户态基础语义
- 用户程序生命周期与资源回收

它不负责：

- 维护真实板级链接脚本
- 定义底层 MCU 内存拓扑
- 让每个用户程序都重新扮演裸机镜像工程

### 3.3 User App

User App 是“只关心程序语义”的那一层。

它应该尽量只看到：

- `main(argc, argv, envp)`
- `stdin/stdout/stderr`
- `open/read/write/close`
- `pipe/dup2/isatty`
- `spawn/spawnp/waitpid/kill/getpid`
- PATH / errno / envp / 重定向 等基础行为

它不应该直接知道：

- 中断向量表位置
- 栈顶符号名
- `.data` 搬运与 `.bss` 清零细节
- 板级链接脚本文件路径
- 某块具体板子的 Flash/SRAM 切分方式

## 4. System Image 层契约

这一层的关键词是：**托管平台事实**。

### 4.1 输入

System Image 的典型输入包括：

- 平台 memory map
- startup 文件
- 链接脚本或链接脚本生成输入
- board/QEMU 相关 bringup 代码
- Runtime 模块集合
- 系统级配置开关

### 4.2 输出

System Image 的典型输出包括：

- 板级可启动 ELF
- 板级二进制镜像
- QEMU 可直接加载的镜像
- 与板级平台匹配的调试符号映像

### 4.3 当前仓库里的对应物

当前最典型的样例是：

- `Examples/kernel/posix/qemu/CMakeLists.txt`
- `Examples/kernel/posix/qemu/startup.cpp`
- `Examples/kernel/posix/qemu/ldscript.ld`
- `Examples/kernel/posix/qemu/main.cpp`

这些文件共同构成“板级可启动系统镜像”的工程现实。

### 4.4 对应用层的约束

System Image 层必须做到：

- 应用工程无需直接编辑链接脚本
- 应用工程无需理解启动路径即可参与构建
- 用户程序的接口形状尽量与平台镜像装配解耦

## 5. Runtime 层契约

这一层的关键词是：**把系统能力收敛为程序执行语义**。

### 5.1 当前核心模块映射

当前仓库中，Runtime 层已经初步落到以下模块：

- `Modules/io/posix/posix.fd_table.cppm`
- `Modules/io/posix/posix.file.cppm`
- `Modules/io/posix/posix.pipe.cppm`
- `Modules/io/posix/posix.term.cppm`
- `Modules/io/posix/posix.errno.cppm`
- `Modules/io/posix/posix.env.cppm`
- `Modules/io/posix/posix.proc.cppm`
- `Modules/io/posix/posix.exec_context.cppm`
- `Modules/io/posix/posix.exec_loader.cppm`
- `Modules/io/posix/posix.program_image.cppm`
- `Modules/io/posix/posix.program_image_elf.cppm`
- `Modules/io/posix/posix.program_image_modulex.cppm`
- `Modules/io/posix/posix.api.cppm`

### 5.2 Runtime 必须提供的最小能力

#### A. FD 统一模型

- 文件、管道、终端、后续 socket/devfs 都应挂在同一 fd 体系下
- 资源回收、继承、dup、`isatty` 等语义应在同一模型内成立

#### B. 程序执行闭环

- `spawn`
- `spawnp`
- `waitpid`
- `kill`
- `getpid`
- `_exit(code)` 收束

#### C. 程序映像抽象

- registered image
- `elfmem:`
- file-backed ELF
- 后续可能的 ModuleX image

这些不应各自长出独立执行路径，
而应统一到 `load_image -> start_image -> wait/cleanup` 的骨架上。

#### D. 程序运行参数

- `argc/argv/envp`
- PATH 搜索
- stdio 绑定
- file actions / redirect
- errno 语义收束

### 5.3 Runtime 不应泄漏给 User App 的东西

Runtime 内部可以知道：

- 当前是 same-address-space 模型
- `_exit` 通过 `ExecContext + setjmp/longjmp` 收束
- fd 表是固定容量模板
- 某些能力目前只是 v0 语义

但这些实现细节不应该成为用户程序的接口前提。

换句话说，应用层可以依赖“行为契约”，
不应该依赖“当前实现是怎么绕过去的”。

## 6. User App 层契约

这一层的关键词是：**程序像程序，而不是像板级工程**。

### 6.1 用户程序应暴露什么入口

最小入口 ABI 应保持统一：

```cpp
int main(int argc, char** argv, char** envp);
```

如果后续需要 `app_main()` 等变体，
也应该在 Runtime 或装载器层做统一包装，
而不是让每个应用各自定义启动协议。

### 6.2 用户程序的最小资源语义

用户程序运行时，应至少拥有：

- 自己看到的 fd 表视图
- 自己的 `argv/envp`
- 自己的程序名与 pid 语义
- 可回收的退出状态

在当前 same-address-space 模型下，
这些语义不等价于 MMU 进程隔离，
但对应用层来说，它们应尽可能表现为独立程序边界。

### 6.3 用户程序产物形状

后续应明确区分两类产物：

#### A. System Image 产物

- 用于 QEMU / 板级启动
- 由平台工程统一链接
- 知道真实内存布局

#### B. User App 产物

- 作为独立程序映像参与运行时加载
- 不直接拥有板级链接脚本知识
- 由 Runtime 根据统一 ABI 启动

这类 User App 产物在现阶段可以表现为：

- 注册映像
- `elfmem:` 样本
- 文件路径 ELF

后续再逐步统一为更明确的用户程序交付契约。

## 7. 三层之间的具体接口边界

### 7.1 System Image -> Runtime

System Image 应向 Runtime 提供：

- 已装配好的核心系统能力
- 时钟/console/VFS 等底座能力
- 稳定的 bringup 和初始化顺序
- 可启动的主执行环境

Runtime 不应自行猜测：

- 板级 console 在哪里
- 哪块内存是安全可执行区域
- 平台有没有完成基础 bringup

### 7.2 Runtime -> User App

Runtime 应向 User App 提供：

- 统一入口 ABI
- 统一 fd / stdio / errno / wait 行为
- 统一程序装载与退出约定
- PATH / envp / 文件/管道等最小 POSIX 语义

User App 不应反向要求 Runtime 暴露：

- 板级链接对象细节
- 启动栈布局细节
- 裸机中断/异常入口知识

### 7.3 User App -> System Image

原则上不应存在直接边界。

用户程序应该总是通过 Runtime 与系统交互，
而不是跨过 Runtime 直接依赖板级镜像组织方式。

## 8. 对当前仓库的落地理解

基于当前代码状态，可以把仓库里的 POSIX 路线理解为：

### 已经具备的部分

- Runtime spine 已经成形
- `fd/file/pipe/proc/term/errno/env/api` 的主骨架已存在
- `spawn -> load_image -> start_image` 已经是执行主链
- QEMU 样例已经承担了 System Image 层的验证入口

### 还没有完全做实的部分

- 用户程序“独立构建产物”契约还没有完全固化
- 平台层到应用层的 sysroot/CRT/toolchain 外观还没有建立
- User App 层仍然更多依赖样例和测试模块，而不是稳定交付接口

这意味着下一阶段的重点，不是再补一套新的执行模型，
而是把这三层边界写实、写稳、写成可构建契约。

## 9. 建议的下一批实现落点

如果要从这份文档继续往代码推进，建议顺序如下：

### 9.1 先固化 User App ABI

把下面这些点写成更明确的单独设计或实现契约：

- `main(argc, argv, envp)` 统一入口
- `_exit(code)` 与返回值优先级
- pid / wait status 的对外编码规则
- stdio / redirect / PATH 的最小保证

### 9.2 再补一个最小 user CRT 视角

哪怕当前不真正做完整 sysroot，
也应该先明确“用户程序被启动前，Runtime 负责什么”：

- 参数组织
- 环境组织
- stdio 绑定
- 退出收束

这一步是把“像普通程序一样写 `main()`”真正做实的关键。

### 9.3 再把 User App 产物契约收口

统一描述：

- 什么叫 registered image
- 什么叫 file-backed ELF
- 什么叫未来的 Charm app image
- 它们共享哪些 ABI，不共享哪些装载细节

### 9.4 最后再考虑 toolchain 外观

只有在前面三点稳定后，
再考虑更完整的：

- sysroot
- CRT 对象
- 平台 profile
- 用户程序独立构建入口

否则很容易在接口尚未收敛时，先把外部构建形状固化死。

## 10. 评审时的判断问题

以后评审相关改动时，可以先问 5 个问题：

1. 这个改动属于三层中的哪一层？
2. 它是否把不该暴露的实现细节泄漏给了上层？
3. 它是在加固 Runtime spine，还是在绕过既有 spine？
4. 它是否让 User App 更接近“只写程序语义”，还是又把平台事实推回给应用？
5. 它是否有 QEMU / smoke / 真实样本支撑，而不是仅仅“接口长得更像 Linux”？

如果这 5 个问题里有两个以上答不上来，
那通常说明该改动还没有处在合适的层次上。

## 11. 总结

Charm 的 POSIX 方向，本质上不是“把 MCU 变成 Linux”，
而是：

- 由 System Image 托管平台事实
- 由 Runtime 托管程序执行语义
- 由 User App 只承接 hosted-like 程序接口

这样，链接脚本不会消失，启动代码不会消失，内存布局不会消失；
但这些复杂度会被压到应用工程看不见的位置。

这才是 Charm 真正要做成的“像 PC 一样写程序”的闭环。
